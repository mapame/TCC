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
#include "logger.h"

static const char opcode_text[OPCODE_NUM][3] = {
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


static const char comm_status_text_invalid[] = "Invalid status";
static const char comm_status_text[COMM_STATUS_NUM][32] = {
	"COMM_OK",
	"COMM_ERR_INVALID_CLIENT_CTX",
	"COMM_ERR_SENDING_COMMAND",
	"COMM_ERR_RECEVING_RESPONSE",
	"COMM_ERR_INVALID_MAC",
	"COMM_ERR_WRONG_RESPONSE",
	"COMM_ERR_PARSING_RESPONSE",
	"COMM_ERR_RESPONSE_CODE"
};


static int convert_opcode(char *buf);
static void compute_hmac(const br_hmac_key_context *hmac_key_ctx, char *output_mac_text, const char *data, size_t len);
static int validate_hmac(const br_hmac_key_context *hmac_key_ctx, char *data, size_t len);
static int recv_command_line(int socket_fd, char *buf, size_t len);
static int parse_response(char *receive_buffer, int op, time_t timestamp, unsigned int counter, int *response_code, char **parameters);
static int parse_parameters(char *parameter_buffer, char parsed_parameters[][PARAM_STR_SIZE], unsigned int parameter_qty);

const char * get_comm_status_text(comm_status_t status) {
	if(status >= COMM_STATUS_NUM)
		return comm_status_text_invalid;
	
	return comm_status_text[status];
}

int comm_create_main_socket(int reuse_addr) {
	int socket_fd;
	struct sockaddr_in bind_addr;
	
	socket_fd = socket(PF_INET, SOCK_STREAM, 0);
	if(socket_fd < 0) {
		LOG_ERROR("Failed to create main socket.\n");
		return -1;
	}
	
	if(reuse_addr)
		setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));
	
	bzero(&bind_addr, sizeof(bind_addr));
	bind_addr.sin_family      = AF_INET;
	bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind_addr.sin_port        = htons(COMM_SERVER_PORT);
	
	if(bind(socket_fd, (struct sockaddr *) &bind_addr, sizeof(bind_addr)) < 0) {
		LOG_ERROR("Failed to bind main socket.\n");
		return -1;
	}
	
	if(listen(socket_fd, 2) < 0) {
		LOG_ERROR("Failed to put main socket in listening mode.\n");
		return -1;
	}
	
	return socket_fd;
}

int comm_accept_client(int main_socket_fd, comm_client_ctx *ctx, const char *hmac_key) {
	unsigned int addr_size = sizeof(struct sockaddr_in);
	struct timeval rcv_timeout_value = {.tv_sec = 2, .tv_usec = 0};
	int result;
	char received_parameters[PARAM_MAX_QTY][PARAM_STR_SIZE];
	
	if(main_socket_fd < 0)
		return -1;
	
	if(ctx == NULL || hmac_key == NULL)
		return -2;
	
	ctx->socket_fd = accept(main_socket_fd, (struct sockaddr *) &(ctx->address), &addr_size);
	
	if(ctx->socket_fd < 0) {
		LOG_ERROR("Failed to accept new client connection.");
		return -3;
	}
	
	if(setsockopt(ctx->socket_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&rcv_timeout_value, sizeof(rcv_timeout_value)) < 0)
		LOG_WARN("Failed to set receive timeout to client socket.");
	
	br_hmac_key_init(&(ctx->hmac_key_ctx), &br_md5_vtable, hmac_key, strlen(hmac_key));
	
	ctx->counter = 0;
	
	if((result = send_comand_and_receive_response(ctx, OP_PROTOCOL_START, NULL, received_parameters, 1))) {
		LOG_ERROR("Error sending OP_PROTOCOL_START command: %s\n", get_comm_status_text(result));
		shutdown(ctx->socket_fd, SHUT_RDWR);
		close(ctx->socket_fd);
		ctx->socket_fd = -1;
		return -4;
	}
	
	strcpy(ctx->version, received_parameters[0]);
	
	return 0;
}

int send_comand_and_receive_response(comm_client_ctx *client_ctx, int op, const char *command_parameters, char response_parameters[][PARAM_STR_SIZE], unsigned int expected_parameter_qty) {
	int result;
	
	if((result = send_command(client_ctx, op, command_parameters)))
		return result;
	
	result = receive_response(client_ctx, op, NULL, response_parameters, expected_parameter_qty);
	
	client_ctx->counter++;
	
	return result;
}

int receive_response(comm_client_ctx *client_ctx, int op, int *response_code, char response_parameters[][PARAM_STR_SIZE], unsigned int expected_parameter_qty) {
	char receive_buffer[200];
	int received_line_len;
	
	int received_response_code;
	int result;
	
	char *response_parameters_ptr;
	
	if(client_ctx == NULL || client_ctx->socket_fd < 0)
		return COMM_ERR_INVALID_CLIENT_CTX;
	
	received_line_len = recv_command_line(client_ctx->socket_fd, receive_buffer, 200);
	
	if(received_line_len <= 0) // Timeout or disconnection
		return COMM_ERR_RECEVING_RESPONSE;
	
	LOG_TRACE("Recv: %s\n", receive_buffer);
	
	if(validate_hmac(&(client_ctx->hmac_key_ctx), receive_buffer, received_line_len))
		return COMM_ERR_INVALID_MAC;
	
	if((result = parse_response(receive_buffer, op, client_ctx->last_timestamp, client_ctx->counter, &received_response_code, &response_parameters_ptr)))
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

int send_command(comm_client_ctx *client_ctx, int op, const char *parameters) {
	char aux[36];
	char send_buffer[200];
	char computed_mac_text[33];
	
	if(client_ctx == NULL || client_ctx->socket_fd < 0)
		return COMM_ERR_INVALID_CLIENT_CTX;
	
	client_ctx->last_timestamp = time(NULL);
	
	sprintf(send_buffer, "%s:%ld:%u:", opcode_text[op], client_ctx->last_timestamp, client_ctx->counter);
	
	if(parameters)
		strlcat(send_buffer, parameters, 200);
	
	compute_hmac(&(client_ctx->hmac_key_ctx), computed_mac_text, send_buffer, strlen(send_buffer));
	
	sprintf(aux, "*%s\n", computed_mac_text);
	strlcat(send_buffer, aux, 200);
	
	if(send(client_ctx->socket_fd, send_buffer, strlen(send_buffer), 0) <= 0)
		return COMM_ERR_SENDING_COMMAND;
	
	return COMM_OK;
}

static int parse_response(char *receive_buffer, int op, time_t timestamp, unsigned int counter, int *response_code, char **parameters) {
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
	
	if(received_code)
		LOG_DEBUG("Received response with error code %d.\n", received_code);
	
	if(response_code)
		*response_code = received_code;
	
	if(parameters != NULL)
		*parameters = strtok_r(NULL, ":", &saveptr); // Parameters
	
	return COMM_OK;
}

static int parse_parameters(char *parameter_buffer, char parsed_parameters[][PARAM_STR_SIZE], unsigned int parameter_qty) {
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
	
	LOG_TRACE("Received HMAC:   %s\n", received_mac_text);
	
	compute_hmac(hmac_key_ctx, computed_mac_text, data, len - 33);
	
	LOG_TRACE("Calculated HMAC: %s\n", computed_mac_text);
	
	if(strcmp(received_mac_text, computed_mac_text)) // Protocol error - Invalid MAC
		return -2;
	
	return 0;
}
