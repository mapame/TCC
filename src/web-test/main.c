#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bsd/string.h>
#include <math.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <microhttpd.h>

#include "cJSON.h"

#include "common.h"
#include "logger.h"
#include "communication.h"

#define WEB_DEFAULT_PORT 8081

#define POWER_DATA_BUFFER_SIZE 200


typedef struct power_data_s {
	time_t timestamp;
	double v[3];
	double i[3];
	double p[3];
	double s[3];
	double q[3];
	double pf[3];
} power_data_t;


static volatile int terminate = 0;

pthread_mutex_t power_data_buffer_mutex;
power_data_t power_data_buffer[POWER_DATA_BUFFER_SIZE];
int power_data_buffer_pos, power_data_buffer_count;

void sigint_handler(int signum) {
	terminate = 1;
}

static enum MHD_Result ahc(void * cls, struct MHD_Connection * connection, const char * url, const char * method, const char * version, const char * upload_data, size_t * upload_data_size, void ** ptr) {
	static int dummy;
	const char *aux;
	time_t req_timestamp = 0;
	int power_data_aux_pos;
	cJSON *json_power_data;
	cJSON *json_power_data_array;
	cJSON *json_response;
	power_data_t power_data_aux;
	char *text_response;
	struct MHD_Response * response;
	int ret;
	
	if (0 != strcmp(method, "GET"))
		return MHD_NO;
	
	if (&dummy != *ptr) {
		/* The first time only the headers are valid, do not respond in the first round... */
		*ptr = &dummy;
		return MHD_YES;
	}
	
	if (0 != *upload_data_size)
		return MHD_NO; /* upload data in a GET!? */
	
	*ptr = NULL; /* clear context pointer */
	
	aux = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "startTime");
	
	if(aux)
		sscanf(aux, "%ld", &req_timestamp);
	
	json_response = cJSON_CreateObject();
	json_power_data_array = cJSON_CreateArray();
	cJSON_AddItemToObject(json_response, "power_data", json_power_data_array);
	
	memset(&power_data_aux, 0, sizeof(power_data_t));
	
	pthread_mutex_lock(&power_data_buffer_mutex);
	
	for(int i = 1; i <= power_data_buffer_count; i++) {
		power_data_aux_pos = power_data_buffer_pos - i;
		power_data_aux_pos += (power_data_aux_pos < 0) ? POWER_DATA_BUFFER_SIZE : 0;
		
		memcpy(&power_data_aux, &power_data_buffer[power_data_aux_pos], sizeof(power_data_t));
		
		if(power_data_aux.timestamp <= req_timestamp)
			break;
		
		json_power_data = cJSON_CreateObject();
		cJSON_AddItemToArray(json_power_data_array, json_power_data);
		
		cJSON_AddNumberToObject(json_power_data, "timestamp", power_data_aux.timestamp);
		
		cJSON_AddItemToObject(json_power_data, "v", cJSON_CreateDoubleArray(power_data_aux.v, 2));
		cJSON_AddItemToObject(json_power_data, "i", cJSON_CreateDoubleArray(power_data_aux.i, 2));
		cJSON_AddItemToObject(json_power_data, "p", cJSON_CreateDoubleArray(power_data_aux.p, 2));
		cJSON_AddItemToObject(json_power_data, "s", cJSON_CreateDoubleArray(power_data_aux.s, 2));
		cJSON_AddItemToObject(json_power_data, "q", cJSON_CreateDoubleArray(power_data_aux.q, 2));
		cJSON_AddItemToObject(json_power_data, "pf", cJSON_CreateDoubleArray(power_data_aux.pf, 2));
	}
	
	pthread_mutex_unlock(&power_data_buffer_mutex);
	
	text_response = cJSON_Print(json_response);
	cJSON_Delete(json_response);
	
	response = MHD_create_response_from_buffer (strlen(text_response), (void*) text_response, MHD_RESPMEM_MUST_FREE);
	
	MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, "application/json");
	MHD_add_response_header(response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN, "*");
	
	ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
	
	MHD_destroy_response(response);
	
	return ret;
}

static void print_usage(const char *filename)
{
	fprintf(stderr, "Usage: %s [-r] [-p http_port] mac_key\n", filename);
	fprintf(stderr, "  -r Reuse address and port\n");
	fprintf(stderr, "  -p Port to start the HTTP server\n");
}

