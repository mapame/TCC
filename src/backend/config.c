#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "database.h"
#include "logger.h"

#include "config.h"

int config_get_list(config_t **config_list_ptr) {
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_get_configs[] = "SELECT key,value,modification_date FROM configs;";
	const char *str_ptr;
	int size = 8, count = 0;
	
	if(config_list_ptr == NULL)
		return 0;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_configs, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	*config_list_ptr = (config_t*) malloc(sizeof(config_t) * size);
	
	if(*config_list_ptr == NULL) {
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -2;
	}
	
	do {
		result = sqlite3_step(ppstmt);
		
		if(result == SQLITE_ROW) {
			if(count >= size) {
				config_t *tmp_ptr;
				
				size *= 2;
				
				if((tmp_ptr = (config_t*) realloc(*config_list_ptr, sizeof(config_t) * size)))
					*config_list_ptr = tmp_ptr;
				else
					break;
			}
			
			if((str_ptr = (const char*) sqlite3_column_text(ppstmt, 0)) == NULL)
				break;
			
			(*config_list_ptr)[count].key = strdup(str_ptr);
			
			if((str_ptr = (const char*) sqlite3_column_text(ppstmt, 1)) == NULL) {
				free((*config_list_ptr)[count].key);
				break;
			}
			
			(*config_list_ptr)[count].value = strdup(str_ptr);
			
			(*config_list_ptr)[count].modification_date = sqlite3_column_int64(ppstmt, 2);
			
			count++;
		}
		
	} while(result == SQLITE_ROW);
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to get the configuration list: %s", sqlite3_errstr(result));
		
		for(int pos = 0; pos < count; pos++) {
			free((*config_list_ptr)[count].key);
			free((*config_list_ptr)[count].value);
		}
		
		free(*config_list_ptr);
		
		return -1;
	}
	
	return count;
}

void config_free(config_t *config_ptr) {
	if(config_ptr == NULL)
		return;
	
	free(config_ptr->key);
	free(config_ptr->value);
}

int config_get_value(const char *key, char *value_buf, size_t buf_size) {
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_get_config_value[] = "SELECT value FROM configs WHERE key=?1;";
	const char *str_ptr = NULL;
	int len = -2;
	
	if(key == NULL)
		return 0;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_config_value, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	if((result = sqlite3_bind_text(ppstmt, 1, key, -1, SQLITE_STATIC)) != SQLITE_OK) {
		LOG_ERROR("Failed to bind value to prepared statement: %s", sqlite3_errstr(result));
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	if((result = sqlite3_step(ppstmt)) == SQLITE_ROW) {
		len = 0;
		
		if(value_buf && (str_ptr = (const char*) sqlite3_column_text(ppstmt, 0))) {
			strncpy(value_buf, str_ptr, buf_size);
			len = strlen(str_ptr);
			value_buf[buf_size - 1] = '\0';
		} else if(sqlite3_errcode(db_conn) == SQLITE_NOMEM) {
			LOG_ERROR("Out of memory.");
			len = -3;
		}
		
		result = sqlite3_step(ppstmt);
	}
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to read configuration: %s", sqlite3_errstr(result));
		return -1;
	}
	
	return len;
}

int config_get_value_int(const char *key, int min, int max, int default_value) {
	char buf[50];
	int value;
	
	if(config_get_value(key, buf, sizeof(buf)) <= 0)
		return default_value;
	
	if(sscanf(buf, "%d", &value) != 1)
		return default_value;
	
	if(value > max)
		return max;
	else if(value < min)
		return min;
	
	return value;
}

float config_get_value_float(const char *key, float min, float max, float default_value) {
	char buf[50];
	float value;
	
	if(config_get_value(key, buf, sizeof(buf)) <= 0)
		return default_value;
	
	if(sscanf(buf, "%f", &value) != 1)
		return default_value;
	
	if(value > max)
		return max;
	else if(value < min)
		return min;
	
	return value;
}

double config_get_value_double(const char *key, double min, double max, double default_value) {
	char buf[50];
	double value;
	
	if(config_get_value(key, buf, sizeof(buf)) <= 0)
		return default_value;
	
	if(sscanf(buf, "%lf", &value) != 1)
		return default_value;
	
	if(value > max)
		return max;
	else if(value < min)
		return min;
	
	return value;
}
