#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bsd/string.h>
#include <math.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <microhttpd.h>
#include <sqlite3.h>

#include "bearssl.h"

#include "cJSON.h"

#include "common.h"
#include "logger.h"
#include "communication.h"

static int terminate = 0;

void sigint_handler(int signum) {
	terminate = 1;
}

int main(int argc, char **argv) {
	struct sigaction sa, old_sa;
	struct stat config_file_stat;
	
	FILE *config_file_fd = NULL;
	
	char *config_file_content;
	
	logger_init(stderr, LOGLEVEL_INFO);
	
	if(argc != 2) {
		LOG_FATAL("Missing config file path.");
		return -1;
	}
	
	if(stat(argv[1], &config_file_stat)) {
		LOG_FATAL("Failed to stat config file '%s'.", argv[1]);
		return -1;
	}
	
	config_file_fd = fopen(argv[1], "r");
	if(config_file_fd == NULL) {
		LOG_FATAL("Failed to open config file '%s'.", argv[1]);
		return -1;
	}
	
	
	
	sa.sa_handler = sigint_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, &old_sa);
	
	
	sigaction(SIGINT, &old_sa, NULL);
	
	return 0;
}
