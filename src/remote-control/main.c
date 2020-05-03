#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bsd/string.h>
#include <math.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "bearssl.h"

#include "common.h"
#include "logger.h"
#include "communication.h"
#include "tftp.h"
#include "gnuplot.h"


#define COMM_SERVER_PORT 2048

#define TFTP_PORT 6969

#define WAVEFORM_MAX_QTY 100


typedef enum {
	ACT_START_SAMPLING,
	ACT_PAUSE_SAMPLING,
	ACT_WRITE_CONFIG,
	ACT_READ_CONFIG,
	ACT_RESTART_DEVICE,
	ACT_FW_UPDATE,
	ACT_READ_STATUS,
	ACT_GET_DATA,
	ACT_PLOT_WAVEFORM,
	ACTION_NUM
} protocol_opcodes_t;

typedef enum {
	DATA_TYPE_INTERNAL_EVENT,
	DATA_TYPE_INTERNAL_EVENT_FLASH,
	DATA_TYPE_POWER_EVENT,
	DATA_TYPE_POWER_EVENT_FLASH,
	DATA_TYPE_POWER_DATA,
	DATA_TYPE_POWER_DATA_FLASH,
	DATA_TYPE_QTY
} data_types_t;


typedef struct action_metadata_s {
	char action_text[32];
	int parameter_qty;
} action_metadata_t;

action_metadata_t action_metadata_list[ACTION_NUM] = {
	{"sampling_start",		0},
	{"sampling_pause",		0},
	{"config_write",		2},
	{"config_read",			1},
	{"restart",				0},
	{"fw_update",			1},
	{"status",				0},
	{"get_data",			3},
	{"plot_waveform",		2}
};

unsigned int data_type_status_index[DATA_TYPE_QTY] = {5, 8, 6, 9, 7, 10};
unsigned int data_type_response_parameter_qty[DATA_TYPE_QTY] = {4, 4, 6, 6, 12, 8};

int convert_action(char *buf) {
	if(!buf)
		return -1;
	
	for(int i = 0; i < ACTION_NUM; i++)
		if(!strcmp(buf, action_metadata_list[i].action_text))
			return i;
	
	return -1;
}

size_t calculate_file_md5(FILE *fd, char *text_result) {
	br_md5_context hash_context;
	uint8_t buffer[1024];
	size_t size_read, total_size = 0;
	uint8_t hash_result[16];
	
	char aux[3];
	
	br_md5_init(&hash_context);
	
	rewind(fd);
	
	while(!feof(fd)) {
		size_read = fread(buffer, 1, 1024, fd);
		total_size += size_read;
		br_md5_update(&hash_context, buffer, size_read);
	}
	
	rewind(fd);
	
	br_md5_out(&hash_context, hash_result);
	
	text_result[0] = '\0';
	
	for(int i = 0; i < 16; i++) {
		sprintf(aux, "%02hx", hash_result[i]);
		strlcat(text_result, aux, 33);
	}
	
	return total_size;
}

int action_fw_update(int client_socket, br_hmac_key_context *hmac_key_ctx, int *counter, FILE *fd) {
	char hash_result_text[33];
	unsigned int file_size;
	int command_result;
	char received_parameters[PARAM_MAX_QTY][PARAM_STR_SIZE];
	char aux[100];
	
	file_size = calculate_file_md5(fd, hash_result_text);
	
	printf("Firmware file MD5 hash: %s\n", hash_result_text);
	printf("File size: %u bytes\n", file_size);
	
	if((command_result = send_comand_and_receive_response(client_socket, hmac_key_ctx, OP_QUERY_STATUS, (*counter)++, NULL, received_parameters, 1))) {
		fprintf(stderr, "Error sending query status command. (%d)\n", command_result);
		close(client_socket);
		return -1;
	}
	
	if(strcmp(received_parameters[0], "0")) {
		if((command_result = send_comand_and_receive_response(client_socket, hmac_key_ctx, OP_SAMPLING_PAUSE, (*counter)++, NULL, NULL, 0))) {
			fprintf(stderr, "Error sending pause command. (%d)\n", command_result);
			close(client_socket);
			return -1;
		}
	}
	
	sprintf(aux, "%s\t", hash_result_text);
	
	if((command_result = send_comand_and_receive_response(client_socket, hmac_key_ctx, OP_FW_UPDATE, (*counter)++, aux, NULL, 0))) {
		fprintf(stderr, "Error sending firmware update command. (%d)\n", command_result);
		close(client_socket);
		return -1;
	}
	
	close(client_socket);
	
	if(tftp_serve_file(fd, TFTP_PORT) < 0) {
		fprintf(stderr, "Failed to serve firmware file.\n");
		return -1;
	}
	
	return 0;
}

