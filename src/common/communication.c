#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bsd/string.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "bearssl.h"

#include "communication.h"

#define DEBUG

#ifdef DEBUG
#define debug(fmt, ...) printf(fmt, ## __VA_ARGS__)
#else
#define debug(fmt, ...)
#endif

char opcode_text[OPCODE_NUM][3] = {
	"HE",
	"SS",
	"SP",
	"CW",
	"CR",
	"RE",
	"FU",
	"QS",
	"GD",
	"DD",
	"GW",
	"BY"
};

static int convert_opcode(char *buf);
static void compute_hmac(const br_hmac_key_context *hmac_key_ctx, char *output_mac_text, const char *data, size_t len);
static int validate_hmac(const br_hmac_key_context *hmac_key_ctx, char *data, size_t len);
static int recv_command_line(int socket_fd, char *buf, size_t len);
int parse_parameters(char *parameter_buffer, char parsed_parameters[][PARAM_STR_SIZE], unsigned int parameter_qty);

int send_comand_and_receive_response(int socket_fd, const br_hmac_key_context *hmac_key_ctx, int op, unsigned int counter, const char *command_parameters, char response_parameters[][PARAM_STR_SIZE], unsigned int expected_parameter_qty) {
	time_t timestamp;
	int send_result;
	
	if((send_result = send_command(socket_fd, hmac_key_ctx, op, &timestamp, counter, command_parameters)))
		return send_result;
	
	return receive_response(socket_fd, hmac_key_ctx, op, timestamp, counter, NULL, response_parameters, expected_parameter_qty);
}

int receive_response(int socket_fd, const br_hmac_key_context *hmac_key_ctx, int op, time_t timestamp, unsigned int counter, int *response_code, char response_parameters[][PARAM_STR_SIZE], unsigned int expected_parameter_qty) {
	char receive_buffer[200];
	int received_line_len;
	
	int received_response_code;
	int result;
	
	char *response_parameters_ptr;
	
	received_line_len = recv_command_line(socket_fd, receive_buffer, 200);
	
	if(received_line_len <= 0) // Timeout or disconnection
		return COMM_ERR_RECEVING_RESPONSE;
	
	//debug("Recv: %s\n", receive_buffer);
	
	if(validate_hmac(hmac_key_ctx, receive_buffer, received_line_len))
		return COMM_ERR_INVALID_MAC;
	
	if((result = parse_response(receive_buffer, op, timestamp, counter, &received_response_code, &response_parameters_ptr)))
		return result;
	
	if(response_code)
		*response_code = received_response_code;
	
	if(received_response_code)
		return COMM_ERR_RESPONSE_CODE;
	
	if(response_parameters != NULL && expected_parameter_qty) {
		if(parse_parameters(response_parameters_ptr, response_parameters, expected_parameter_qty) != expected_parameter_qty)
			return COMM_ERR_PARSING_RESPONSE;
	}
	
	return COMM_OK;
}

int send_command(int socket_fd, const br_hmac_key_context *hmac_key_ctx, int op, time_t *timestamp_ptr, unsigned int counter, const char *parameters) {
	time_t timestamp;
	char aux[36];
	char send_buffer[200];
	char computed_mac_text[33];
	
	timestamp = time(NULL);
	
	if(timestamp_ptr)
		*timestamp_ptr = timestamp;
	
	sprintf(send_buffer, "%s:%ld:%u:", opcode_text[op], timestamp, counter);
	
	if(parameters)
		strlcat(send_buffer, parameters, 200);
	
	compute_hmac(hmac_key_ctx, computed_mac_text, send_buffer, strlen(send_buffer));
	
	sprintf(aux, "*%s\n", computed_mac_text);
	strlcat(send_buffer, aux, 200);
	
	if(send(socket_fd, send_buffer, strlen(send_buffer), 0) <= 0)
		return COMM_ERR_SENDING_COMMAND;
	
	return COMM_OK;
}

