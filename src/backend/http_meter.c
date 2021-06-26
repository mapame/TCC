#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <json-c/json.h>

#include "http.h"
#include "database.h"
#include "logger.h"

unsigned int http_handler_get_meter_events(struct MHD_Connection *conn,
											int logged_user_id,
											path_parameter_t *path_parameters,
											char *req_data,
											size_t req_data_size,
											char **resp_content_type,
											char **resp_data,
											size_t *resp_data_size,
											void *arg) {
	const char *last_secs_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "last");
	const char *start_timestamp_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "start");
	const char *end_timestamp_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "end");
	
	int last_secs;
	time_t start_timestamp, end_timestamp;
	
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char *str_ptr;
	const char sql_get_meter_events[] = "SELECT timestamp,type,count FROM meter_events WHERE timestamp >= ?1 AND timestamp <= ?2 ORDER BY timestamp DESC;";
	
	struct json_object* json_response = NULL;
	struct json_object* json_event = NULL;
	
	if(logged_user_id <= 0)
		return MHD_HTTP_UNAUTHORIZED;
	
	if(last_secs_str) {
		if(sscanf(last_secs_str, "%d", &last_secs) != 1)
			return MHD_HTTP_BAD_REQUEST;
		
		if(last_secs < 0)
			return MHD_HTTP_BAD_REQUEST;
		
		end_timestamp = time(NULL);
		start_timestamp = end_timestamp - last_secs;
		
	} else if(start_timestamp_str && end_timestamp_str) {
		if(sscanf(start_timestamp_str, "%ld", &start_timestamp) != 1 || sscanf(end_timestamp_str, "%ld", &end_timestamp) != 1)
			return MHD_HTTP_BAD_REQUEST;
		
		if(end_timestamp <= 0 || start_timestamp <= 0 || end_timestamp < start_timestamp || end_timestamp - start_timestamp > 31 * 24 * 3600)
			return MHD_HTTP_BAD_REQUEST;
		
	} else {
		return MHD_HTTP_BAD_REQUEST;
	}
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_meter_events, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	if(sqlite3_bind_int64(ppstmt, 1, start_timestamp) != SQLITE_OK || sqlite3_bind_int64(ppstmt, 2, end_timestamp) != SQLITE_OK) {
		LOG_ERROR("Failed to bind values to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	json_response = json_object_new_array();
	
	while((result = sqlite3_step(ppstmt)) == SQLITE_ROW) {
		json_event = json_object_new_object();
		
		json_object_array_add(json_response, json_event);
		
		json_object_object_add_ex(json_event, "timestamp", json_object_new_int64(sqlite3_column_int64(ppstmt, 0)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		if((str_ptr = (const char*) sqlite3_column_text(ppstmt, 1)) == NULL)
			break;
		
		json_object_object_add_ex(json_event, "type", json_object_new_string(str_ptr), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(json_event, "count", json_object_new_int(sqlite3_column_int(ppstmt, 2)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
	}
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to fetch meter events from database: %s", sqlite3_errstr(result));
		
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
