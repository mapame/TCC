#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>

#include "common.h"
#include "logger.h"
#include "power.h"

#define POWER_DATA_BUFFER_SIZE 24 * 3600

pthread_mutex_t power_data_mutex = PTHREAD_MUTEX_INITIALIZER;

time_t last_loaded_timestamp = 0;

power_data_t power_data_buffer[POWER_DATA_BUFFER_SIZE];
int power_data_buffer_pos = 0;
int power_data_buffer_count = 0;

FILE *open_pd_fd = NULL;
char open_pd_filename[32];

static size_t generate_pd_filename(time_t time_epoch, char *buffer, size_t len) {
	struct tm time_tm;
	
	gmtime_r(&time_epoch, &time_tm);
	return strftime(buffer, len, "pd-%F.csv", &time_tm);
}

static int import_power_data_file(const char *filename, time_t timestamp_limit) {
	FILE *pd_file = NULL;
	power_data_t pd_aux;
	int counter = 0;
	
	if((pd_file = fopen(filename, "r")) == NULL) {
		LOG_ERROR("Failed to open power data file \"%s\": %s", filename, strerror(errno));
		return -1;
	}
	
	if(pthread_mutex_lock(&power_data_mutex))
		return -2;
	
	while(fscanf(pd_file, "%li,%lf,%lf,%lf,%lf,%lf,%lf\n", &pd_aux.timestamp, &pd_aux.v[0], &pd_aux.v[1], &pd_aux.i[0], &pd_aux.i[1], &pd_aux.p[0], &pd_aux.p[1]) == 7) {
		if(pd_aux.timestamp < timestamp_limit)
			continue;
		
		/* Não carrega valores fora de ordem */
		if(pd_aux.timestamp <= last_loaded_timestamp)
			continue;
		
		counter++;
		
		last_loaded_timestamp = pd_aux.timestamp;
		
		pd_aux.s[0] = pd_aux.v[0] * pd_aux.i[0];
		pd_aux.s[1] = pd_aux.v[1] * pd_aux.i[1];
		
		pd_aux.q[0] = sqrtf(powf(pd_aux.s[0], 2) - powf(pd_aux.p[0], 2));
		pd_aux.q[1] = sqrtf(powf(pd_aux.s[1], 2) - powf(pd_aux.p[1], 2));
		
		memcpy(&power_data_buffer[power_data_buffer_pos], &pd_aux, sizeof(power_data_t));
		
		power_data_buffer_pos = (power_data_buffer_pos + 1) % POWER_DATA_BUFFER_SIZE;
		if(power_data_buffer_count < POWER_DATA_BUFFER_SIZE)
			power_data_buffer_count++;
	}
	
	pthread_mutex_unlock(&power_data_mutex);
	
	fclose(pd_file);
	
	return counter;
}

int load_saved_power_data() {
	time_t time_epoch;
	char filename_today[32];
	char filename_yesterday[32];
	int result;
	
	time_epoch = time(NULL);
	generate_pd_filename(time_epoch, filename_today, sizeof(filename_today));
	
	/* Subtrai os segundos equivalentes a 24 horas para obter o dia de ontem */
	time_epoch -= (24 * 60 * 60);
	generate_pd_filename(time_epoch, filename_yesterday, sizeof(filename_yesterday));
	
	if(access(filename_yesterday, F_OK) == 0) {
		LOG_INFO("Loading power data from yesterday's file \"%s\".", filename_yesterday);
		
		result = import_power_data_file(filename_yesterday, time_epoch);
		if(result < 0) {
			LOG_ERROR("Failed to load power data from yesterday's file.");
			
			return -1;
		}
		
		LOG_INFO("Loaded %d entries from yesterday's file.", result);
	}
	
	if(access(filename_today, F_OK) == 0) {
		LOG_INFO("Loading power data from today's file \"%s\".", filename_today);
		
		result = import_power_data_file(filename_today, 0);
		if(result < 0) {
			LOG_ERROR("Failed to load power data from today's file.");
			
			return -1;
		}
		
		LOG_INFO("Loaded %d entries from today's file.", result);
	}
	
	return 0;
}

void close_power_data_file() {
	if(open_pd_fd == NULL)
		return;
	
	fclose(open_pd_fd);
	
	open_pd_fd = NULL;
}

int store_power_data(power_data_t *pd_ptr) {
	char new_pd_filename[32];
	
	if(pd_ptr == NULL)
		return -1;
	
	pd_ptr->s[0] = pd_ptr->v[0] * pd_ptr->i[0];
	pd_ptr->s[1] = pd_ptr->v[1] * pd_ptr->i[1];
	
	pd_ptr->q[0] = sqrtf(powf(pd_ptr->s[0], 2) - powf(pd_ptr->p[0], 2));
	pd_ptr->q[1] = sqrtf(powf(pd_ptr->s[1], 2) - powf(pd_ptr->p[1], 2));
	
	if(pthread_mutex_lock(&power_data_mutex))
		return -2;
	
	if(pd_ptr->timestamp <= last_loaded_timestamp) {
		pthread_mutex_unlock(&power_data_mutex);
		return 1;
	}
	
	generate_pd_filename(pd_ptr->timestamp, new_pd_filename, sizeof(new_pd_filename));
	
	if(open_pd_fd == NULL) {
		open_pd_fd = fopen(new_pd_filename, "a");
		if(open_pd_fd == NULL) {
			LOG_ERROR("Failed to open power data file \"%s\": %s", new_pd_filename, strerror(errno));
			
			pthread_mutex_unlock(&power_data_mutex);
			return -3;
		}
		
		strcpy(open_pd_filename, new_pd_filename);
	} else {
		/* Quando o dia terminar, fecha o arquivo atual e cria um novo. */
		if(strcmp(open_pd_filename, new_pd_filename)) {
			LOG_INFO("Changing to new file \"%s\".", new_pd_filename);
			
			fclose(open_pd_fd);
			
			open_pd_fd = fopen(new_pd_filename, "a");
			if(open_pd_fd == NULL) {
				LOG_FATAL("Failed to create new power data file \"%s\": %s", new_pd_filename, strerror(errno));
				
				pthread_mutex_unlock(&power_data_mutex);
				return -3;
			}
			
			strcpy(open_pd_filename, new_pd_filename);
		}
	}
	
	if(fprintf(open_pd_fd, "%li,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf\n", pd_ptr->timestamp, pd_ptr->v[0], pd_ptr->v[1], pd_ptr->i[0], pd_ptr->i[1], pd_ptr->p[0], pd_ptr->p[1]) < 0) {
		LOG_ERROR("Failed to write power data to file.");
		
		pthread_mutex_unlock(&power_data_mutex);
		return -4;
	}
	
	fflush(open_pd_fd);
	
	memcpy(&power_data_buffer[power_data_buffer_pos], pd_ptr, sizeof(power_data_t));
	
	power_data_buffer_pos = (power_data_buffer_pos + 1) % POWER_DATA_BUFFER_SIZE;
	if(power_data_buffer_count < POWER_DATA_BUFFER_SIZE)
		power_data_buffer_count++;
	
	last_loaded_timestamp = pd_ptr->timestamp;
	
	pthread_mutex_unlock(&power_data_mutex);
	
	return 0;
}
