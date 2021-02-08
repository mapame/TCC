#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <pthread.h>

#include <microhttpd.h>
#include <sqlite3.h>

#include "logger.h"


void *data_acquisition_loop(void *argp);

int main(int argc, char **argv) {
	sigset_t signal_set;
	int recv_signal;
	volatile int terminate = 0;
	
	pthread_t data_acquisition_thread;
	
	int opt;
	int http_port = DEFAULT_HTTP_PORT;
	char *log_level_name = NULL;
	char *working_dir_path = NULL;
	
	while((opt = getopt(argc, argv, "l:p:w:")) != -1) {
		switch (opt) {
			case 'l':
				log_level_name = strdup(optarg);
				break;
			case 'p':
				http_port = atoi(optarg);
				break;
			case 'w':
				working_dir_path = strdup(optarg);
				break;
			default:
				fprintf(stderr, "Usage: %s [options] -k key\n", argv[0]);
				fprintf(stderr, "Valid options:\n");
				fprintf(stderr, "\t-l Logging level\n");
				fprintf(stderr, "\t-p HTTP port number\n");
				fprintf(stderr, "\t-w Working directory path\n");
				exit(EXIT_FAILURE);
		}
	}
	
	if(log_level_name != NULL) {
		if(logger_set_level_by_name(log_level_name) != 0) {
			LOG_FATAL("Invalid log level.");
			LOG_FATAL("Allowed values: TRACE, DEBUG, INFO, WARN, ERROR, FATAL");
			exit(EXIT_FAILURE);
		}
		
		free(log_level_name);
	}
	
	if(http_port == 0) {
		LOG_FATAL("Invalid HTTP port number.");
		exit(EXIT_FAILURE);
	}
	
	if(working_dir_path != NULL) {
		if(chdir(working_dir_path) < 0) {
			LOG_FATAL("Failed changing working directory to: %s", working_dir_path);
			exit(EXIT_FAILURE);
		}
		
		free(working_dir_path);
	}
	
	if(access(DB_FILENAME, F_OK) != 0) {
		LOG_FATAL("Database file does not exists.");
		exit(EXIT_FAILURE);
	}
	
	sigemptyset(&signal_set);
	sigaddset(&signal_set, SIGINT);
	sigaddset(&signal_set, SIGTERM);
	sigaddset(&signal_set, SIGHUP);
	
	/* Bloqueia os sinais SIGINT, SIGTERM e SIGHUP, de maneira que as threads
	 * criadas a partir de agora vão herdar esse bloqueio. */
	pthread_sigmask(SIG_BLOCK, &signal_set, NULL);
	
	LOG_INFO("Starting data acquisition thread.");
	pthread_create(&data_acquisition_thread, NULL, data_acquisition_loop, (void*) &terminate);
	
	/* Suspende a execução da thread principal até receber algum sinal do conjunto */
	sigwait(&signal_set, &recv_signal);
	
	terminate = 1;
	
	pthread_join(data_acquisition_thread, NULL);
	
	
	return 0;
}
