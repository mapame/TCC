#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "bearssl.h"

#include "common.h"
#include "logger.h"
#include "communication.h"
#include "power_data.h"

#define COMM_PORT 2048
#define POWER_DATA_BUFFER_SIZE 24 * 3600

#define POWER_DATA_STORAGE_IFMT "%li,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf\n"
#define POWER_DATA_STORAGE_OFMT "%li,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf\n"
#define POWER_DATA_STORAGE_ARGS(var)	var.timestamp,\
										var.v[0], var.v[1], var.v[2],\
										var.i[0], var.i[1], var.i[2],\
										var.p[0], var.p[1], var.p[2]
#define POWER_DATA_STORAGE_ARG_QTY 10


const char mac_key[8] = "abc123";

pthread_mutex_t power_data_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

power_data_t power_data_buffer[POWER_DATA_BUFFER_SIZE];
int power_data_buffer_pos = 0;
int power_data_buffer_count = 0;

static size_t generate_pd_filename(time_t time_epoch, char *buffer, size_t len) {
	struct tm time_tm;
	
	gmtime_r(&time_epoch, &time_tm);
	return strftime(buffer, len, "pd-%F.csv", &time_tm);
}

static int import_power_data_file(const char *filename, time_t timestamp_limit) {
	FILE *pd_file = NULL;
	power_data_t pd_aux;
	time_t last_loaded_timestamp = 0;
	int counter = 0;
	
	if((pd_file = fopen(filename, "r")) == NULL) {
		LOG_ERROR("Failed to open power data file \"%s\": %s", filename, strerror(errno));
		return -1;
	}
	
	pthread_mutex_lock(&power_data_buffer_mutex);
	
	if(power_data_buffer_count > 0)
		last_loaded_timestamp = power_data_buffer[(power_data_buffer_pos == 0) ? (POWER_DATA_BUFFER_SIZE - 1) : (power_data_buffer_pos - 1)].timestamp;
	
	while(fscanf(pd_file, POWER_DATA_STORAGE_IFMT, POWER_DATA_STORAGE_ARGS(&pd_aux)) == POWER_DATA_STORAGE_ARG_QTY) {
		if(pd_aux.timestamp < timestamp_limit)
			continue;
		
		/* Não carrega valores fora de ordem */
		if(pd_aux.timestamp <= last_loaded_timestamp)
			continue;
		
		counter++;
		
		last_loaded_timestamp = pd_aux.timestamp;
		
		pd_aux.s[0] = pd_aux.v[0] * pd_aux.i[0];
		pd_aux.s[1] = pd_aux.v[1] * pd_aux.i[1];
		pd_aux.s[2] = pd_aux.v[2] * pd_aux.i[2];
		
		pd_aux.q[0] = sqrtf(powf(pd_aux.s[0], 2) - powf(pd_aux.p[0], 2));
		pd_aux.q[1] = sqrtf(powf(pd_aux.s[1], 2) - powf(pd_aux.p[1], 2));
		pd_aux.q[2] = sqrtf(powf(pd_aux.s[2], 2) - powf(pd_aux.p[2], 2));
		
		memcpy(&power_data_buffer[power_data_buffer_pos], &pd_aux, sizeof(power_data_t));
		
		power_data_buffer_pos = (power_data_buffer_pos + 1) % POWER_DATA_BUFFER_SIZE;
		if(power_data_buffer_count < POWER_DATA_BUFFER_SIZE)
			power_data_buffer_count++;
	}
	
	pthread_mutex_unlock(&power_data_buffer_mutex);
	
	fclose(pd_file);
	
	return counter;
}

static int load_saved_power_data() {
	time_t time_epoch;
	char filename_today[32];
	char filename_yesterday[32];
	int result;
	
	time_epoch = time(NULL);
	generate_pd_filename(time_epoch, filename_today, sizeof(filename_today));
	
	/* Subtrai os segundos equivalentes a 24 horas para obter o dia de ontem */
	time_epoch -= (24 * 60 * 60);
	generate_pd_filename(time_epoch, filename_yesterday, sizeof(filename_yesterday));
	
	if(access(filename_yesterday, F_OK) == 0) {
		LOG_INFO("Loading power data from yesterday's file \"%s\".", filename_yesterday);
		
		result = import_power_data_file(filename_yesterday, time_epoch);
		if(result < 0)
			LOG_ERROR("Failed to load power data from yesterday's file.");
		else
			LOG_INFO("Loaded %d entries from yesterday's file.", result);
	}
	
	if(access(filename_today, F_OK) == 0) {
		LOG_INFO("Loading power data from today's file \"%s\".", filename_today);
		
		result = import_power_data_file(filename_today, 0);
		if(result < 0)
			LOG_ERROR("Failed to load power data from today's file.");
		else
			LOG_INFO("Loaded %d entries from today's file.", result);
	}
	
	return 0;
}