int parse_response(char *receive_buffer, int op, time_t timestamp, unsigned int counter, int *response_code, char **parameters) {
	char *token;
	char *saveptr;
	
	time_t received_timestamp;
	unsigned int received_counter;
	int received_code;
	
	token = strtok_r(receive_buffer, ":", &saveptr); // Response prefix "A"
	if(token == NULL || token[0] != 'A')
		return COMM_ERR_PARSING_RESPONSE;
	
	token = strtok_r(NULL, ":", &saveptr); // Opcode
	if(token == NULL)
		return COMM_ERR_PARSING_RESPONSE;
	
	if(convert_opcode(token) != op)
		return COMM_ERR_WRONG_RESPONSE;
	
	token = strtok_r(NULL, ":", &saveptr); // Timestamp
	if(token == NULL)
		return COMM_ERR_PARSING_RESPONSE;
	
	if(sscanf(token, "%ld", &received_timestamp) != 1)
		return COMM_ERR_PARSING_RESPONSE;
	
	if(timestamp != received_timestamp)
		return COMM_ERR_WRONG_RESPONSE;
	
	token = strtok_r(NULL, ":", &saveptr); // Counter
	if(token == NULL)
		return COMM_ERR_PARSING_RESPONSE;
	
	if(sscanf(token, "%u", &received_counter) != 1)
		return COMM_ERR_PARSING_RESPONSE;
	
	if(counter != received_counter)
		return COMM_ERR_WRONG_RESPONSE;
	
	token = strtok_r(NULL, ":", &saveptr); // Response Code
	if(token == NULL)
		return COMM_ERR_PARSING_RESPONSE;
	
	if(sscanf(token, "%d", &received_code) != 1)
		return COMM_ERR_PARSING_RESPONSE;
	
	#ifdef DEBUG
		if(received_code)
			debug("Received response with error code %d.\n", received_code);
	#endif
	
	if(response_code)
		*response_code = received_code;
	
	if(parameters != NULL)
		*parameters = strtok_r(NULL, ":", &saveptr); // Parameters
	
	return COMM_OK;
}

int parse_parameters(char *parameter_buffer, char parsed_parameters[][PARAM_STR_SIZE], unsigned int parameter_qty) {
	char *buffer_ptr = parameter_buffer;
	char *token;
	char *saveptr;
	
	int count = 0;
	
	if(parameter_buffer == NULL)
		return 0;
	
	while((token = strtok_r(buffer_ptr, "\t", &saveptr)) && count < parameter_qty && count < PARAM_MAX_QTY) {
		if(strlen(token) >= (PARAM_STR_SIZE - 1))
			break;
		
		strcpy(parsed_parameters[count], token);
		count++;
		buffer_ptr = NULL;
	}
	
	return count;
}

static int recv_command_line(int socket_fd, char *buf, size_t len) {
	int num = 0;
	
	do {
		char c;
		
		if (recv(socket_fd, &c, 1, 0) <= 0)
			return -1;
		
		if (c == '\n')
			break;
		
		if (num < len)
            buf[num] = c;
		
		num++;
	} while(1);
	
	buf[(num >= len) ? len - 1 : num] = 0; // Null terminate
	
	return num;
}

static int convert_opcode(char *buf) {
	if(!buf)
		return -1;
	
	for(int i = 0; i < OPCODE_NUM; i++)
		if(!strcmp(buf, opcode_text[i]))
			return i;
	
	return -1;
}

static void compute_hmac(const br_hmac_key_context *hmac_key_ctx, char *output_mac_text, const char *data, size_t len) {
	br_hmac_context hmac_ctx;
	uint8_t computed_mac[16];
	char aux[3];
	
	br_hmac_init(&hmac_ctx, hmac_key_ctx, 0);
	
	br_hmac_update(&hmac_ctx, data, len);
	
	br_hmac_out(&hmac_ctx, computed_mac);
	
	output_mac_text[0] = '\0';
	
	for(int i = 0; i < 16; i++) {
		sprintf(aux, "%02hx", computed_mac[i]);
		strlcat(output_mac_text, aux, 33);
	}
}

static int validate_hmac(const br_hmac_key_context *hmac_key_ctx, char *data, size_t len) {
	char *received_mac_text;
	char computed_mac_text[33];
	
	if(data[len - 33] != '*') // Protocol error - Syntax Error
		return -1;
	
	data[len - 33] = '\0';
	
	received_mac_text = &(data[len - 32]);
	
	if(strlen(received_mac_text) != 32)
		return -1;
	
	//debug("Received HMAC:   %s\n", received_mac_text);
	
	compute_hmac(hmac_key_ctx, computed_mac_text, data, len - 33);
	
	//debug("Calculated HMAC: %s\n", computed_mac_text);
	
	if(strcmp(received_mac_text, computed_mac_text)) // Protocol error - Invalid MAC
		return -2;
	
	return 0;
}
