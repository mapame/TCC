#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "logger.h"

static FILE *log_fd = NULL;
static volatile loglevel_t loglevel = LOGLEVEL_WARN;

static pthread_mutex_t log_mutex;

static const char *level_names[] = {
	"TRACE",
	"DEBUG",
	"INFO",
	"WARN",
	"ERROR",
	"FATAL"
};

void logger_init(FILE *fd, loglevel_t level) {
	if(log_fd)
		return;
	
	log_fd = fd ? fd : stderr;
	loglevel = level;
	
	pthread_mutex_init(&log_mutex, NULL);
}

void logger_set_level(loglevel_t level) {
	loglevel = level;
}

int logger_set_level_by_name(const char* level_name) {
	if(level_name == NULL)
		return -1;
	
	for(loglevel_t level = LOGLEVEL_TRACE; level <= LOGLEVEL_FATAL; level++)
		if(!strcmp(level_names[level], level_name)) {
			loglevel = level;
			
			return 0;
		}
	
	return -2;
}

void logger_log(loglevel_t level, const char* file, int line, const char* fmt, ...) {
	va_list fargs;
	time_t time_now;
	char time_str[32];
	
	if(level < loglevel || log_fd == NULL)
		return;
	
	time_now = time(NULL);
	strftime(time_str, 32, "%Y-%m-%d %H:%M:%S", localtime(&time_now));
	
	va_start(fargs, fmt);
	
	pthread_mutex_lock(&log_mutex);
	
	fprintf(log_fd, "%s | %s | ", time_str, level_names[level]);
	
	if(loglevel <= LOGLEVEL_DEBUG)
		fprintf(log_fd, "%s:%d | ", (file ? file : "N/A"), line);
	
	vfprintf(log_fd, fmt, fargs);
	fprintf(log_fd, "\n");
	
	fflush(log_fd);
	
	pthread_mutex_unlock(&log_mutex);
	
	va_end(fargs);
}
