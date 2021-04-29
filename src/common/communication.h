#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <arpa/inet.h>

#define COMM_SERVER_PORT 2048

#define PARAM_STR_SIZE 65
#define PARAM_MAX_QTY 16

typedef enum {
	COMM_OK,
	COMM_ERR_INVALID_CLIENT_CTX,
	COMM_ERR_SENDING_COMMAND,
	COMM_ERR_RECEVING_RESPONSE,
	COMM_ERR_INVALID_MAC,
	COMM_ERR_WRONG_RESPONSE,
	COMM_ERR_PARSING_RESPONSE,
	COMM_ERR_RESPONSE_CODE,
	COMM_STATUS_NUM
} comm_status_t;

typedef enum {
	OP_PROTOCOL_START,
	OP_SAMPLING_START,
	OP_SAMPLING_PAUSE,
	OP_CONFIG_WRITE,
	OP_CONFIG_READ,
	OP_RESTART,
	OP_SET_RTC,
	OP_FW_UPDATE,
	OP_QUERY_STATUS,
	OP_GET_DATA,
	OP_DELETE_DATA,
	OP_GET_WAVEFORM,
	OP_DISCONNECT,
	OPCODE_NUM
} protocol_opcode_t;

typedef struct comm_client_ctx_s {
	int socket_fd;
	struct sockaddr_in address;
	unsigned int counter;
	uint32_t self_rndn;
	uint32_t client_rndn;
	char version[16];
	char hmac_key[128];
} comm_client_ctx;

const char * get_comm_status_text(comm_status_t status);
int comm_create_main_socket(int reuse_addr);
int comm_accept_client(int main_socket_fd, comm_client_ctx *ctx, const char *hmac_key, int *terminate);
int send_command(comm_client_ctx *client_ctx, int op, const char *parameters);
int receive_response(comm_client_ctx *client_ctx, int op, int *response_code, char response_parameters[][PARAM_STR_SIZE], unsigned int expected_parameter_qty);
int send_comand_and_receive_response(comm_client_ctx *client_ctx, int op, const char *command_parameters, char response_parameters[][PARAM_STR_SIZE], unsigned int expected_parameter_qty);


#endif