int print_power_data(const char *v1, const char *v2, const char *v3, const char *i1, const char *i2, const char *i3, const char *p1, const char *p2, const char *p3) {
	int result = 0;
	
	float v[3];
	float i[3];
	float p[3];
	float s[3];
	float q[3];
	
	result += sscanf(v1, "%f", &v[0]);
	result += sscanf(v2, "%f", &v[1]);
	result += sscanf(v3, "%f", &v[2]);
	
	result += sscanf(i1, "%f", &i[0]);
	result += sscanf(i2, "%f", &i[1]);
	result += sscanf(i3, "%f", &i[2]);
	
	result += sscanf(p1, "%f", &p[0]);
	result += sscanf(p2, "%f", &p[1]);
	result += sscanf(p3, "%f", &p[2]);
	
	if(result != 9)
		return 1;
	
	s[0] = v[0] * i[0];
	s[1] = v[1] * i[1];
	s[2] = v[2] * i[2];
	
	q[0] = sqrtf(powf(s[0], 2) - powf(p[0], 2));
	q[1] = sqrtf(powf(s[1], 2) - powf(p[1], 2));
	q[2] = sqrtf(powf(s[2], 2) - powf(p[2], 2));
	
	printf("A:  %5.1f V | %7.3f I | %7.1f W | %7.1f VA | %7.1f VAr | PF %5.2f\n", v[0], i[0], p[0], s[0], q[0], p[0] / s[0]);
	printf("B:  %5.1f V | %7.3f I | %7.1f W | %7.1f VA | %7.1f VAr | PF %5.2f\n", v[1], i[1], p[1], s[1], q[1], p[1] / s[1]);
	printf("C:  %5.1f V | %7.3f I | %7.1f W | %7.1f VA | %7.1f VAr | PF %5.2f\n", v[2], i[2], p[2], s[2], q[2], p[2] / s[2]);
	printf("Total:                    %7.1f W | %7.1f VA | %7.1f VAr |\n", p[0] + p[1] + p[2], s[0] + s[1] + s[2], q[0] + q[1] + q[2]);
	
	return 0;
}