int main(int argc, char **argv) {
	struct sigaction sa, old_sa;
	int opt;
	int port = WEB_DEFAULT_PORT;
	int reuse_addr = 0;
	char mac_password[PARAM_STR_SIZE];
	
	int command_result;
	char aux[200];
	char received_parameters[PARAM_MAX_QTY][PARAM_STR_SIZE];
	
	int main_socket;
	comm_client_ctx client_ctx;
	
	struct MHD_Daemon *d;
	
	while ((opt = getopt(argc, argv, "rp:")) != -1) {
		switch (opt) {
			case 'r':
				reuse_addr = 1;
				break;
			case 'p':
				port = atoi(optarg);
				break;
			default:
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
		}
	}
	
	if(optind + 1 > argc) {
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	
	strlcpy(mac_password, argv[optind], PARAM_STR_SIZE);
	
	sa.sa_handler = sigint_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, &old_sa);
	
	power_data_buffer_pos = 0;
	power_data_buffer_count = 0;
	
	logger_set_level(LOGLEVEL_INFO);
	
	pthread_mutex_init(&power_data_buffer_mutex, NULL);
	
	d = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION | MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_ERROR_LOG, port, NULL, NULL, &ahc, NULL, MHD_OPTION_END);
	
	main_socket = comm_create_main_socket(reuse_addr);
	
	if(main_socket < 0) {
		LOG_ERROR("Failed to create/setup socket.");
		return -1;
	}
	
	LOG_INFO("Waiting for device connection on port %d.", COMM_SERVER_PORT);
	
	while(!terminate) {
		power_data_t power_data_aux;
		int conversion_result;
		int qty;
		
		if(comm_accept_client(main_socket, &client_ctx, mac_password) < 0) {
			LOG_ERROR("Failed to accept new client connection.");
			continue;
		}
		
		LOG_INFO("Received connection from %s", inet_ntoa(client_ctx.address.sin_addr));
		LOG_INFO("Device firmware version: %s", client_ctx.version);
		
		while(!terminate) {
			if((command_result = send_comand_and_receive_response(&client_ctx, OP_QUERY_STATUS, "A\t", received_parameters, 4))) {
				LOG_ERROR("Error sending OP_QUERY_STATUS command. (%s)", get_comm_status_text(command_result));
				close(client_ctx.socket_fd);
				break;
			}
			
			if(*received_parameters[0] == '0') {
				if((command_result = send_comand_and_receive_response(&client_ctx, OP_SAMPLING_START, NULL, NULL, 0))) {
					LOG_ERROR("Error sending OP_SAMPLING_START command. (%s)", get_comm_status_text(command_result));
					close(client_ctx.socket_fd);
				}
			}
			
			sscanf(received_parameters[3], "%u", &qty);
			
			if(qty == 0)
				continue;
			
			sprintf(aux, "P\t%u\t", qty);
			
			if((command_result = send_command(&client_ctx, OP_GET_DATA, aux))) {
				LOG_ERROR("Error sending OP_GET_DATA command. (%s)", get_comm_status_text(command_result));
				close(client_ctx.socket_fd);
				break;
			}
			
			for(int i = 0; i < qty; i++) {
				if((command_result = receive_response(&client_ctx, OP_GET_DATA, NULL, received_parameters, 12))) {
					LOG_ERROR("Error receiving OP_GET_DATA response. (%s)", get_comm_status_text(command_result));
					close(client_ctx.socket_fd);
					break;
				}
				memset(&power_data_aux, 0, sizeof(power_data_t));
				
				conversion_result = 0;
				
				conversion_result += sscanf(received_parameters[0], "%lu", &power_data_aux.timestamp);
				
				conversion_result += sscanf(received_parameters[3], "%lf", &power_data_aux.v[0]);
				conversion_result += sscanf(received_parameters[4], "%lf", &power_data_aux.v[1]);
				conversion_result += sscanf(received_parameters[5], "%lf", &power_data_aux.v[2]);
				
				conversion_result += sscanf(received_parameters[6], "%lf", &power_data_aux.i[0]);
				conversion_result += sscanf(received_parameters[7], "%lf", &power_data_aux.i[1]);
				conversion_result += sscanf(received_parameters[8], "%lf", &power_data_aux.i[2]);
				
				conversion_result += sscanf(received_parameters[9], "%lf", &power_data_aux.p[0]);
				conversion_result += sscanf(received_parameters[10], "%lf", &power_data_aux.p[1]);
				conversion_result += sscanf(received_parameters[11], "%lf", &power_data_aux.p[2]);
				
				if(conversion_result == 10) {
					if(power_data_buffer_count && power_data_aux.timestamp <= power_data_buffer[(power_data_buffer_pos) ? (power_data_buffer_pos - 1) : (POWER_DATA_BUFFER_SIZE - 1)].timestamp)
						continue;
					
					power_data_aux.s[0] = power_data_aux.v[0] * power_data_aux.i[0];
					power_data_aux.s[1] = power_data_aux.v[1] * power_data_aux.i[1];
					
					power_data_aux.q[0] = sqrtf(powf(power_data_aux.s[0], 2) - powf(power_data_aux.p[0], 2));
					power_data_aux.q[1] = sqrtf(powf(power_data_aux.s[1], 2) - powf(power_data_aux.p[1], 2));
					
					power_data_aux.pf[0] = power_data_aux.p[0] / power_data_aux.s[0];
					power_data_aux.pf[1] = power_data_aux.p[1] / power_data_aux.s[1];
					
					pthread_mutex_lock(&power_data_buffer_mutex);
					
					memcpy(&power_data_buffer[power_data_buffer_pos], &power_data_aux, sizeof(power_data_t));
					
					power_data_buffer_pos = (power_data_buffer_pos + 1) % POWER_DATA_BUFFER_SIZE;
					if(power_data_buffer_count < POWER_DATA_BUFFER_SIZE)
						power_data_buffer_count++;
					
					pthread_mutex_unlock(&power_data_buffer_mutex);
				}
			}
			
			client_ctx.counter++;
			
			if((command_result = send_comand_and_receive_response(&client_ctx, OP_DELETE_DATA, aux, NULL, 0))) {
				LOG_ERROR("Error sending OP_DELETE_DATA command. (%s)", get_comm_status_text(command_result));
				close(client_ctx.socket_fd);
				break;
			}
			
			sleep(1);
		}
	}
	
	MHD_stop_daemon(d);
	
	pthread_mutex_destroy(&power_data_buffer_mutex);
	
	sigaction(SIGINT, &old_sa, NULL);
	
	if(terminate) {
		LOG_INFO("Terminating...");
		
		if(client_ctx.socket_fd >= 0) {
			if((command_result = send_comand_and_receive_response(&client_ctx, OP_DISCONNECT, "1000\t", NULL, 0)))
				LOG_ERROR("Error sending OP_DISCONNECT command. (%s)", get_comm_status_text(command_result));
			
			shutdown(client_ctx.socket_fd, SHUT_RDWR);
		}
		close(client_ctx.socket_fd);
	}
	
	shutdown(main_socket, SHUT_RDWR);
	close(main_socket);
	
	return 0;
}
