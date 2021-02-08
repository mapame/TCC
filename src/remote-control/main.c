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
#include "communication.h"
#include "tftp.h"

#define TFTP_PORT 6969


typedef enum {
	ACT_START_SAMPLING,
	ACT_PAUSE_SAMPLING,
	ACT_WRITE_CONFIG,
	ACT_READ_CONFIG,
	ACT_RESTART_DEVICE,
	ACT_FW_UPDATE,
	ACT_READ_STATUS,
	ACT_READ_ADV_STATUS,
	ACT_GET_DATA,
	ACTION_NUM
} protocol_opcodes_t;


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
	{"status_adv",			0},
	{"get_data",			2}
};


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

int action_fw_update(comm_client_ctx *client_ctx, FILE *fd) {
	char hash_result_text[33];
	unsigned int file_size;
	int command_result;
	char received_parameters[PARAM_MAX_QTY][PARAM_STR_SIZE];
	char aux[100];
	
	file_size = calculate_file_md5(fd, hash_result_text);
	
	printf("Firmware file MD5 hash: %s\n", hash_result_text);
	printf("File size: %u bytes\n", file_size);
	
	if((command_result = send_comand_and_receive_response(client_ctx, OP_QUERY_STATUS, "A\t", received_parameters, 1))) {
		fprintf(stderr, "Error sending query status command: %s\n", get_comm_status_text(command_result));
		close(client_ctx->socket_fd);
		return -1;
	}
	
	if(strcmp(received_parameters[0], "0")) {
		if((command_result = send_comand_and_receive_response(client_ctx, OP_SAMPLING_PAUSE, NULL, NULL, 0))) {
			fprintf(stderr, "Error sending pause command: %s\n", get_comm_status_text(command_result));
			close(client_ctx->socket_fd);
			return -1;
		}
	}
	
	sprintf(aux, "%s\t", hash_result_text);
	
	if((command_result = send_comand_and_receive_response(client_ctx, OP_FW_UPDATE, aux, NULL, 0))) {
		fprintf(stderr, "Error sending firmware update command: %s\n", get_comm_status_text(command_result));
		close(client_ctx->socket_fd);
		return -1;
	}
	
	close(client_ctx->socket_fd);
	
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
	
	FILE *fw_fd = NULL;
	
	comm_client_ctx client_ctx;
	
	char aux_str[100];
	time_t aux_time;
	struct tm aux_tm;
	unsigned int uptime;
	unsigned int aux_qty;
	unsigned int status_qty;
	
	int is_event;
	int command_result;
	char txparam[200];
	char received_parameters[PARAM_MAX_QTY][PARAM_STR_SIZE];
	
	
	if(argc < 3 || (action = convert_action(argv[2])) < 0 || (argc - 3) != action_metadata_list[action].parameter_qty) {
		printf("Usage: %s mac_key action [action_parameters]\n\n", argv[0]);
		printf("Actions:\n");
		printf("\t sampling_start\n");
		printf("\t sampling_pause\n");
		printf("\t config_write key value\n");
		printf("\t config_read key\n");
		printf("\t restart\n");
		printf("\t fw_update filename\n");
		printf("\t status\n");
		printf("\t get_data type quantity\n");
		return -1;
	}
	
	if(action == ACT_FW_UPDATE) {
		fw_fd = fopen(argv[3], "r");
		
		if(fw_fd == NULL) {
			fprintf(stderr, "Error opening file %s.\n", argv[3]);
			return -1;
		}
	}
	
	main_socket = comm_create_main_socket(1);
	
	if(main_socket < 0) {
		fprintf(stderr, "Failed to create/setup socket.\n");
		return -1;
	}
	
	printf("Waiting for device connection on port %d...\n", COMM_SERVER_PORT);
	
	if(comm_accept_client(main_socket, &client_ctx, argv[1]) < 0) {
		fprintf(stderr, "Failed to accept new client connection.\n");
		return -1;
	}
	
	printf("Received connection from %s\n", inet_ntoa(client_ctx.address.sin_addr));
	printf("Device firmware version: %s\n\n", client_ctx.version);
	
	switch(action) {
		case ACT_START_SAMPLING:
			if((command_result = send_comand_and_receive_response(&client_ctx, OP_SAMPLING_START, NULL, NULL, 0))) {
				fprintf(stderr, "Error sending OP_SAMPLING_START command: %s\n", get_comm_status_text(command_result));
				close(client_ctx.socket_fd);
			}
			break;
		case ACT_PAUSE_SAMPLING:
			if((command_result = send_comand_and_receive_response(&client_ctx, OP_SAMPLING_PAUSE, NULL, NULL, 0))) {
				fprintf(stderr, "Error sending OP_SAMPLING_PAUSE command: %s\n", get_comm_status_text(command_result));
				close(client_ctx.socket_fd);
			}
			break;
		case ACT_WRITE_CONFIG:
			if(strlen(argv[3]) >= PARAM_STR_SIZE) {
				fprintf(stderr, "Config key too long.\n");
				break;
			}
			
			if(strlen(argv[4]) >= PARAM_STR_SIZE) {
				fprintf(stderr, "Config value too long.\n");
				break;
			}
			
			sprintf(txparam, "%s\t%s\t", argv[3], argv[4]);
			
			if((command_result = send_comand_and_receive_response(&client_ctx, OP_CONFIG_WRITE, txparam, NULL, 0))) {
				fprintf(stderr, "Error sending OP_CONFIG_WRITE command: %s\n", get_comm_status_text(command_result));
				close(client_ctx.socket_fd);
				break;
			}
			
			printf("Set %s = %s.\n", argv[3], argv[4]);
			
			break;
		case ACT_READ_CONFIG:
			if(strlen(argv[3]) >= PARAM_STR_SIZE) {
				fprintf(stderr, "Config key too long.\n");
				break;
			}
			
			sprintf(txparam, "%s\t", argv[3]);
			if((command_result = send_comand_and_receive_response(&client_ctx, OP_CONFIG_READ, txparam, received_parameters, 1))) {
				fprintf(stderr, "Error sending OP_CONFIG_READ command: %s\n", get_comm_status_text(command_result));
				close(client_ctx.socket_fd);
				break;
			}
			
			printf("%s = %s\n", argv[3], received_parameters[0]);
			
			break;
		case ACT_RESTART_DEVICE:
			if((command_result = send_comand_and_receive_response(&client_ctx, OP_RESTART, NULL, NULL, 0))) {
				fprintf(stderr, "Error sending OP_RESTART command: %s\n", get_comm_status_text(command_result));
				close(client_ctx.socket_fd);
			}
			break;
		case ACT_FW_UPDATE:
			action_fw_update(&client_ctx, fw_fd);
			break;
		case ACT_READ_STATUS:
			if((command_result = send_comand_and_receive_response(&client_ctx, OP_QUERY_STATUS, "A\t", received_parameters, 4))) {
				fprintf(stderr, "Error sending OP_QUERY_STATUS command: %s\n", get_comm_status_text(command_result));
				close(client_ctx.socket_fd);
				break;
			}
			
			sscanf(received_parameters[1], "%ld", &aux_time);
			localtime_r(&aux_time, &aux_tm);
			strftime(aux_str, 100, "%F %T UTC%Z", &aux_tm);
			
			printf("Sampling state: %s (%s)\n", (received_parameters[0][0] == '0') ? "PAUSED" : "RUNNING", received_parameters[0]);
			printf("RTC time: %s (%s)\n", aux_str, received_parameters[1]);
			printf("Event count: %s\n", received_parameters[2]);
			printf("Power data count: %s\n", received_parameters[3]);
			
			break;
		case ACT_READ_ADV_STATUS:
			if((command_result = send_comand_and_receive_response(&client_ctx, OP_QUERY_STATUS, "B\t", received_parameters, 3))) {
				fprintf(stderr, "Error sending OP_QUERY_STATUS command: %s\n", get_comm_status_text(command_result));
				close(client_ctx.socket_fd);
				break;
			}
			
			sscanf(received_parameters[0], "%u", &uptime);
			
			printf("Uptime: %uh %02um %02us\n", uptime / 3600, (uptime % 3600) / 60, uptime % 60);
			printf("Free Heap: %s bytes\n", received_parameters[1]);
			printf("Temperature: %s C\n", received_parameters[2]);
			
			break;
		case ACT_GET_DATA:
			if(!strcmp(argv[4], "all")) {
					aux_qty = 10000;
			} else if(sscanf(argv[4], "%u", &aux_qty) != 1) {
				fprintf(stderr, "Invalid quantity argument.\n");
				break;
			}
			
			if(!strcmp(argv[3], "event")) {
				is_event = 1;
			} else if(!strcmp(argv[3], "power")) {
				is_event = 0;
			} else {
				fprintf(stderr, "Invalid type, must be \"power\" or \"event\".\n");
				break;
			}
			
			if((command_result = send_comand_and_receive_response(&client_ctx, OP_QUERY_STATUS, "A\t", received_parameters, 4))) {
				fprintf(stderr, "Error sending OP_QUERY_STATUS command: %s\n", get_comm_status_text(command_result));
				close(client_ctx.socket_fd);
				break;
			}
			
			sscanf(received_parameters[(is_event ? 2 : 3)], "%u", &status_qty);
			
			aux_qty = MIN(aux_qty, status_qty);
			
			if(aux_qty == 0) {
				printf("No data.\n");
				break;
			}
			
			sprintf(txparam, "%c\t%u\t", (is_event ? 'E' : 'P'), aux_qty);
			if((command_result = send_command(&client_ctx, OP_GET_DATA, txparam))) {
				fprintf(stderr, "Error sending OP_GET_DATA command: %s\n", get_comm_status_text(command_result));
				close(client_ctx.socket_fd);
				break;
			}
			
			for(int i = 0; i < aux_qty; i++) {
				if((command_result = receive_response(&client_ctx, OP_GET_DATA, NULL, received_parameters, (is_event ? 2 : 12)))) {
					fprintf(stderr, "Error receiving OP_GET_DATA response: %s\n", get_comm_status_text(command_result));
					close(client_ctx.socket_fd);
					break;
				}
				
				sscanf(received_parameters[0], "%ld", &aux_time);
				localtime_r(&aux_time, &aux_tm);
				strftime(aux_str, 200, "%F %T UTC%Z", &aux_tm);
				
				printf("--------------------------------------------------------------------------\n");
				printf("Timestamp: %s (%s)\n", aux_str, received_parameters[0]);
				
				if(is_event) {
					printf("Text: %s\n", received_parameters[1]);
				} else {
					printf("Samples: %s\n", received_parameters[1]);
					printf("Duration: %s us\n", received_parameters[2]);
					print_power_data(received_parameters[3], received_parameters[4], received_parameters[5], received_parameters[6], received_parameters[7], received_parameters[8], received_parameters[9], received_parameters[10], received_parameters[11]);
				}
			}
			
			client_ctx.counter++;
			
			if((command_result = send_comand_and_receive_response(&client_ctx, OP_DELETE_DATA, txparam, NULL, 0))) {
				fprintf(stderr, "Error sending OP_DELETE_DATA command: %s\n", get_comm_status_text(command_result));
				close(client_ctx.socket_fd);
				break;
			}
			break;
		default:
			break;
	}
	
	if(action != ACT_FW_UPDATE && action != ACT_RESTART_DEVICE) {
		if((command_result = send_comand_and_receive_response(&client_ctx, OP_DISCONNECT, "1000\t", NULL, 0)))
			fprintf(stderr, "Error sending OP_DISCONNECT command: %s\n", get_comm_status_text(command_result));
		shutdown(client_ctx.socket_fd, SHUT_RDWR);
		close(client_ctx.socket_fd);
	}
	
	shutdown(main_socket, SHUT_RDWR);
	close(main_socket);
	
	return 0;
}
