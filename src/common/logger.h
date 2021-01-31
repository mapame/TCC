#ifndef _LOGGER_H
#define _LOGGER_H

#include <stdio.h>

#define LOG_TRACE(fmt, ...) logger_log(LOGLEVEL_TRACE, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) logger_log(LOGLEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  logger_log(LOGLEVEL_INFO , __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  logger_log(LOGLEVEL_WARN , __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) logger_log(LOGLEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) logger_log(LOGLEVEL_FATAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)


typedef enum {
    LOGLEVEL_TRACE,
    LOGLEVEL_DEBUG,
    LOGLEVEL_INFO,
    LOGLEVEL_WARN,
    LOGLEVEL_ERROR,
    LOGLEVEL_FATAL,
} loglevel_t;

void logger_init(FILE *fd, loglevel_t level);
void logger_set_level(loglevel_t level);
int logger_set_level_by_name(const char* level_name);
void logger_log(loglevel_t level, const char* file, int line, const char* fmt, ...);

#endif /* _LOGGER_H */
