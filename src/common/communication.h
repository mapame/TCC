#define PARAM_STR_SIZE 65
#define PARAM_MAX_QTY 16

typedef enum {
	COMM_OK,
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
	OP_FW_UPDATE,
	OP_QUERY_STATUS,
	OP_GET_DATA,
	OP_DELETE_DATA,
	OP_GET_WAVEFORM,
	OP_DISCONNECT,
	OPCODE_NUM
} protocol_opcode_t;

const char * get_comm_status_text(comm_status_t status);
int send_command(int socket_fd, const br_hmac_key_context *hmac_key_ctx, int op, time_t *timestamp_ptr, unsigned int counter, const char *parameters);
int receive_response(int socket_fd, const br_hmac_key_context *hmac_key_ctx, int op, time_t timestamp, unsigned int counter, int *response_code, char response_parameters[][PARAM_STR_SIZE], unsigned int expected_parameter_qty);
int send_comand_and_receive_response(int socket_fd, const br_hmac_key_context *hmac_key_ctx, int op, unsigned int counter, const char *command_parameters, char response_parameters[][PARAM_STR_SIZE], unsigned int expected_parameter_qty);
int parse_response(char *receive_buffer, int op, time_t timestamp, unsigned int counter, int *response_code, char **parameters);
