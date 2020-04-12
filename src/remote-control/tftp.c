#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "tftp.h"

ssize_t tftp_send_error(int s, int code, char *string, struct sockaddr_in *addr, socklen_t addr_len) {
	tftp_message message;
	ssize_t result;

	if(strlen(string) >= 512) {
		fprintf(stderr, "TFTP error string too long.\n");
		return -1;
	}
	
	message.opcode = htons(TFTP_OP_ERROR);
	message.error.error_code = code;
	strcpy((char*)message.error.error_string, string);
	
	if ((result = sendto(s, &message, 4 + strlen(string) + 1, 0, (struct sockaddr *) addr, addr_len)) < 0)
		perror("Failed to sent TFTP error packet.");
	
	return result;
}

ssize_t tftp_send_data(int s, uint16_t block_number, uint8_t *data, ssize_t data_len, struct sockaddr_in *addr, socklen_t addr_len) {
	tftp_message message;
	ssize_t result;
	
	message.opcode = htons(TFTP_OP_DATA);
	message.data.block_number = htons(block_number);
	memcpy(message.data.data, data, data_len);
	
	if ((result = sendto(s, &message, 4 + data_len, 0, (struct sockaddr *) addr, addr_len)) < 0)
		perror("Failed to sent TFTP data packet.");
	
	return result;
}

int tftp_serve_file(FILE *fd, unsigned int port) {
	int main_socket;
	struct sockaddr_in addr_main_socket;
	
	main_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if(main_socket < 0) {
		fprintf(stderr, "Unable to create main TFTP socket.\n");
		return -1;
	}
	
	addr_main_socket.sin_family = AF_INET;
	addr_main_socket.sin_addr.s_addr = htonl(INADDR_ANY);
	addr_main_socket.sin_port = htons(port);
	
	if(bind(main_socket, (struct sockaddr *) &addr_main_socket, sizeof(addr_main_socket)) < 0) {
		fprintf(stderr, "Unable to bind TFTP socket.\n");
		close(main_socket);
		return -1;
	}
	
	printf("Waiting for TFTP connection on port %u.\n", port);
	
	while(1) {
		ssize_t len;
		tftp_message message;
		struct sockaddr_in client_addr;
		socklen_t client_addr_len = sizeof(client_addr);
		char * mode_pos;
		int client_socket;
		struct timeval socket_timeout_value = {.tv_sec = TFTP_RECV_TIMEOUT_SECS, .tv_usec = 0};
		size_t file_read_size;
		uint8_t data_buffer[512];
		int block_number = 1;
		
		int attempts;
		
		len = recvfrom(main_socket, &message, sizeof(message), 0, (struct sockaddr *) (&client_addr), &client_addr_len);
		
		if(len < 6) {
			fprintf(stderr, "Received request with invalid size.\n");
			tftp_send_error(main_socket, 0, "invalid request size", &client_addr, sizeof(client_addr));
			continue;
		}
		
		printf("Received request from %s.\n", inet_ntoa(client_addr.sin_addr));
		
		mode_pos = strchr((char*)message.request.filename_and_mode_string, '\0') + 1;
		
		if(ntohs(message.opcode) != TFTP_OP_RRQ) {
			fprintf(stderr, "Received invalid TFTP request.\n");
			tftp_send_error(main_socket, 0, "invalid opcode", &client_addr, sizeof(client_addr));
			continue;
		}
		
		if(strcmp((char*)message.request.filename_and_mode_string, "firmware.bin")) {
			fprintf(stderr, "Invalid filename requested.\n");
			tftp_send_error(main_socket, 0, "invalid filename", &client_addr, sizeof(client_addr));
			continue;
		}
		
		if(strcmp(mode_pos, "octet")) {
			fprintf(stderr, "Invalid mode requested.\n");
			tftp_send_error(main_socket, 0, "mode not supported", &client_addr, sizeof(client_addr));
			continue;
		}
		
		client_socket = socket(AF_INET, SOCK_DGRAM, 0);
		if(client_socket < 0) {
			fprintf(stderr, "Unable to create client TFTP socket.\n");
			close(main_socket);
			return -1;
		}
		
		if(setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &socket_timeout_value, sizeof(socket_timeout_value)) < 0) {
			fprintf(stderr, "Unable to set socket receive timeout.\n");
			close(client_socket);
			close(main_socket);
			return -1;
		}
		
		while(!feof(fd)) {
			file_read_size = fread(data_buffer, 1, 512, fd);
			
			for(attempts = TFTP_SEND_ATTEMPTS; attempts; attempts--) {
				len = tftp_send_data(client_socket, block_number, data_buffer, file_read_size, &client_addr, sizeof(client_addr)); // Implicit bind
				if(len < 0) {
					fprintf(stderr, "Data transfer failed.\n");
					close(client_socket);
					close(main_socket);
					return -1;
				}
				
				len = recvfrom(client_socket, &message, sizeof(message), 0, (struct sockaddr *) (&client_addr), &client_addr_len);
				
				if(len >= 4)
					break;
				
				if(len < 0 && errno != EAGAIN) {
					fprintf(stderr, "Data transfer failed.\n");
					close(client_socket);
					close(main_socket);
					return -1;
				}
			}
			
			if(!attempts) {
				fprintf(stderr, "Transfer timed out.\n");
				close(client_socket);
				close(main_socket);
				return -1;
			}
			
			message.opcode = ntohs(message.opcode);
			
			if(message.opcode == TFTP_OP_ERROR) {
				fprintf(stderr, "Received error. Code %u. Description: %s.\n", ntohs(message.error.error_code), message.error.error_string);
				close(client_socket);
				close(main_socket);
				return -1;
			}
			
			if(message.opcode != TFTP_OP_ACK) {
				fprintf(stderr, "Received invalid message during transfer.\n");
				tftp_send_error(client_socket, 0, "received invalid message during transfer", &client_addr, sizeof(client_addr));
				close(client_socket);
				close(main_socket);
				return -1;
			}
			
			if(ntohs(message.ack.block_number) != block_number) {
				fprintf(stderr, "Received ack with invalid block number.\n");
				tftp_send_error(client_socket, 0, "invalid ack block number", &client_addr, sizeof(client_addr));
				close(client_socket);
				close(main_socket);
				return -1;
			}
			
			block_number++;
		}
		
		printf("Done transfering file.\n");
		
		close(client_socket);
		
		break;
	}
	
	close(main_socket);
	
	return 0;
}
