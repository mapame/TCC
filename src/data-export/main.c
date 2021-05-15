#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bsd/string.h>
#include <math.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <signal.h>

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "common.h"
#include "logger.h"
#include "communication.h"

#define WAVEFORM_MAX_QTY 100


typedef struct power_data_s {
	time_t timestamp;
	double v[2];
	double i[2];
	double p[2];
	double s[2];
	double q[2];
} power_data_t;


static int terminate = 0;

void sigint_handler(int signum)
{
	terminate = 1;
}

static void print_usage(const char *filename)
{
	fprintf(stderr, "Usage: %s [-r] mac_key output_file\n", filename);
	fprintf(stderr, "\t -r Reuse address and port\n");
	fprintf(stderr, "\t -w Export waveform instead of power data\n");
	fprintf(stderr, "\t -q Quantity of waveform points to export\n");
	fprintf(stderr, "\t -p Phase to export waveform\n");
}

int main(int argc, char **argv) {
	struct sigaction sa, old_sa;
	int opt;
	int reuse_addr_opt = 0;
	int waveform_opt = 0;
	int waveform_qty = WAVEFORM_MAX_QTY;
	int waveform_phase = 1;
	char mac_key[PARAM_STR_SIZE];
	char output_filename[200];
	
	int main_socket;
	comm_client_ctx client_ctx;
	
	int command_result;
	char aux[200];
	char received_parameters[PARAM_MAX_QTY][PARAM_STR_SIZE];
	
	time_t last_timestamp = 0;
	
	FILE *output_fd = NULL;
	
	while ((opt = getopt(argc, argv, "rwq:p:")) != -1) {
		switch (opt) {
			case 'r':
				reuse_addr_opt = 1;
				break;
			case 'w':
				waveform_opt = 1;
				break;
			case 'q':
				sscanf(optarg, "%d", &waveform_qty);
				break;
			case 'p':
				sscanf(optarg, "%d", &waveform_phase);
				break;
			default:
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
		}
	}
	
	if(optind + 2 > argc) {
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	
	logger_set_level(LOGLEVEL_INFO);
	
	if(waveform_qty < 0 || waveform_qty > WAVEFORM_MAX_QTY) {
		LOG_FATAL("Invalid waveform quantity parameter.\n");
		exit(EXIT_FAILURE);
	}
	
	if(waveform_phase < 1 || waveform_phase > 3) {
		LOG_FATAL("Invalid waveform phase parameter.\n");
		exit(EXIT_FAILURE);
	}
	
	strlcpy(mac_key, argv[optind], PARAM_STR_SIZE);
	strlcpy(output_filename, argv[optind + 1], 200);
	
	output_fd = fopen(output_filename, waveform_opt ? "w" : "a");
	
	if(!output_fd) {
		LOG_FATAL("Unable to open file %s.", output_filename);
		return -1;
	}
	
	sa.sa_handler = sigint_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, &old_sa);
	
	main_socket = comm_create_main_socket(reuse_addr_opt);
	
	if(main_socket < 0) {
		LOG_ERROR("Failed to create/setup socket.");
		return -1;
	}
	
	while(!terminate) {
		power_data_t received_power_data;
		int conversion_result;
		int qty;
		
		if(comm_accept_client(main_socket, &client_ctx, mac_key, &terminate) < 0) {
			LOG_ERROR("Failed to accept new client connection.");
			continue;
		}
		
		if(!terminate) {
			LOG_INFO("Received connection from %s", inet_ntoa(client_ctx.address.sin_addr));
			LOG_INFO("Device firmware version: %s", client_ctx.version);
		}
		
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
			
			if(waveform_opt == 1) {
				float waveform_v, waveform_i;
				
				sprintf(aux, "%u\t%u\t", waveform_phase, waveform_qty);
				if((command_result = send_command(&client_ctx, OP_GET_WAVEFORM, aux))) {
					LOG_ERROR("Error sending OP_GET_WAVEFORM command. (%d)\n", command_result);
					close(client_ctx.socket_fd);
					break;
				}
				
				for(int i = 0; i < waveform_qty; i++) {
					if((command_result = receive_response(&client_ctx, OP_GET_WAVEFORM, NULL, received_parameters, 2))) {
						LOG_ERROR("Error receiving OP_GET_WAVEFORM response. (%d)\n", command_result);
						close(client_ctx.socket_fd);
						break;
					}
					
					conversion_result = 0;
					
					conversion_result += sscanf(received_parameters[0], "%f", &waveform_v);
					conversion_result += sscanf(received_parameters[1], "%f", &waveform_i);
					
					if(conversion_result == 2)
						fprintf(output_fd, "%f,%f\n", waveform_v, waveform_i);
					
				}
				
				client_ctx.counter++;
				
				terminate = 1;
				
				break;
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
				if((command_result = receive_response(&client_ctx, OP_GET_DATA, NULL, received_parameters, 9))) {
					LOG_ERROR("Error receiving OP_GET_DATA response. (%s)", get_comm_status_text(command_result));
					close(client_ctx.socket_fd);
					break;
				}
				conversion_result = 0;
				
				conversion_result += sscanf(received_parameters[0], "%ld", &received_power_data.timestamp);
				
				if(conversion_result && received_power_data.timestamp <= last_timestamp)
					continue;
				
				conversion_result += sscanf(received_parameters[3], "%lf", &received_power_data.v[0]);
				conversion_result += sscanf(received_parameters[4], "%lf", &received_power_data.v[1]);
				
				conversion_result += sscanf(received_parameters[6], "%lf", &received_power_data.i[0]);
				conversion_result += sscanf(received_parameters[7], "%lf", &received_power_data.i[1]);
				
				conversion_result += sscanf(received_parameters[9], "%lf", &received_power_data.p[0]);
				conversion_result += sscanf(received_parameters[10], "%lf", &received_power_data.p[1]);
				
				if(conversion_result == 7) {
					received_power_data.s[0] = received_power_data.v[0] * received_power_data.i[0];
					received_power_data.s[1] = received_power_data.v[1] * received_power_data.i[1];
					
					received_power_data.q[0] = sqrtf(powf(received_power_data.s[0], 2) - powf(received_power_data.p[0], 2));
					received_power_data.q[1] = sqrtf(powf(received_power_data.s[1], 2) - powf(received_power_data.p[1], 2));
					
					fprintf(output_fd, "%ld,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\n", received_power_data.timestamp,
																		received_power_data.v[0], received_power_data.v[1],
																		received_power_data.i[0], received_power_data.i[1],
																		received_power_data.p[0], received_power_data.p[1],
																		received_power_data.s[0], received_power_data.s[1],
																		received_power_data.q[0], received_power_data.q[1]);
					
					last_timestamp = received_power_data.timestamp;
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
	
	sigaction(SIGINT, &old_sa, NULL);
	
	fclose(output_fd);
	
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