void *data_acquisition_loop(void *argp) {
	int *terminate = (int*) argp;
	int main_socket, client_socket;
	struct sockaddr_in server_bind_address, client_address;
	unsigned int client_address_size = sizeof(struct sockaddr_in);
	int result;
	int counter;
	br_hmac_key_context hmac_key_ctx;
	time_t last_loaded_timestamp = 0;
	char pd_filename[32];
	char new_pd_filename[32];
	FILE *pd_file = NULL;
	
	load_saved_power_data();
	
	generate_pd_filename(time(NULL), pd_filename, sizeof(pd_filename));
	pd_file = fopen(pd_filename, "a");
	if(pd_file == NULL) {
		LOG_ERROR("Failed to open power data file \"%s\": %s", pd_filename, strerror(errno));
		return NULL;
	}
	
	if(power_data_buffer_count > 0)
		last_loaded_timestamp = power_data_buffer[(power_data_buffer_pos == 0) ? (POWER_DATA_BUFFER_SIZE - 1) : (power_data_buffer_pos - 1)].timestamp;
	
	br_hmac_key_init(&hmac_key_ctx, &br_md5_vtable, mac_key, strlen(mac_key));
	
	main_socket = socket(PF_INET, SOCK_STREAM, 0);
	if(main_socket < 0) {
		LOG_FATAL("Failed to create main socket.");
		return NULL;
	}
	
	setsockopt(main_socket, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));
	
	bzero(&server_bind_address, sizeof(server_bind_address));
	server_bind_address.sin_family      = AF_INET;
	server_bind_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_bind_address.sin_port        = htons(COMM_PORT);
	
	if(bind(main_socket, (struct sockaddr *) &server_bind_address, sizeof(server_bind_address)) < 0) {
		LOG_FATAL("Failed to bind main socket.");
		close(main_socket);
		return NULL;
	}
	
	if(listen(main_socket, 2) < 0) {
		LOG_FATAL("Failed to listen in the main socket.");
		close(main_socket);
		return NULL;
	}
	
	LOG_INFO("Waiting for device connection on port %d.", COMM_PORT);
	
	while(!(*terminate)) {
		char param_str[16];
		power_data_t pd_aux;
		time_t aux_timestamp;
		char received_parameters[PARAM_MAX_QTY][PARAM_STR_SIZE];
		int qty;
		
		client_socket = accept(main_socket, (struct sockaddr *) &client_address, &client_address_size);
		if(client_socket < 0) {
			if(!(*terminate))
				LOG_ERROR("Failed to accept connection.");
			continue;
		}
		
		LOG_INFO("Received connection from %s", inet_ntoa(client_address.sin_addr));
		
		struct timeval socket_timeout_value = {.tv_sec = 2, .tv_usec = 0};
		if(setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&socket_timeout_value, sizeof(socket_timeout_value)) < 0)
			LOG_ERROR("Failed to set socket timeout.");
		
		counter = 0;
		
		if((result = send_comand_and_receive_response(client_socket, &hmac_key_ctx, OP_PROTOCOL_START, counter++, NULL, received_parameters, 1))) {
			LOG_ERROR("Error sending OP_PROTOCOL_START command. (%s)", get_comm_status_text(result));
			close(client_socket);
			continue;
		}
		
		LOG_INFO("Device firmware version: %s", received_parameters[0]);
		
		while(!(*terminate)) {
			generate_pd_filename(time(NULL), new_pd_filename, sizeof(pd_filename));
			
			/* Quando o dia terminar, fecha o arquivo atual e cria um novo. */
			if(strcmp(pd_filename, new_pd_filename)) {
				LOG_INFO("Changing to new file \"%s\".", new_pd_filename);
				
				fclose(pd_file);
				
				pd_file = fopen(new_pd_filename, "w");
				if(pd_file == NULL) {
					LOG_ERROR("Failed to create new power data file \"%s\": %s", new_pd_filename, strerror(errno));
					*terminate = 1;
					kill(getpid(), SIGTERM);
					break;
				}
				
				strcpy(pd_filename, new_pd_filename);
			}
			
			if((result = send_comand_and_receive_response(client_socket, &hmac_key_ctx, OP_QUERY_STATUS, counter++, "A\t", received_parameters, 4))) {
				LOG_ERROR("Error sending OP_QUERY_STATUS command. (%s)", get_comm_status_text(result));
				close(client_socket);
				break;
			}
			
			if(*received_parameters[0] == '0') {
				if((result = send_comand_and_receive_response(client_socket, &hmac_key_ctx, OP_SAMPLING_START, counter++, NULL, NULL, 0))) {
					LOG_ERROR("Error sending OP_SAMPLING_START command. (%s)", get_comm_status_text(result));
					close(client_socket);
				}
			}
			
			sscanf(received_parameters[3], "%u", &qty);
			
			if(qty == 0)
				continue;
			
			sprintf(param_str, "P\t%u\t", qty);
			
			if((result = send_command(client_socket, &hmac_key_ctx, OP_GET_DATA, &aux_timestamp, counter, param_str))) {
				LOG_ERROR("Error sending OP_GET_DATA command. (%s)", get_comm_status_text(result));
				close(client_socket);
				break;
			}
			
			for(int i = 0; i < qty; i++) {
				if((result = receive_response(client_socket, &hmac_key_ctx, OP_GET_DATA, aux_timestamp, counter, NULL, received_parameters, 12))) {
					LOG_ERROR("Error receiving OP_GET_DATA response. (%s)", get_comm_status_text(result));
					close(client_socket);
					break;
				}
				memset(&pd_aux, 0, sizeof(power_data_t));
				
				result = sscanf(received_parameters[0], "%li", &pd_aux.timestamp);
				result += sscanf(received_parameters[3], "%lf", &pd_aux.v[0]);
				result += sscanf(received_parameters[4], "%lf", &pd_aux.v[1]);
				result += sscanf(received_parameters[5], "%lf", &pd_aux.v[2]);
				result += sscanf(received_parameters[6], "%lf", &pd_aux.i[0]);
				result += sscanf(received_parameters[7], "%lf", &pd_aux.i[1]);
				result += sscanf(received_parameters[8], "%lf", &pd_aux.i[2]);
				result += sscanf(received_parameters[9], "%lf", &pd_aux.p[0]);
				result += sscanf(received_parameters[10], "%lf", &pd_aux.p[1]);
				result += sscanf(received_parameters[11], "%lf", &pd_aux.p[2]);
				
				if(result == 10) {
					if(pd_aux.timestamp <= last_loaded_timestamp)
						continue;
					
					pd_aux.s[0] = pd_aux.v[0] * pd_aux.i[0];
					pd_aux.s[1] = pd_aux.v[1] * pd_aux.i[1];
					pd_aux.s[2] = pd_aux.v[2] * pd_aux.i[2];
					
					pd_aux.q[0] = sqrtf(powf(pd_aux.s[0], 2) - powf(pd_aux.p[0], 2));
					pd_aux.q[1] = sqrtf(powf(pd_aux.s[1], 2) - powf(pd_aux.p[1], 2));
					pd_aux.q[2] = sqrtf(powf(pd_aux.s[2], 2) - powf(pd_aux.p[2], 2));
					
					last_loaded_timestamp = pd_aux.timestamp;
					
					result = fprintf(pd_file, POWER_DATA_STORAGE_OFMT, POWER_DATA_STORAGE_ARGS(pd_aux));
					if(result < 0)
						LOG_ERROR("Failed to write power data to file.");
					
					pthread_mutex_lock(&power_data_buffer_mutex);
					
					memcpy(&power_data_buffer[power_data_buffer_pos], &pd_aux, sizeof(power_data_t));
					
					power_data_buffer_pos = (power_data_buffer_pos + 1) % POWER_DATA_BUFFER_SIZE;
					if(power_data_buffer_count < POWER_DATA_BUFFER_SIZE)
						power_data_buffer_count++;
					
					pthread_mutex_unlock(&power_data_buffer_mutex);
				}
			}
			
			fflush(pd_file);
			
			counter++;
			
			if((result = send_comand_and_receive_response(client_socket, &hmac_key_ctx, OP_DELETE_DATA, counter++, param_str, NULL, 0))) {
				LOG_ERROR("Error sending OP_DELETE_DATA command. (%s)", get_comm_status_text(result));
				close(client_socket);
				break;
			}
			
			sleep(1);
		}
	}
	
	fclose(pd_file);
	
	if(*terminate) {
		LOG_INFO("Terminating data acquisition thread.");
		
		if(client_socket >= 0) {
			if((result = send_comand_and_receive_response(client_socket, &hmac_key_ctx, OP_DISCONNECT, counter++, "1000\t", NULL, 0)))
				LOG_ERROR("Error sending OP_DISCONNECT command. (%s)", get_comm_status_text(result));
			
			shutdown(client_socket, SHUT_RDWR);
		}
		close(client_socket);
	}
	
	shutdown(main_socket, SHUT_RDWR);
	close(main_socket);
	
	pthread_mutex_destroy(&power_data_buffer_mutex);
	
	return NULL;
}
