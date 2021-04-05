#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <json-c/json.h>

#include "database.h"
#include "logger.h"

struct json_object *config_get_list_json() {
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_get_all_configs[] = "SELECT name,value,modification_date FROM configs";
	const char *str_ptr = NULL;
	long int modification_date = 0;
	struct json_object* config_list = NULL;
	struct json_object* config_item = NULL;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return NULL;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_all_configs, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return NULL;
	}
	
	config_list = json_object_new_array();
	
	do {
		result = sqlite3_step(ppstmt);
		
		if(result == SQLITE_ROW) {
			config_item = json_object_new_object();
			
			if((str_ptr = (const char*) sqlite3_column_text(ppstmt, 0)))
				json_object_object_add_ex(config_item, "name", json_object_new_string(str_ptr), JSON_C_OBJECT_ADD_KEY_IS_NEW);
			
			if((str_ptr = (const char*) sqlite3_column_text(ppstmt, 1)))
				json_object_object_add_ex(config_item, "value", json_object_new_string(str_ptr), JSON_C_OBJECT_ADD_KEY_IS_NEW);
			
			json_object_object_add_ex(config_item, "modification_date", json_object_new_int64(sqlite3_column_int64(ppstmt, 2)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
			
			json_object_array_add(config_list, config_item);
		}
		
	} while(result == SQLITE_ROW);
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to get the configuration list: %s", sqlite3_errstr(result));
		json_object_put(config_list);
		return NULL;
	}
	
	return config_list;
}

int config_get_value(const char *name, char *value_buf, size_t buf_size) {
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_get_config_value[] = "SELECT value FROM configs WHERE name=?1;";
	const char *str_ptr = NULL;
	int len = -2;
	
	if(name == NULL)
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
	
	if((result = sqlite3_bind_text(ppstmt, 1, name, -1, SQLITE_STATIC)) != SQLITE_OK) {
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

int config_get_value_int(const char *name, int min, int max, int default_value) {
	char buf[50];
	int value;
	
	if(config_get_value(name, buf, sizeof(buf)) <= 0)
		return default_value;
	
	if(sscanf(buf, "%d", &value) != 1)
		return default_value;
	
	if(value > max)
		return max;
	else if(value < min)
		return min;
	
	return value;
}

float config_get_value_float(const char *name, float min, float max, float default_value) {
	char buf[50];
	float value;
	
	if(config_get_value(name, buf, sizeof(buf)) <= 0)
		return default_value;
	
	if(sscanf(buf, "%f", &value) != 1)
		return default_value;
	
	if(value > max)
		return max;
	else if(value < min)
		return min;
	
	return value;
}

double config_get_value_double(const char *name, double min, double max, double default_value) {
	char buf[50];
	double value;
	
	if(config_get_value(name, buf, sizeof(buf)) <= 0)
		return default_value;
	
	if(sscanf(buf, "%lf", &value) != 1)
		return default_value;
	
	if(value > max)
		return max;
	else if(value < min)
		return min;
	
	return value;
}
