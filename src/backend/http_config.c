#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <json-c/json.h>

#include "http.h"
#include "logger.h"
#include "users.h"
#include "database.h"

unsigned int http_handler_get_config_list(struct MHD_Connection *conn,
											int logged_user_id,
											path_parameter_t *path_parameters,
											char *req_data,
											size_t req_data_size,
											char **resp_content_type,
											char **resp_data,
											size_t *resp_data_size,
											void *arg) {
	
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char *str_ptr;
	const char sql_get_configs[] = "SELECT name,value,description,modification_date FROM configs;";
	
	struct json_object* json_response = NULL;
	struct json_object* json_config_item = NULL;
	
	if(logged_user_id <= 0)
		return MHD_HTTP_UNAUTHORIZED;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_configs, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	json_response = json_object_new_array();
	
	while((result = sqlite3_step(ppstmt)) == SQLITE_ROW) {
		json_config_item = json_object_new_object();
		
		json_object_array_add(json_response, json_config_item);
		
		if((str_ptr = (const char*) sqlite3_column_text(ppstmt, 0)) == NULL)
			break;
		
		json_object_object_add_ex(json_config_item, "name", json_object_new_string(str_ptr), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		if((str_ptr = (const char*) sqlite3_column_text(ppstmt, 1)) == NULL)
			break;
		
		json_object_object_add_ex(json_config_item, "value", json_object_new_string(str_ptr), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		if((str_ptr = (const char*) sqlite3_column_text(ppstmt, 2)) == NULL)
			break;
		
		json_object_object_add_ex(json_config_item, "description", json_object_new_string(str_ptr), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		json_object_object_add_ex(json_config_item, "modification_date", json_object_new_int64(sqlite3_column_int64(ppstmt, 3)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
	}
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to fetch config list from database: %s", sqlite3_errstr(result));
		
		json_object_put(json_response);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	*resp_data = strdup(json_object_get_string(json_response));
	
	json_object_put(json_response);
	
	if(*resp_data == NULL)
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	
	*resp_data_size = strlen(*resp_data);
	
	*resp_content_type = strdup(JSON_CONTENT_TYPE);
	
	return MHD_HTTP_OK;
}

unsigned int http_handler_get_config(struct MHD_Connection *conn,
											int logged_user_id,
											path_parameter_t *path_parameters,
											char *req_data,
											size_t req_data_size,
											char **resp_content_type,
											char **resp_data,
											size_t *resp_data_size,
											void *arg) {
	
	const char *config_name_str;
	
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char *str_ptr;
	const char sql_get_config[] = "SELECT name,value,description,modification_date FROM configs WHERE name = ?1;";
	int count = 0;
	
	struct json_object* json_response = NULL;
	
	if(logged_user_id <= 0)
		return MHD_HTTP_UNAUTHORIZED;
	
	if((config_name_str = http_parameter_get_value(path_parameters, 2)) == NULL || strlen(config_name_str) < 2)
		return MHD_HTTP_BAD_REQUEST;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_config, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	if((result = sqlite3_bind_text(ppstmt, 1, config_name_str, -1, SQLITE_STATIC)) != SQLITE_OK) {
		LOG_ERROR("Failed to bind value to prepared statement: %s", sqlite3_errstr(result));
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	json_response = json_object_new_object();
	
	while((result = sqlite3_step(ppstmt)) == SQLITE_ROW) {
		if(count++)
			continue;
		
		if((str_ptr = (const char*) sqlite3_column_text(ppstmt, 0)) == NULL)
			break;
		
		json_object_object_add_ex(json_response, "name", json_object_new_string(str_ptr), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		if((str_ptr = (const char*) sqlite3_column_text(ppstmt, 1)) == NULL)
			break;
		
		json_object_object_add_ex(json_response, "value", json_object_new_string(str_ptr), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		if((str_ptr = (const char*) sqlite3_column_text(ppstmt, 2)) == NULL)
			break;
		
		json_object_object_add_ex(json_response, "description", json_object_new_string(str_ptr), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		json_object_object_add_ex(json_response, "modification_date", json_object_new_int64(sqlite3_column_int64(ppstmt, 3)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
	}
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to fetch config from database: %s", sqlite3_errstr(result));
		
		json_object_put(json_response);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	if(count == 0) {
		json_object_put(json_response);
		
		return MHD_HTTP_NOT_FOUND;
	}
	
	*resp_data = strdup(json_object_get_string(json_response));
	
	json_object_put(json_response);
	
	if(*resp_data == NULL)
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	
	*resp_data_size = strlen(*resp_data);
	
	*resp_content_type = strdup(JSON_CONTENT_TYPE);
	
	return MHD_HTTP_OK;
}

unsigned int http_handler_update_config(struct MHD_Connection *conn,
											int logged_user_id,
											path_parameter_t *path_parameters,
											char *req_data,
											size_t req_data_size,
											char **resp_content_type,
											char **resp_data,
											size_t *resp_data_size,
											void *arg) {
	
	const char *config_name_str;
	
	struct json_object *received_json;
	
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_update_config[] = "UPDATE configs SET (value,modification_date) = (?2,?3) WHERE name = ?1;";
	int changes;
	
	if(logged_user_id <= 0 || users_check_admin(logged_user_id) == 0)
		return MHD_HTTP_UNAUTHORIZED;
	
	if((config_name_str = http_parameter_get_value(path_parameters, 2)) == NULL || strlen(config_name_str) < 2)
		return MHD_HTTP_BAD_REQUEST;
	
	if(req_data == NULL)
		return MHD_HTTP_BAD_REQUEST;
	
	received_json = json_tokener_parse(req_data);
	
	if(json_object_get_type(received_json) != json_type_string)
		return MHD_HTTP_BAD_REQUEST;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_update_config, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	// SQLITE_OK é zero, então somando todos os resultados podemos saber se algum falhou
	result = sqlite3_bind_text(ppstmt, 1, config_name_str, -1, SQLITE_STATIC);
	result += sqlite3_bind_text(ppstmt, 2, json_object_get_string(received_json), -1, SQLITE_STATIC);
	result += sqlite3_bind_int64(ppstmt, 3, time(NULL));
		
	if(result) {
		LOG_ERROR("Failed to bind value to prepared statement: %s", sqlite3_errstr(result));
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	result = sqlite3_step(ppstmt);
		
	changes = sqlite3_changes(db_conn);
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to update config value in database: %s", sqlite3_errstr(result));
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	if(changes == 0)
		return MHD_HTTP_NOT_FOUND;
	
	return MHD_HTTP_OK;
}
