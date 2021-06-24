#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "common.h"
#include "logger.h"
#include "config.h"
#include "communication.h"
#include "power.h"
#include "energy.h"
#include "meter_events.h"
#include "database.h"

#define COMM_PORT 2048

#define MAX_POWER_FETCH_QTY 30
#define MAX_EVENT_FETCH_QTY 10


void *data_acquisition_loop(void *argp) {
	int *terminate = (int*) argp;
	int main_socket;
	comm_client_ctx client_ctx;
	int result;
	time_t last_loaded_timestamp = 0;
	
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
		
		config_get_value("device_mac_key", mac_key, sizeof(mac_key));
		
		if(comm_accept_client(main_socket, &client_ctx, mac_key, terminate) < 0) {
			LOG_ERROR("Failed to accept new client connection.");
			sleep(1);
			continue;
		}
		
		if(*terminate == 0) {
			LOG_INFO("Received connection from %s", inet_ntoa(client_ctx.address.sin_addr));
			LOG_INFO("Device firmware version: %s", client_ctx.version);
		}
		
		while(!(*terminate)) {
			if((result = send_comand_and_receive_response(&client_ctx, OP_QUERY_STATUS, "A\t", received_parameters, 4))) {
				LOG_ERROR("Error sending OP_QUERY_STATUS command. (%s)", get_comm_status_text(result));
				break;
			}
			
			if(*received_parameters[0] == '0') {
				LOG_INFO("Sampling is paused, restarting.");
				
				if((result = send_comand_and_receive_response(&client_ctx, OP_SAMPLING_START, NULL, NULL, 0))) {
					LOG_ERROR("Error sending OP_SAMPLING_START command. (%s)", get_comm_status_text(result));
				}
			}
			
			e_qty = pd_qty = 0;
			sscanf(received_parameters[2], "%d", &e_qty);
			sscanf(received_parameters[3], "%d", &pd_qty);
			
			if(pd_qty > MAX_POWER_FETCH_QTY)
				pd_qty = MAX_POWER_FETCH_QTY;
			
			if(e_qty > MAX_EVENT_FETCH_QTY)
				e_qty = MAX_EVENT_FETCH_QTY;
			
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
			
			// TODO: break when terminate is 1
			for(received_qty = 0; received_qty < pd_qty; received_qty++) {
				if((result = receive_response(&client_ctx, OP_GET_DATA, NULL, received_parameters, 9))) {
					LOG_ERROR("Error receiving OP_GET_DATA response. (%s)", get_comm_status_text(result));
					break;
				}
				
				result = sscanf(received_parameters[0], "%li", &pd_aux.timestamp);
				result += sscanf(received_parameters[3], "%lf", &pd_aux.v[0]);
				result += sscanf(received_parameters[4], "%lf", &pd_aux.v[1]);
				result += sscanf(received_parameters[5], "%lf", &pd_aux.i[0]);
				result += sscanf(received_parameters[6], "%lf", &pd_aux.i[1]);
				result += sscanf(received_parameters[7], "%lf", &pd_aux.p[0]);
				result += sscanf(received_parameters[8], "%lf", &pd_aux.p[1]);
				
				if(result != 7) {
					LOG_ERROR("Failed to parse power data response from device.");
					break;
				}
				
				if(pd_aux.timestamp <= last_loaded_timestamp) {
					repeated_counter++;
					continue;
				}
				
				if(store_power_data(&pd_aux) < 0) {
					*terminate = 1;
					kill(getpid(), SIGTERM);
					break;
				}
				
				energy_add_power(&pd_aux);
				
				last_loaded_timestamp = pd_aux.timestamp;
			}
			
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
						store_meter_event_db(e_timestamp, received_parameters[1]);
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
		
		if(*terminate == 0)
			close(client_ctx.socket_fd);
	}
	
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
	
	return NULL;
}
