#define TFTP_RECV_TIMEOUT_SECS 3
#define TFTP_SEND_ATTEMPTS 5

typedef union {
	uint16_t opcode;
	
	struct {
		uint16_t opcode;           
		uint8_t filename_and_mode_string[514];
	} request;     
	
	struct {
		uint16_t opcode;
		uint16_t block_number;
		uint8_t data[512];
	} data;
	
	struct {
		uint16_t opcode;           
		uint16_t block_number;
	} ack;
	
	struct {
		uint16_t opcode;  
		uint16_t error_code;
		uint8_t error_string[512];
	} error;
} tftp_message;

typedef enum {
	TFTP_OP_RRQ = 1,
	TFTP_OP_WRQ,
	TFTP_OP_DATA,
	TFTP_OP_ACK,
	TFTP_OP_ERROR
} tftp_opcode;

int tftp_serve_file(FILE *fd, unsigned int port);
