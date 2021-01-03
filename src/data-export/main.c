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

#include "bearssl.h"

#include "common.h"
#include "logger.h"
#include "communication.h"

#define COMM_SERVER_PORT 2048


typedef struct power_data_s {
	time_t timestamp;
	double v[3];
	double i[3];
	double p[3];
	double s[3];
	double q[3];
} power_data_t;


static int terminate = 0;

void sigint_handler(int signum)
{
	terminate = 1;
}

static void print_usage(const char *filename)
{
	fprintf(stderr, "Usage: %s [-r] deviceID password output_file\n", filename);
	fprintf(stderr, "  -r Reuse address and port\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
	struct sigaction sa, old_sa;
	int opt;
	int reuse_addr = 0;
	char device_id[PARAM_STR_SIZE];
	char mac_password[PARAM_STR_SIZE];
	char output_filename[200];
	
	FILE *output_fd = NULL;
	
	int main_socket;
	struct sockaddr_in server_bind_address;
	
	logger_init(stderr, LOGLEVEL_INFO);
	
	while ((opt = getopt(argc, argv, "r")) != -1) {
		switch (opt) {
			case 'r':
				reuse_addr = 1;
				break;
			default:
				print_usage(argv[0]);
		}
	}
	
	if(optind + 3 > argc)
		print_usage(argv[0]);
	
	strlcpy(device_id, argv[optind], PARAM_STR_SIZE);
	strlcpy(mac_password, argv[optind + 1], PARAM_STR_SIZE);
	strlcpy(output_filename, argv[optind + 2], 200);
	
	output_fd = fopen(output_filename, "a");
	
	if(!output_fd) {
		LOG_FATAL("Unable to open file %s.", output_filename);
		return -1;
	}
	
	sa.sa_handler = sigint_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, &old_sa);
	
	
	main_socket = socket(PF_INET, SOCK_STREAM, 0);
	if(main_socket < 0) {
		LOG_FATAL("Unable to create main socket.");
		return -1;
	}
	
	if(reuse_addr)
		setsockopt(main_socket, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));
	
	bzero(&server_bind_address, sizeof(server_bind_address));
	server_bind_address.sin_family      = AF_INET;
	server_bind_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_bind_address.sin_port        = htons(COMM_SERVER_PORT);
	
	if(bind(main_socket, (struct sockaddr *) &server_bind_address, sizeof(server_bind_address)) < 0) {
		LOG_FATAL("Unable to bind main socket.");
		return -1;
	}
	
	if(listen(main_socket, 2) < 0) {
		LOG_FATAL("Unable to put main socket in listening mode.");
		return -1;
	}
	
	LOG_INFO("Waiting for device connection on port %d...", COMM_SERVER_PORT);
	fflush(stdin);
	
	int client_socket;
	struct sockaddr_in client_address;
	unsigned int client_address_size = sizeof(struct sockaddr_in);
	
	br_hmac_key_context hmac_key_ctx;
	
	int counter = 0;
	time_t aux_timestamp;
	int command_result;
	char aux[200];
	char received_parameters[PARAM_MAX_QTY][PARAM_STR_SIZE];
	
	time_t last_timestamp;
	
	br_hmac_key_init(&hmac_key_ctx, &br_md5_vtable, mac_password, strlen(mac_password));
	
	while(!terminate) {
		power_data_t received_power_data;
		int conversion_result;
		int qty;
		
		while(!terminate) {
			client_socket = accept(main_socket, (struct sockaddr *) &client_address, &client_address_size);
			if(client_socket < 0) {
				LOG_ERROR("Unable to accept connection.");
				continue;
			}
			
			LOG_INFO("Received connection from %s", inet_ntoa(client_address.sin_addr));
			fflush(stdin);
			
			struct timeval socket_timeout_value = {.tv_sec = 2, .tv_usec = 0};
			if(setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&socket_timeout_value, sizeof(socket_timeout_value)) < 0)
				LOG_ERROR("Unable to set socket timeout value.");
			
			counter = 0;
			
			if((command_result = send_comand_and_receive_response(client_socket, &hmac_key_ctx, OP_PROTOCOL_START, counter++, NULL, received_parameters, 2))) {
				LOG_ERROR("Error sending OP_PROTOCOL_START command. (%s)", get_comm_status_text(command_result));
				close(client_socket);
				continue;
			}
			
			if(strcmp(received_parameters[0], device_id) == 0)
				break;
			
			send_comand_and_receive_response(client_socket, &hmac_key_ctx, OP_DISCONNECT, counter++, "2000\t", NULL, 0);
			shutdown(client_socket, SHUT_RDWR);
			close(client_socket);
			LOG_INFO("Wrong device, disconneting...");
		}
		
		if(!terminate) {
			LOG_INFO("Device firmware version: %s", received_parameters[1]);
		}
		
		while(!terminate) {
			if((command_result = send_comand_and_receive_response(client_socket, &hmac_key_ctx, OP_QUERY_STATUS, counter++, "A\t", received_parameters, 4))) {
				LOG_ERROR("Error sending OP_QUERY_STATUS command. (%s)", get_comm_status_text(command_result));
				close(client_socket);
				break;
			}
			
			if(*received_parameters[0] == '0') {
				if((command_result = send_comand_and_receive_response(client_socket, &hmac_key_ctx, OP_SAMPLING_START, counter++, NULL, NULL, 0))) {
					LOG_ERROR("Error sending OP_SAMPLING_START command. (%s)", get_comm_status_text(command_result));
					close(client_socket);
				}
			}
			
			sscanf(received_parameters[3], "%u", &qty);
			
			if(qty == 0)
				continue;
			
			sprintf(aux, "P\t%u\t", qty);
			
			if((command_result = send_command(client_socket, &hmac_key_ctx, OP_GET_DATA, &aux_timestamp, counter, aux))) {
				LOG_ERROR("Error sending OP_GET_DATA command. (%s)", get_comm_status_text(command_result));
				close(client_socket);
				break;
			}
			
			for(int i = 0; i < qty; i++) {
				if((command_result = receive_response(client_socket, &hmac_key_ctx, OP_GET_DATA, aux_timestamp, counter, NULL, received_parameters, 12))) {
					LOG_ERROR("Error receiving OP_GET_DATA response. (%s)", get_comm_status_text(command_result));
					close(client_socket);
					break;
				}
				conversion_result = 0;
				
				conversion_result += sscanf(received_parameters[0], "%ld", &received_power_data.timestamp);
				
				if(conversion_result && received_power_data.timestamp <= last_timestamp)
					continue;
				
				conversion_result += sscanf(received_parameters[3], "%lf", &received_power_data.v[0]);
				conversion_result += sscanf(received_parameters[4], "%lf", &received_power_data.v[1]);
				conversion_result += sscanf(received_parameters[5], "%lf", &received_power_data.v[2]);
				
				conversion_result += sscanf(received_parameters[6], "%lf", &received_power_data.i[0]);
				conversion_result += sscanf(received_parameters[7], "%lf", &received_power_data.i[1]);
				conversion_result += sscanf(received_parameters[8], "%lf", &received_power_data.i[2]);
				
				conversion_result += sscanf(received_parameters[9], "%lf", &received_power_data.p[0]);
				conversion_result += sscanf(received_parameters[10], "%lf", &received_power_data.p[1]);
				conversion_result += sscanf(received_parameters[11], "%lf", &received_power_data.p[2]);
				
				if(conversion_result == 10) {
					received_power_data.s[0] = received_power_data.v[0] * received_power_data.i[0];
					received_power_data.s[1] = received_power_data.v[1] * received_power_data.i[1];
					received_power_data.s[2] = received_power_data.v[2] * received_power_data.i[2];
					
					received_power_data.q[0] = sqrtf(powf(received_power_data.s[0], 2) - powf(received_power_data.p[0], 2));
					received_power_data.q[1] = sqrtf(powf(received_power_data.s[1], 2) - powf(received_power_data.p[1], 2));
					received_power_data.q[2] = sqrtf(powf(received_power_data.s[2], 2) - powf(received_power_data.p[2], 2));
					
					fprintf(output_fd, "%ld,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\n", received_power_data.timestamp,
																		received_power_data.v[0], received_power_data.v[1], received_power_data.v[2],
																		received_power_data.i[0], received_power_data.i[1], received_power_data.i[2],
																		received_power_data.p[0], received_power_data.p[1], received_power_data.p[2],
																		received_power_data.s[0], received_power_data.s[1], received_power_data.s[2],
																		received_power_data.q[0], received_power_data.q[1], received_power_data.q[2]);
					
					last_timestamp = received_power_data.timestamp;
				}
			}
			
			counter++;
			
			if((command_result = send_comand_and_receive_response(client_socket, &hmac_key_ctx, OP_DELETE_DATA, counter++, aux, NULL, 0))) {
				LOG_ERROR("Error sending OP_DELETE_DATA command. (%s)", get_comm_status_text(command_result));
				close(client_socket);
				break;
			}
			
			sleep(1);
		}
	}
	
	sigaction(SIGINT, &old_sa, NULL);
	
	fclose(output_fd);
	
	if(terminate) {
		LOG_INFO("Terminating...");
		
		if((command_result = send_comand_and_receive_response(client_socket, &hmac_key_ctx, OP_DISCONNECT, counter++, "1000\t", NULL, 0)))
			LOG_ERROR("Error sending OP_DISCONNECT command. (%s)", get_comm_status_text(command_result));
		
		shutdown(client_socket, SHUT_RDWR);
		close(client_socket);
	}
	
	shutdown(main_socket, SHUT_RDWR);
	close(main_socket);
	
	return 0;
}
