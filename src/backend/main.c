#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <pthread.h>

#include <microhttpd.h>
#include <sqlite3.h>

#include "logger.h"
#include "database.h"
#include "http.h"
#include "power.h"
#include "disaggregation.h"

void *data_acquisition_loop(void *argp);
void *disaggregation_loop(void *argp);

int main(int argc, char **argv) {
	sigset_t signal_set;
	int recv_signal;
	volatile int terminate = 0;
	
	int opt;
	int http_port = DEFAULT_HTTP_PORT;
	char *log_level_name = NULL;
	char *working_dir_path = NULL;
	time_t offline_timestamp = 0;
	
	pthread_t data_acquisition_thread;
	pthread_t disaggregation_thread;
	
	struct MHD_Daemon *httpd;
	
	while((opt = getopt(argc, argv, "l:p:w:t:")) != -1) {
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
			case 't':
				sscanf(optarg, "%li", &offline_timestamp);
				break;
			default:
				fprintf(stderr, "Usage: %s [options]\n", argv[0]);
				fprintf(stderr, "Valid options:\n");
				fprintf(stderr, "\t-l Logging level\n");
				fprintf(stderr, "\t-p HTTP port number\n");
				fprintf(stderr, "\t-w Working directory path\n");
				fprintf(stderr, "\t-t Date as unix epoch (offline mode)\n");
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
	
	if(offline_timestamp < 0) {
		LOG_FATAL("Invalid date.");
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
	
	load_saved_power_data(offline_timestamp);
	
	if(offline_timestamp == 0) {
		load_saved_load_events();
		
		LOG_INFO("Starting data acquisition thread.");
		pthread_create(&data_acquisition_thread, NULL, data_acquisition_loop, (void*) &terminate);
		
		LOG_INFO("Starting disaggregation thread.");
		pthread_create(&disaggregation_thread, NULL, disaggregation_loop, (void*) &terminate);
	} else {
		LOG_INFO("Running in offline mode with unix epoch %li.", offline_timestamp);
		
		LOG_INFO("Detecting load events.");
		detect_all_load_events();
	}
	
	LOG_INFO("Starting HTTP server.");
	httpd = http_init(http_port);
	
	/* Suspende a execução da thread principal até receber algum sinal do conjunto */
	sigwait(&signal_set, &recv_signal);
	
	terminate = 1;
	
	http_stop(httpd);
	
	if(offline_timestamp == 0) {
		LOG_INFO("Waiting for threads to terminate.");
		
		pthread_join(data_acquisition_thread, NULL);
		pthread_join(disaggregation_thread, NULL);
	}
	
	close_power_data_file();
	
	LOG_INFO("Program terminated.");
	
	return 0;
}
