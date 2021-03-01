#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "common.h"
#include "logger.h"
#include "configs.h"
#include "communication.h"
#include "power_data.h"
#include "energy.h"
#include "device_events.h"
#include "database.h"

#define COMM_PORT 2048
#define POWER_DATA_BUFFER_SIZE 24 * 3600

#define POWER_DATA_STORAGE_IFMT "%li,%lf,%lf,%lf,%lf,%lf,%lf\n"
#define POWER_DATA_STORAGE_OFMT "%li,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf\n"
#define POWER_DATA_STORAGE_ARGS(var)	var.timestamp,\
										var.v[0], var.v[1],\
										var.i[0], var.i[1],\
										var.p[0], var.p[1]
#define POWER_DATA_STORAGE_ARG_QTY 7

#define MAX_EVENT_FETCH_QTY 10


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
		
		pd_aux.q[0] = sqrtf(powf(pd_aux.s[0], 2) - powf(pd_aux.p[0], 2));
		pd_aux.q[1] = sqrtf(powf(pd_aux.s[1], 2) - powf(pd_aux.p[1], 2));
		
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
	int main_socket;
	comm_client_ctx client_ctx;
	int result;
	time_t last_loaded_timestamp = 0;
	char pd_filename[32];
	char new_pd_filename[32];
	FILE *pd_file = NULL;
	
	load_saved_power_data();
	// TODO: Try to store last loaded power data entry
	
	generate_pd_filename(time(NULL), pd_filename, sizeof(pd_filename));
	pd_file = fopen(pd_filename, "a");
	if(pd_file == NULL) {
		LOG_ERROR("Failed to open power data file \"%s\": %s", pd_filename, strerror(errno));
		return NULL;
	}
	
	if(power_data_buffer_count > 0)
		last_loaded_timestamp = power_data_buffer[(power_data_buffer_pos == 0) ? (POWER_DATA_BUFFER_SIZE - 1) : (power_data_buffer_pos - 1)].timestamp;
	
	main_socket = comm_create_main_socket(1);
	
	if(main_socket < 0) {
		LOG_ERROR("Failed to create/setup socket.");
		return NULL;
	}
	
	LOG_INFO("Waiting for device connection on port %d.", COMM_PORT);
	
	while(!(*terminate)) {
		char mac_key[32] = "";
		char param_str[16];
		char received_parameters[PARAM_MAX_QTY][PARAM_STR_SIZE];
		int received_qty;
		int pd_qty, e_qty;
		int repeated_counter;
		power_data_t pd_aux;
		
		configs_get_value("device_mac_key", mac_key, sizeof(mac_key));
		
		if(comm_accept_client(main_socket, &client_ctx, mac_key) < 0) {
			LOG_ERROR("Failed to accept new client connection.");
			sleep(1);
			continue;
		}
		
		LOG_INFO("Received connection from %s", inet_ntoa(client_ctx.address.sin_addr));
		LOG_INFO("Device firmware version: %s", client_ctx.version);
		
		while(!(*terminate)) {
			generate_pd_filename(time(NULL), new_pd_filename, sizeof(pd_filename));
			
			// TODO: mover a verificação para dentro do loop, usando o timestamp recebido
			/* Quando o dia terminar, fecha o arquivo atual e cria um novo. */
			if(strcmp(pd_filename, new_pd_filename)) {
				LOG_INFO("Changing to new file \"%s\".", new_pd_filename);
				
				fclose(pd_file);
				
				pd_file = fopen(new_pd_filename, "w");
				if(pd_file == NULL) {
					LOG_FATAL("Failed to create new power data file \"%s\": %s", new_pd_filename, strerror(errno));
					*terminate = 1;
					kill(getpid(), SIGTERM);
					break;
				}
				
				strcpy(pd_filename, new_pd_filename);
			}
			
			if((result = send_comand_and_receive_response(&client_ctx, OP_QUERY_STATUS, "A\t", received_parameters, 4))) {
				LOG_ERROR("Error sending OP_QUERY_STATUS command. (%s)", get_comm_status_text(result));
				break;
			}
			
			if(*received_parameters[0] == '0') {
				if((result = send_comand_and_receive_response(&client_ctx, OP_SAMPLING_START, NULL, NULL, 0))) {
					LOG_ERROR("Error sending OP_SAMPLING_START command. (%s)", get_comm_status_text(result));
				}
			}
			
			e_qty = pd_qty = 0;
			sscanf(received_parameters[2], "%d", &e_qty);
			sscanf(received_parameters[3], "%d", &pd_qty);
			
			if(pd_qty == 0) {
				usleep(500000);
				continue;
			}
			
			sprintf(param_str, "P\t%u\t", pd_qty);
			
			if((result = send_command(&client_ctx, OP_GET_DATA, param_str))) {
				LOG_ERROR("Error sending OP_GET_DATA command. (%s)", get_comm_status_text(result));
				break;
			}
			
			repeated_counter = 0;
			
			for(received_qty = 0; received_qty < pd_qty; received_qty++) {
				if((result = receive_response(&client_ctx, OP_GET_DATA, NULL, received_parameters, 9))) {
					LOG_ERROR("Error receiving OP_GET_DATA response. (%s)", get_comm_status_text(result));
					break;
				}
				
				memset(&pd_aux, 0, sizeof(power_data_t));
				
				result = sscanf(received_parameters[0], "%li", &pd_aux.timestamp);
				result += sscanf(received_parameters[3], "%lf", &pd_aux.v[0]);
				result += sscanf(received_parameters[4], "%lf", &pd_aux.v[1]);
				result += sscanf(received_parameters[5], "%lf", &pd_aux.i[0]);
				result += sscanf(received_parameters[6], "%lf", &pd_aux.i[1]);
				result += sscanf(received_parameters[7], "%lf", &pd_aux.p[0]);
				result += sscanf(received_parameters[8], "%lf", &pd_aux.p[1]);
				
				if(result == 7) {
					if(pd_aux.timestamp <= last_loaded_timestamp) {
						repeated_counter++;
						continue;
					}
					
					pd_aux.s[0] = pd_aux.v[0] * pd_aux.i[0];
					pd_aux.s[1] = pd_aux.v[1] * pd_aux.i[1];
					
					pd_aux.q[0] = sqrtf(powf(pd_aux.s[0], 2) - powf(pd_aux.p[0], 2));
					pd_aux.q[1] = sqrtf(powf(pd_aux.s[1], 2) - powf(pd_aux.p[1], 2));
					
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
					
					energy_add_power(&pd_aux);
				} else {
					LOG_ERROR("Failed to parse power data response from device.");
				}
			}
			
			fflush(pd_file);
			
			client_ctx.counter++;
			
			if(received_qty < pd_qty)
				break;
			
			if(repeated_counter)
				LOG_WARN("Received %d repeated power data entries.", repeated_counter);
			
			if((result = send_comand_and_receive_response(&client_ctx, OP_DELETE_DATA, param_str, NULL, 0))) {
				LOG_ERROR("Error sending OP_DELETE_DATA command. (%s)", get_comm_status_text(result));
				break;
			}
			
			if(e_qty) {
				time_t e_timestamp;
				
				if(e_qty > MAX_EVENT_FETCH_QTY)
					e_qty = MAX_EVENT_FETCH_QTY;
				
				sprintf(param_str, "E\t%u\t", e_qty);
				
				if((result = send_command(&client_ctx, OP_GET_DATA, param_str))) {
					LOG_ERROR("Error sending OP_GET_DATA command. (%s)", get_comm_status_text(result));
					break;
				}
				
				for(received_qty = 0; received_qty < e_qty; received_qty++) {
					if((result = receive_response(&client_ctx, OP_GET_DATA, NULL, received_parameters, 2))) {
						LOG_ERROR("Error receiving OP_GET_DATA response. (%s)", get_comm_status_text(result));
						break;
					}
					
					if(sscanf(received_parameters[0], "%li", &e_timestamp) == 1 && strlen(received_parameters[1]) > 0) {
						store_device_event_db(e_timestamp, received_parameters[1]);
					} else {
						LOG_WARN("Failed to parse event response from device.");
					}
				}
				
				client_ctx.counter++;
				
				if(received_qty < e_qty)
					break;
				
				if((result = send_comand_and_receive_response(&client_ctx, OP_DELETE_DATA, param_str, NULL, 0))) {
					LOG_ERROR("Error sending OP_DELETE_DATA command. (%s)", get_comm_status_text(result));
					break;
				}
			}
			
			sleep(1);
		}
		
		close(client_ctx.socket_fd);
	}
	
	fclose(pd_file);
	
	if(*terminate) {
		LOG_INFO("Terminating data acquisition thread.");
		
		if(client_ctx.socket_fd >= 0) {
			if((result = send_comand_and_receive_response(&client_ctx, OP_DISCONNECT, "1000\t", NULL, 0)))
				LOG_ERROR("Error sending OP_DISCONNECT command. (%s)", get_comm_status_text(result));
			
			shutdown(client_ctx.socket_fd, SHUT_RDWR);
		}
		close(client_ctx.socket_fd);
	}
	
	shutdown(main_socket, SHUT_RDWR);
	close(main_socket);
	
	pthread_mutex_destroy(&power_data_buffer_mutex);
	
	return NULL;
}
