#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "logger.h"

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static volatile loglevel_t loglevel = LOGLEVEL_WARN;

static const char *level_names[] = {
	"TRACE",
	"DEBUG",
	"INFO",
	"WARN",
	"ERROR",
	"FATAL"
};

void logger_set_level(loglevel_t level) {
	pthread_mutex_lock(&log_mutex);
	loglevel = level;
	pthread_mutex_unlock(&log_mutex);
}

int logger_set_level_by_name(const char* level_name) {
	if(level_name == NULL)
		return -1;
	
	for(loglevel_t level = LOGLEVEL_TRACE; level <= LOGLEVEL_FATAL; level++)
		if(!strcmp(level_names[level], level_name)) {
			logger_set_level(level);
			
			return 0;
		}
	
	return -2;
}

void logger_log(loglevel_t level, const char* file, int line, const char* fmt, ...) {
	va_list fargs;
	time_t time_now;
	char time_str[32];
	
	if(level < loglevel)
		return;
	
	time_now = time(NULL);
	strftime(time_str, 32, "%Y-%m-%d %H:%M:%S", localtime(&time_now));
	
	va_start(fargs, fmt);
	
	pthread_mutex_lock(&log_mutex);
	
	fprintf(stderr, "%s | %s | ", time_str, level_names[level]);
	
	if(loglevel <= LOGLEVEL_DEBUG)
		fprintf(stderr, "%s:%d | ", (file ? file : "N/A"), line);
	
	vfprintf(stderr, fmt, fargs);
	fprintf(stderr, "\n");
	
	fflush(stderr);
	
	pthread_mutex_unlock(&log_mutex);
	
	va_end(fargs);
}