int main(int argc, char **argv) {
	int action = -1;
	int main_socket;
	struct sockaddr_in server_bind_address;
	
	FILE *fw_fd = NULL;
	
	logger_init(stderr, LOGLEVEL_INFO);
	
	if(argc < 4 || (action = convert_action(argv[3])) < 0 || (argc - 4) != action_metadata_list[action].parameter_qty) {
		printf("Usage: %s device_id mac_password action [action_parameters]\n\n", argv[0]);
		printf("Actions:\n");
		printf("\t sampling_start\n");
		printf("\t sampling_pause\n");
		printf("\t config_write key value\n");
		printf("\t config_read key\n");
		printf("\t restart\n");
		printf("\t fw_update filename\n");
		printf("\t status\n");
		printf("\t get_data type source quantity\n");
		printf("\t plot_waveform channel samples\n");
		return -1;
	}
	
	if(action == ACT_FW_UPDATE) {
		fw_fd = fopen(argv[4], "r");
		
		if(fw_fd == NULL) {
			printf("Error opening file %s.\n", argv[4]);
			return -1;
		}
	}
	
	main_socket = socket(PF_INET, SOCK_STREAM, 0);
	if(main_socket < 0) {
		fprintf(stderr, "Unable to create main socket.\n");
		return -1;
	}
	
	setsockopt(main_socket, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));
	
	bzero(&server_bind_address, sizeof(server_bind_address));
	server_bind_address.sin_family      = AF_INET;
	server_bind_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_bind_address.sin_port        = htons(COMM_SERVER_PORT);
	
	if(bind(main_socket, (struct sockaddr *) &server_bind_address, sizeof(server_bind_address)) < 0) {
		fprintf(stderr, "Unable to bind main socket.\n");
		return -1;
	}
	
	if(listen(main_socket, 2) < 0) {
		fprintf(stderr, "Unable to put main socket in listening mode.\n");
		return -1;
	}
	
	printf("Waiting for device connection on port %d...\n", COMM_SERVER_PORT);
	fflush(stdin);
	
	gnuplot_ctrl *waveform_plot = NULL;
	
	int client_socket;
	struct sockaddr_in client_address;
	unsigned int client_address_size = sizeof(struct sockaddr_in);
	
	br_hmac_key_context hmac_key_ctx;
	
	int counter = 0;
	time_t aux_timestamp;
	time_t aux_time;
	unsigned int uptime;
	unsigned int aux_qty;
	unsigned int status_qty;
	unsigned int aux_channel;
	
	int aux_data_type;
	int command_result;
	char aux[200];
	char received_parameters[PARAM_MAX_QTY][PARAM_STR_SIZE];
	
	float waveform_buffer[WAVEFORM_MAX_QTY];
	
	br_hmac_key_init(&hmac_key_ctx, &br_md5_vtable, argv[2], strlen(argv[2]));
	
	while(1) {
		client_socket = accept(main_socket, (struct sockaddr *) &client_address, &client_address_size);
		if(client_socket < 0) {
			fprintf(stderr, "Unable to accept connection.\n");
			return -1;
		}
		
		printf("Received connection from %s\n", inet_ntoa(client_address.sin_addr));
		fflush(stdin);
		
		struct timeval socket_timeout_value = {.tv_sec = 2, .tv_usec = 0};
		if(setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&socket_timeout_value, sizeof(socket_timeout_value)) < 0)
			fprintf(stderr, "Unable to set socket timeout value.\n");
		
		counter = 0;
		
		if((command_result = send_comand_and_receive_response(client_socket, &hmac_key_ctx, OP_PROTOCOL_START, counter++, NULL, received_parameters, 3))) {
			fprintf(stderr, "Error sending OP_PROTOCOL_START command. (%d)\n", command_result);
			close(client_socket);
			return -1;
		}
		
		if(strcmp(received_parameters[1], argv[1]) == 0)
			break;
		
		send_comand_and_receive_response(client_socket, &hmac_key_ctx, OP_DISCONNECT, counter++, "2000\t", NULL, 0);
		shutdown(client_socket, SHUT_RDWR);
		close(client_socket);
		printf("Wrong device, disconneting...\n");
	}
	
	printf("Device type: %s\n", received_parameters[0]);
	printf("Device firmware version: %s\n\n", received_parameters[2]);
	
	switch(action) {
		case ACT_START_SAMPLING:
			if((command_result = send_comand_and_receive_response(client_socket, &hmac_key_ctx, OP_SAMPLING_START, counter++, NULL, NULL, 0))) {
				fprintf(stderr, "Error sending OP_SAMPLING_START command. (%d)\n", command_result);
				close(client_socket);
			}
			break;
		case ACT_PAUSE_SAMPLING:
			if((command_result = send_comand_and_receive_response(client_socket, &hmac_key_ctx, OP_SAMPLING_PAUSE, counter++, NULL, NULL, 0))) {
				fprintf(stderr, "Error sending OP_SAMPLING_PAUSE command. (%d)\n", command_result);
				close(client_socket);
			}
			break;
		case ACT_WRITE_CONFIG:
			if(strlen(argv[4]) >= PARAM_STR_SIZE) {
				fprintf(stderr, "Config key too long.\n");
				break;
			}
			
			if(strlen(argv[5]) >= PARAM_STR_SIZE) {
				fprintf(stderr, "Config value too long.\n");
				break;
			}
			
			sprintf(aux, "%s\t%s\t", argv[4], argv[5]);
			
			if((command_result = send_comand_and_receive_response(client_socket, &hmac_key_ctx, OP_CONFIG_WRITE, counter++, aux, NULL, 0))) {
				fprintf(stderr, "Error sending OP_CONFIG_WRITE command. (%d)\n", command_result);
				close(client_socket);
				break;
			}
			
			printf("Set %s = %s.\n", argv[4], argv[5]);
			
			break;
		case ACT_READ_CONFIG:
			if(strlen(argv[4]) >= PARAM_STR_SIZE) {
				fprintf(stderr, "Config key too long.\n");
				break;
			}
			
			sprintf(aux, "%s\t", argv[4]);
			if((command_result = send_comand_and_receive_response(client_socket, &hmac_key_ctx, OP_CONFIG_READ, counter++, aux, received_parameters, 1))) {
				fprintf(stderr, "Error sending OP_CONFIG_READ command. (%d)\n", command_result);
				close(client_socket);
				break;
			}
			
			printf("%s = %s\n", argv[4], received_parameters[0]);
			
			break;
		case ACT_RESTART_DEVICE:
			if((command_result = send_comand_and_receive_response(client_socket, &hmac_key_ctx, OP_RESTART, counter++, NULL, NULL, 0))) {
				fprintf(stderr, "Error sending OP_RESTART command. (%d)\n", command_result);
				close(client_socket);
			}
			break;
		case ACT_FW_UPDATE:
			action_fw_update(client_socket, &hmac_key_ctx, &counter, fw_fd);
			break;
		case ACT_READ_STATUS:
			if((command_result = send_comand_and_receive_response(client_socket, &hmac_key_ctx, OP_QUERY_STATUS, counter++, NULL, received_parameters, 11))) {
				fprintf(stderr, "Error sending OP_QUERY_STATUS command. (%d)\n", command_result);
				close(client_socket);
				break;
			}
			
			sscanf(received_parameters[1], "%u", &uptime);
			sscanf(received_parameters[3], "%ld", &aux_time);
			
			printf("Sampling state: %s (%s)\n", (received_parameters[0][0] == '0') ? "PAUSED" : "RUNNING", received_parameters[0]);
			printf("Uptime: %uh %02um %02us\n", uptime / 3600, (uptime % 3600) / 60, uptime % 60);
			printf("Temperature: %s C\n", received_parameters[2]);
			printf("RTC time: %s = %s", received_parameters[3], ctime(&aux_time));
			printf("RTC oscillator stop count: %s\n", received_parameters[4]);
			printf("RAM:\n");
			printf("  Internal events: %s\n", received_parameters[5]);
			printf("  Power events: %s\n", received_parameters[6]);
			printf("  Power data: %s\n", received_parameters[7]);
			printf("Flash:\n");
			printf("  Internal events: %s\n", received_parameters[8]);
			printf("  Power events: %s\n", received_parameters[9]);
			printf("  Power data: %s\n", received_parameters[10]);
			
			break;
		case ACT_GET_DATA:
			if(!strcmp(argv[6], "all")) {
					aux_qty = 10000;
			} else if(sscanf(argv[6], "%u", &aux_qty) != 1) {
				fprintf(stderr, "Invalid quantity argument.\n");
				break;
			}
			
			if(!strcmp(argv[4], "ie")) {
				aux_data_type = DATA_TYPE_INTERNAL_EVENT;
			} else if(!strcmp(argv[4], "pe")) {
				aux_data_type = DATA_TYPE_POWER_EVENT;
			} else if(!strcmp(argv[4], "pd")) {
				aux_data_type = DATA_TYPE_POWER_DATA;
			} else {
				fprintf(stderr, "Invalid type argument.\n");
				break;
			}
			
			if(*argv[5] == 'f')
				aux_data_type++;
			
			if((command_result = send_comand_and_receive_response(client_socket, &hmac_key_ctx, OP_QUERY_STATUS, counter++, NULL, received_parameters, 11))) {
				fprintf(stderr, "Error sending OP_QUERY_STATUS command. (%d)\n", command_result);
				close(client_socket);
				break;
			}
			
			sscanf(received_parameters[data_type_status_index[aux_data_type]], "%u", &status_qty);
			
			aux_qty = MIN(aux_qty, status_qty);
			
			if(aux_qty == 0) {
				printf("No data.\n");
				break;
			}
			
			sprintf(aux, "%s\t%s\t%u\t", argv[4], argv[5], aux_qty);
			if((command_result = send_command(client_socket, &hmac_key_ctx, OP_GET_DATA, &aux_timestamp, counter, aux))) {
				fprintf(stderr, "Error sending OP_GET_DATA command. (%d)\n", command_result);
				close(client_socket);
				break;
			}
			
			for(int i = 0; i < aux_qty; i++) {
				if((command_result = receive_response(client_socket, &hmac_key_ctx, OP_GET_DATA, aux_timestamp, counter, NULL, received_parameters, data_type_response_parameter_qty[aux_data_type]))) {
					fprintf(stderr, "Error receiving OP_GET_DATA response. (%d)\n", command_result);
					close(client_socket);
					break;
				}
				
				sscanf(received_parameters[0], "%ld", &aux_time);
				
				printf("--------------------------------------------------------------------------\n");
				printf("Timestamp: %s = %s", received_parameters[0], ctime(&aux_time));
				
				if(aux_data_type == DATA_TYPE_POWER_DATA) {
					printf("Samples: %s\n", received_parameters[1]);
					printf("Duration: %s us\n", received_parameters[2]);
					print_power_data(received_parameters[3], received_parameters[4], received_parameters[5], received_parameters[6], received_parameters[7], received_parameters[8], received_parameters[9], received_parameters[10], received_parameters[11]);
				} else if(aux_data_type == DATA_TYPE_POWER_DATA_FLASH) {
					printf("Seconds: %s\n", received_parameters[1]);
					printf("A: %s Wh | %s VArh\n", received_parameters[2], received_parameters[5]);
					printf("B: %s Wh | %s VArh\n", received_parameters[3], received_parameters[6]);
					printf("C: %s Wh | %s VArh\n", received_parameters[4], received_parameters[7]);
				} else if(aux_data_type == DATA_TYPE_POWER_EVENT || aux_data_type == DATA_TYPE_POWER_EVENT_FLASH) {
					printf("Type: %s\n", received_parameters[1]);
					printf("Count: %s\n", received_parameters[2]);
					printf("Channel: %s\n", received_parameters[3]);
					printf("Average Value: %s\n", received_parameters[4]);
					printf("Worst Value: %s\n", received_parameters[5]);
				} else {
					printf("Type: %s\n", received_parameters[1]);
					printf("Count: %s\n", received_parameters[2]);
					printf("Value: %s\n", received_parameters[3]);
				}
			}
			
			counter++;
			
			if((command_result = send_comand_and_receive_response(client_socket, &hmac_key_ctx, OP_DELETE_DATA, counter++, aux, NULL, 0))) {
				fprintf(stderr, "Error sending OP_DELETE_DATA command. (%d)\n", command_result);
				close(client_socket);
				break;
			}
			break;
		case ACT_PLOT_WAVEFORM:
			waveform_plot = gnuplot_init();

			if(sscanf(argv[4], "%u", &aux_channel) != 1 || sscanf(argv[5], "%u", &aux_qty) != 1 || aux_qty > WAVEFORM_MAX_QTY) {
				fprintf(stderr, "Invalid argument.\n");
				break;
			}
			
			sprintf(aux, "%u\t%u\t", aux_channel, aux_qty);
			if((command_result = send_command(client_socket, &hmac_key_ctx, OP_GET_WAVEFORM, &aux_timestamp, counter, aux))) {
				fprintf(stderr, "Error sending OP_GET_WAVEFORM command. (%d)\n", command_result);
				close(client_socket);
				break;
			}
			
			for(int i = 0; i < aux_qty; i++) {
				if((command_result = receive_response(client_socket, &hmac_key_ctx, OP_GET_WAVEFORM, aux_timestamp, counter, NULL, received_parameters, 1))) {
					fprintf(stderr, "Error receiving OP_GET_WAVEFORM response. (%d)\n", command_result);
					close(client_socket);
					break;
				}
				
				sscanf(received_parameters[0], "%f", &waveform_buffer[i]);
			}
			
			counter++;
			
			gnuplot_command(waveform_plot, "set terminal x11 noraise");
			gnuplot_command(waveform_plot, "set autoscale");
			gnuplot_command(waveform_plot, "set grid");
			gnuplot_command(waveform_plot, "set ytics");
			
			sprintf(aux, "Channel %d", aux_channel);
			
			gnuplot_plot_data(waveform_plot, NULL, waveform_buffer, aux_qty, aux, NULL, "lp");
			
			break;
		default:
			break;
	}
	
	if(action != ACT_FW_UPDATE && action != ACT_RESTART_DEVICE) {
		if((command_result = send_comand_and_receive_response(client_socket, &hmac_key_ctx, OP_DISCONNECT, counter++, "1000\t", NULL, 0)))
			fprintf(stderr, "Error sending OP_DISCONNECT command. (%d)\n", command_result);
		shutdown(client_socket, SHUT_RDWR);
		close(client_socket);
	}
	
	shutdown(main_socket, SHUT_RDWR);
	close(main_socket);
	
	gnuplot_close(waveform_plot);
	
	
	return 0;
}
