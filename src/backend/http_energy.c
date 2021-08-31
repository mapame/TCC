#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <json-c/json.h>

#include "common.h"
#include "logger.h"
#include "http.h"
#include "database.h"

unsigned int http_handler_get_energy_overview(struct MHD_Connection *conn,
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
	const char sql_get_energy_months[] = "SELECT DISTINCT year,month FROM energy_hours;";
	const char sql_get_energy_minute_bounds[] = "SELECT MIN(timestamp),MAX(timestamp) FROM energy_minutes;";
	int year = 0;
	
	json_object *response_object = NULL;
	json_object *year_array = NULL;
	json_object *year_item = NULL;
	json_object *month_array = NULL;
	
	if(logged_user_id <= 0)
		return MHD_HTTP_UNAUTHORIZED;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_energy_months, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	response_object = json_object_new_object();
	
	year_array = json_object_new_array();
	
	json_object_object_add_ex(response_object, "years", year_array, JSON_C_OBJECT_ADD_KEY_IS_NEW);
	
	while((result = sqlite3_step(ppstmt)) == SQLITE_ROW) {
		if(year != sqlite3_column_int(ppstmt, 0)) {
			year = sqlite3_column_int(ppstmt, 0);
			
			year_item = json_object_new_object();
			json_object_array_add(year_array, year_item);
			json_object_object_add_ex(year_item, "year", json_object_new_int(year), JSON_C_OBJECT_ADD_KEY_IS_NEW);
			
			month_array = json_object_new_array();
			json_object_object_add_ex(year_item, "months", month_array, JSON_C_OBJECT_ADD_KEY_IS_NEW);
		}
		
		json_object_array_add(month_array, json_object_new_int(sqlite3_column_int(ppstmt, 1)));
	}
	
	sqlite3_finalize(ppstmt);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to get energy dates from database: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		json_object_put(response_object);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_energy_minute_bounds, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		json_object_put(response_object);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	if((result = sqlite3_step(ppstmt)) == SQLITE_ROW) {
		json_object_object_add_ex(response_object, "minute_min_timestamp", json_object_new_int(sqlite3_column_int(ppstmt, 0)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_object, "minute_max_timestamp", json_object_new_int(sqlite3_column_int(ppstmt, 1)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		result = sqlite3_step(ppstmt);
	}
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to get energy dates from database: %s", sqlite3_errstr(result));
		json_object_put(response_object);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	*resp_data = strdup(json_object_get_string(response_object));
	
	json_object_put(response_object);
	
	if(*resp_data == NULL)
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	
	*resp_data_size = strlen(*resp_data);
	
	*resp_content_type = strdup(JSON_CONTENT_TYPE);
	
	return MHD_HTTP_OK;
}

unsigned int http_handler_get_energy_minutes(struct MHD_Connection *conn,
												int logged_user_id,
												path_parameter_t *path_parameters,
												char *req_data,
												size_t req_data_size,
												char **resp_content_type,
												char **resp_data,
												size_t *resp_data_size,
												void *arg) {
	const char *start_timestamp_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "start");
	const char *end_timestamp_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "end");
	time_t start_timestamp, end_timestamp;
	
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_get_energy_minutes[] = "SELECT timestamp,second_count,active,reactive FROM energy_minutes WHERE timestamp >= ?1 AND timestamp <= ?2;";
	
	json_object *response_array = NULL;
	json_object *response_item = NULL;
	
	if(logged_user_id <= 0)
		return MHD_HTTP_UNAUTHORIZED;
	
	if(start_timestamp_str == NULL || sscanf(start_timestamp_str, "%ld", &start_timestamp) != 1)
		return MHD_HTTP_BAD_REQUEST;
	
	if(end_timestamp_str == NULL || sscanf(end_timestamp_str, "%ld", &end_timestamp) != 1)
		end_timestamp = start_timestamp + 24 * 3600;
	
	if(end_timestamp - start_timestamp > 24 * 3600)
		return MHD_HTTP_BAD_REQUEST;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_energy_minutes, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	if(sqlite3_bind_int64(ppstmt, 1, start_timestamp) != SQLITE_OK || sqlite3_bind_int64(ppstmt, 2, end_timestamp) != SQLITE_OK) {
		LOG_ERROR("Failed to bind value to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	response_array = json_object_new_array();
	
	while((result = sqlite3_step(ppstmt)) == SQLITE_ROW) {
		response_item = json_object_new_object();
		
		json_object_object_add_ex(response_item, "timestamp", json_object_new_int64(sqlite3_column_int64(ppstmt, 0)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "second_count", json_object_new_int(sqlite3_column_int(ppstmt, 1)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "active", json_object_new_double(sqlite3_column_double(ppstmt, 2)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "reactive", json_object_new_double(sqlite3_column_double(ppstmt, 3)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		json_object_array_add(response_array, response_item);
	}
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to get energy data as minutes: %s", sqlite3_errstr(result));
		json_object_put(response_array);
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	*resp_data = strdup(json_object_get_string(response_array));
	
	json_object_put(response_array);
	
	if(*resp_data == NULL)
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	
	*resp_data_size = strlen(*resp_data);
	
	*resp_content_type = strdup(JSON_CONTENT_TYPE);
	
	return MHD_HTTP_OK;
}

unsigned int http_handler_get_energy_hours(struct MHD_Connection *conn,
									int logged_user_id,
									path_parameter_t *path_parameters,
									char *req_data,
									size_t req_data_size,
									char **resp_content_type,
									char **resp_data,
									size_t *resp_data_size,
									void *arg) {
	const char *date_year_srt = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "year");
	const char *date_month_srt = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "month");
	const char *date_day_srt = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "day");
	int date_year, date_month, date_day;
	
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_get_energy_hours[] = "SELECT hour,second_count,active,reactive,cost FROM energy_hours WHERE year = ?1 AND month = ?2 AND day = ?3;";
	
	json_object *response_array = NULL;
	json_object *response_item = NULL;
	
	if(logged_user_id <= 0)
		return MHD_HTTP_UNAUTHORIZED;
	
	if(date_year_srt == NULL || sscanf(date_year_srt, "%d", &date_year) != 1)
		return MHD_HTTP_BAD_REQUEST;
		
	if(date_month_srt == NULL || sscanf(date_month_srt, "%d", &date_month) != 1)
		return MHD_HTTP_BAD_REQUEST;
		
	if(date_day_srt == NULL || sscanf(date_day_srt, "%d", &date_day) != 1)
		return MHD_HTTP_BAD_REQUEST;
	
	if(date_year < 2021 || date_month < 1 || date_month > 12 || date_day < 1 || date_day > 31)
		return MHD_HTTP_BAD_REQUEST;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_energy_hours, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	if(sqlite3_bind_int(ppstmt, 1, date_year) != SQLITE_OK || sqlite3_bind_int(ppstmt, 2, date_month) != SQLITE_OK || sqlite3_bind_int(ppstmt, 3, date_day) != SQLITE_OK) {
		LOG_ERROR("Failed to bind value to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	response_array = json_object_new_array();
	
	while((result = sqlite3_step(ppstmt)) == SQLITE_ROW) {
		response_item = json_object_new_object();
		
		json_object_object_add_ex(response_item, "hour", json_object_new_int(sqlite3_column_int(ppstmt, 0)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "second_count", json_object_new_int(sqlite3_column_int(ppstmt, 1)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "active", json_object_new_double(sqlite3_column_double(ppstmt, 2)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "reactive", json_object_new_double(sqlite3_column_double(ppstmt, 3)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "cost", json_object_new_double(sqlite3_column_double(ppstmt, 4)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		json_object_array_add(response_array, response_item);
	}
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to get energy data as hours: %s", sqlite3_errstr(result));
		json_object_put(response_array);
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	*resp_data = strdup(json_object_get_string(response_array));
	
	json_object_put(response_array);
	
	if(*resp_data == NULL)
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	
	*resp_data_size = strlen(*resp_data);
	
	*resp_content_type = strdup(JSON_CONTENT_TYPE);
	
	return MHD_HTTP_OK;
}

unsigned int http_handler_get_energy_days(struct MHD_Connection *conn,
									int logged_user_id,
									path_parameter_t *path_parameters,
									char *req_data,
									size_t req_data_size,
									char **resp_content_type,
									char **resp_data,
									size_t *resp_data_size,
									void *arg) {
	const char *date_year_srt = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "year");
	const char *date_month_srt = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "month");
	int date_year, date_month;
	
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_get_energy_days[] = "SELECT day,second_count,active,reactive,cost FROM energy_days WHERE year = ?1 AND month = ?2;";
	
	json_object *response_array = NULL;
	json_object *response_item = NULL;
	
	if(logged_user_id <= 0)
		return MHD_HTTP_UNAUTHORIZED;
	
	if(date_year_srt == NULL || sscanf(date_year_srt, "%d", &date_year) != 1)
		return MHD_HTTP_BAD_REQUEST;
		
	if(date_month_srt == NULL || sscanf(date_month_srt, "%d", &date_month) != 1)
		return MHD_HTTP_BAD_REQUEST;
	
	if(date_year < 2021 || date_month < 1 || date_month > 12)
		return MHD_HTTP_BAD_REQUEST;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_energy_days, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	if(sqlite3_bind_int(ppstmt, 1, date_year) != SQLITE_OK || sqlite3_bind_int(ppstmt, 2, date_month) != SQLITE_OK) {
		LOG_ERROR("Failed to bind value to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	response_array = json_object_new_array();
	
	while((result = sqlite3_step(ppstmt)) == SQLITE_ROW) {
		response_item = json_object_new_object();
		
		json_object_object_add_ex(response_item, "day", json_object_new_int(sqlite3_column_int(ppstmt, 0)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "second_count", json_object_new_int(sqlite3_column_int(ppstmt, 1)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "active", json_object_new_double(sqlite3_column_double(ppstmt, 2)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "reactive", json_object_new_double(sqlite3_column_double(ppstmt, 3)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "cost", json_object_new_double(sqlite3_column_double(ppstmt, 4)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		json_object_array_add(response_array, response_item);
	}
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to get energy data as days: %s", sqlite3_errstr(result));
		json_object_put(response_array);
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	*resp_data = strdup(json_object_get_string(response_array));
	
	json_object_put(response_array);
	
	if(*resp_data == NULL)
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	
	*resp_data_size = strlen(*resp_data);
	
	*resp_content_type = strdup(JSON_CONTENT_TYPE);
	
	return MHD_HTTP_OK;
}

unsigned int http_handler_get_energy_months(struct MHD_Connection *conn,
											int logged_user_id,
											path_parameter_t *path_parameters,
											char *req_data,
											size_t req_data_size,
											char **resp_content_type,
											char **resp_data,
											size_t *resp_data_size,
											void *arg) {
	const char *date_year_srt = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "year");
	int date_year;
	
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_get_energy_months[] = "SELECT month,second_count,active,reactive,cost FROM energy_months WHERE year = ?1;";
	
	json_object *response_array = NULL;
	json_object *response_item = NULL;
	
	if(logged_user_id <= 0)
		return MHD_HTTP_UNAUTHORIZED;
	
	if(date_year_srt == NULL || sscanf(date_year_srt, "%d", &date_year) != 1 || date_year < 2021)
		return MHD_HTTP_BAD_REQUEST;
	
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_energy_months, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	if(sqlite3_bind_int(ppstmt, 1, date_year) != SQLITE_OK) {
		LOG_ERROR("Failed to bind value to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	response_array = json_object_new_array();
	
	while((result = sqlite3_step(ppstmt)) == SQLITE_ROW) {
		response_item = json_object_new_object();
		
		json_object_object_add_ex(response_item, "month", json_object_new_int(sqlite3_column_int(ppstmt, 0)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "second_count", json_object_new_int(sqlite3_column_int(ppstmt, 1)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "active", json_object_new_double(sqlite3_column_double(ppstmt, 2)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "reactive", json_object_new_double(sqlite3_column_double(ppstmt, 3)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "cost", json_object_new_double(sqlite3_column_double(ppstmt, 4)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		json_object_array_add(response_array, response_item);
	}
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to get energy data as months: %s", sqlite3_errstr(result));
		json_object_put(response_array);
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	*resp_data = strdup(json_object_get_string(response_array));
	
	json_object_put(response_array);
	
	if(*resp_data == NULL)
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	
	*resp_data_size = strlen(*resp_data);
	
	*resp_content_type = strdup(JSON_CONTENT_TYPE);
	
	return MHD_HTTP_OK;
}


unsigned int http_handler_get_disaggregated_energy_minutes(struct MHD_Connection *conn,
															int logged_user_id,
															path_parameter_t *path_parameters,
															char *req_data,
															size_t req_data_size,
															char **resp_content_type,
															char **resp_data,
															size_t *resp_data_size,
															void *arg) {
	
	const char *start_timestamp_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "start");
	const char *end_timestamp_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "end");
	time_t start_timestamp, end_timestamp;
	
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_get_dis_energy_mins[] = "SELECT timestamp, 0 AS appliance_id, energy_minutes.active, energy_months.min_p FROM energy_minutes LEFT JOIN energy_months ON year = strftime('%Y', timestamp, 'unixepoch', 'localtime') AND month = strftime('%m', timestamp, 'unixepoch', 'localtime') WHERE timestamp >= ?1 AND timestamp <= ?2 UNION SELECT timestamp, appliance_id, active, 0 AS min_p FROM disaggregated_energy_minutes WHERE timestamp >= ?1 AND timestamp <= ?2 ORDER BY timestamp;";
	
	time_t timestamp = 0;
	int appliance_id;
	json_object *response_array = NULL;
	json_object *response_item = NULL;
	json_object *appliance_energy_array = NULL;
	
	if(logged_user_id <= 0)
		return MHD_HTTP_UNAUTHORIZED;
	
	if(start_timestamp_str == NULL || sscanf(start_timestamp_str, "%ld", &start_timestamp) != 1)
		return MHD_HTTP_BAD_REQUEST;
	
	if(end_timestamp_str == NULL || sscanf(end_timestamp_str, "%ld", &end_timestamp) != 1)
		end_timestamp = start_timestamp + 24 * 3600;
	
	if(end_timestamp - start_timestamp > 24 * 3600)
		return MHD_HTTP_BAD_REQUEST;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_dis_energy_mins, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	if(sqlite3_bind_int64(ppstmt, 1, start_timestamp) != SQLITE_OK || sqlite3_bind_int64(ppstmt, 2, end_timestamp) != SQLITE_OK) {
		LOG_ERROR("Failed to bind value to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	response_array = json_object_new_array();
	
	while((result = sqlite3_step(ppstmt)) == SQLITE_ROW) {
		if(sqlite3_column_int64(ppstmt, 0) != timestamp) {
			timestamp = sqlite3_column_int64(ppstmt, 0);
			
			response_item = json_object_new_object();
			appliance_energy_array = json_object_new_array();
			
			json_object_object_add_ex(response_item, "timestamp", json_object_new_int64(timestamp), JSON_C_OBJECT_ADD_KEY_IS_NEW);
			json_object_object_add_ex(response_item, "appliance_energy", appliance_energy_array, JSON_C_OBJECT_ADD_KEY_IS_NEW);
			
			json_object_array_add(response_array, response_item);
		}
		
		appliance_id = sqlite3_column_int(ppstmt, 1);
		
		if(appliance_id == 0) {
			json_object_object_add_ex(response_item, "total_energy", json_object_new_double(sqlite3_column_double(ppstmt, 2)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
			json_object_object_add_ex(response_item, "standby_energy", json_object_new_double(sqlite3_column_double(ppstmt, 3) / (60 * 1000)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		} else {
			json_object_array_put_idx(appliance_energy_array, appliance_id - 1, json_object_new_double(sqlite3_column_double(ppstmt, 2)));
		}
	}
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to fetch disaggregated minute energy from database: %s", sqlite3_errstr(result));
		json_object_put(response_array);
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	*resp_data = strdup(json_object_get_string(response_array));
	
	json_object_put(response_array);
	
	if(*resp_data == NULL)
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	
	*resp_data_size = strlen(*resp_data);
	
	*resp_content_type = strdup(JSON_CONTENT_TYPE);
	
	return MHD_HTTP_OK;
}

unsigned int http_handler_get_disaggregated_energy_hours(struct MHD_Connection *conn,
															int logged_user_id,
															path_parameter_t *path_parameters,
															char *req_data,
															size_t req_data_size,
															char **resp_content_type,
															char **resp_data,
															size_t *resp_data_size,
															void *arg) {
	
	const char *date_year_srt = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "year");
	const char *date_month_srt = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "month");
	const char *date_day_srt = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "day");
	int date_year, date_month, date_day;
	
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_get_dis_energy_hours[] = "SELECT hour, 0 AS appliance_id, energy_hours.active, energy_hours.cost, energy_months.min_p FROM energy_hours LEFT JOIN energy_months ON energy_hours.year = energy_months.year AND energy_hours.month = energy_months.month WHERE energy_hours.year = ?1 AND energy_hours.month = ?2 AND energy_hours.day = ?3 UNION SELECT hour, appliance_id, active, cost, 0 AS min_p FROM disaggregated_energy_hours WHERE disaggregated_energy_hours.year = ?1 AND disaggregated_energy_hours.month = ?2 AND disaggregated_energy_hours.day = ?3;";
	
	int hour = -1;
	int appliance_id;
	json_object *response_array = NULL;
	json_object *response_item = NULL;
	json_object *appliance_energy_array = NULL;
	json_object *appliance_cost_array = NULL;
	
	if(logged_user_id <= 0)
		return MHD_HTTP_UNAUTHORIZED;
	
	if(date_year_srt == NULL || sscanf(date_year_srt, "%d", &date_year) != 1)
		return MHD_HTTP_BAD_REQUEST;
		
	if(date_month_srt == NULL || sscanf(date_month_srt, "%d", &date_month) != 1)
		return MHD_HTTP_BAD_REQUEST;
		
	if(date_day_srt == NULL || sscanf(date_day_srt, "%d", &date_day) != 1)
		return MHD_HTTP_BAD_REQUEST;
	
	if(date_year < 2021 || date_month < 1 || date_month > 12 || date_day < 1 || date_day > 31)
		return MHD_HTTP_BAD_REQUEST;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_dis_energy_hours, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	if(sqlite3_bind_int(ppstmt, 1, date_year) != SQLITE_OK || sqlite3_bind_int(ppstmt, 2, date_month) != SQLITE_OK || sqlite3_bind_int(ppstmt, 3, date_day) != SQLITE_OK) {
		LOG_ERROR("Failed to bind value to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	response_array = json_object_new_array();
	
	while((result = sqlite3_step(ppstmt)) == SQLITE_ROW) {
		if(sqlite3_column_int64(ppstmt, 0) != hour) {
			hour = sqlite3_column_int64(ppstmt, 0);
			
			response_item = json_object_new_object();
			appliance_energy_array = json_object_new_array();
			appliance_cost_array = json_object_new_array();
			
			json_object_object_add_ex(response_item, "hour", json_object_new_int(hour), JSON_C_OBJECT_ADD_KEY_IS_NEW);
			json_object_object_add_ex(response_item, "appliance_energy", appliance_energy_array, JSON_C_OBJECT_ADD_KEY_IS_NEW);
			json_object_object_add_ex(response_item, "appliance_cost", appliance_cost_array, JSON_C_OBJECT_ADD_KEY_IS_NEW);
			
			json_object_array_add(response_array, response_item);
		}
		
		appliance_id = sqlite3_column_int(ppstmt, 1);
		
		if(appliance_id == 0) {
			json_object_object_add_ex(response_item, "total_energy", json_object_new_double(sqlite3_column_double(ppstmt, 2)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
			json_object_object_add_ex(response_item, "standby_energy", json_object_new_double(sqlite3_column_double(ppstmt, 4) / 1000), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		} else {
			json_object_array_put_idx(appliance_energy_array, appliance_id - 1, json_object_new_double(sqlite3_column_double(ppstmt, 2)));
			json_object_array_put_idx(appliance_cost_array, appliance_id - 1, json_object_new_double(sqlite3_column_double(ppstmt, 3)));
		}
	}
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to fetch disaggregated hourly energy from database: %s", sqlite3_errstr(result));
		json_object_put(response_array);
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	*resp_data = strdup(json_object_get_string(response_array));
	
	json_object_put(response_array);
	
	if(*resp_data == NULL)
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	
	*resp_data_size = strlen(*resp_data);
	
	*resp_content_type = strdup(JSON_CONTENT_TYPE);
	
	return MHD_HTTP_OK;
}

unsigned int http_handler_get_disaggregated_energy_days(struct MHD_Connection *conn,
															int logged_user_id,
															path_parameter_t *path_parameters,
															char *req_data,
															size_t req_data_size,
															char **resp_content_type,
															char **resp_data,
															size_t *resp_data_size,
															void *arg) {
	
	const char *date_year_srt = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "year");
	const char *date_month_srt = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "month");
	int date_year, date_month;
	
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_get_dis_energy_days[] = "SELECT day, 0 AS appliance_id, energy_days.active, energy_days.cost, energy_months.min_p FROM energy_days LEFT JOIN energy_months ON energy_days.year = energy_days.year AND energy_days.month = energy_months.month WHERE energy_days.year = ?1 AND energy_days.month = ?2 UNION SELECT day, appliance_id, active, cost, 0 AS min_p FROM disaggregated_energy_days WHERE disaggregated_energy_days.year = ?1 AND disaggregated_energy_days.month = ?2;";
	
	int day = -1;
	int appliance_id;
	json_object *response_array = NULL;
	json_object *response_item = NULL;
	json_object *appliance_energy_array = NULL;
	json_object *appliance_cost_array = NULL;
	
	if(logged_user_id <= 0)
		return MHD_HTTP_UNAUTHORIZED;
	
	if(date_year_srt == NULL || sscanf(date_year_srt, "%d", &date_year) != 1)
		return MHD_HTTP_BAD_REQUEST;
	
	if(date_month_srt == NULL || sscanf(date_month_srt, "%d", &date_month) != 1)
		return MHD_HTTP_BAD_REQUEST;
	
	if(date_year < 2021 || date_month < 1 || date_month > 12)
		return MHD_HTTP_BAD_REQUEST;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_dis_energy_days, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	if(sqlite3_bind_int(ppstmt, 1, date_year) != SQLITE_OK || sqlite3_bind_int(ppstmt, 2, date_month) != SQLITE_OK) {
		LOG_ERROR("Failed to bind value to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	response_array = json_object_new_array();
	
	while((result = sqlite3_step(ppstmt)) == SQLITE_ROW) {
		if(sqlite3_column_int64(ppstmt, 0) != day) {
			day = sqlite3_column_int64(ppstmt, 0);
			
			response_item = json_object_new_object();
			appliance_energy_array = json_object_new_array();
			appliance_cost_array = json_object_new_array();
			
			json_object_object_add_ex(response_item, "day", json_object_new_int(day), JSON_C_OBJECT_ADD_KEY_IS_NEW);
			json_object_object_add_ex(response_item, "appliance_energy", appliance_energy_array, JSON_C_OBJECT_ADD_KEY_IS_NEW);
			json_object_object_add_ex(response_item, "appliance_cost", appliance_cost_array, JSON_C_OBJECT_ADD_KEY_IS_NEW);
			
			json_object_array_add(response_array, response_item);
		}
		
		appliance_id = sqlite3_column_int(ppstmt, 1);
		
		if(appliance_id == 0) {
			json_object_object_add_ex(response_item, "total_energy", json_object_new_double(sqlite3_column_double(ppstmt, 2)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
			json_object_object_add_ex(response_item, "standby_energy", json_object_new_double((sqlite3_column_double(ppstmt, 4) * 24) / 1000), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		} else {
			json_object_array_put_idx(appliance_energy_array, appliance_id - 1, json_object_new_double(sqlite3_column_double(ppstmt, 2)));
			json_object_array_put_idx(appliance_cost_array, appliance_id - 1, json_object_new_double(sqlite3_column_double(ppstmt, 3)));
		}
	}
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to fetch disaggregated daily energy from database: %s", sqlite3_errstr(result));
		json_object_put(response_array);
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	*resp_data = strdup(json_object_get_string(response_array));
	
	json_object_put(response_array);
	
	if(*resp_data == NULL)
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	
	*resp_data_size = strlen(*resp_data);
	
	*resp_content_type = strdup(JSON_CONTENT_TYPE);
	
	return MHD_HTTP_OK;
}

unsigned int http_handler_get_disaggregated_energy_months(struct MHD_Connection *conn,
															int logged_user_id,
															path_parameter_t *path_parameters,
															char *req_data,
															size_t req_data_size,
															char **resp_content_type,
															char **resp_data,
															size_t *resp_data_size,
															void *arg) {
	
	const char *date_year_srt = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "year");
	int date_year;
	
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_get_dis_energy_months[] = "SELECT month, 0 AS appliance_id, active, cost, min_p FROM energy_months WHERE energy_months.year = ?1 UNION SELECT month, appliance_id, active, cost, 0 AS min_p FROM disaggregated_energy_months WHERE disaggregated_energy_months.year = ?1;";
	
	int month = -1;
	int appliance_id;
	json_object *response_array = NULL;
	json_object *response_item = NULL;
	json_object *appliance_energy_array = NULL;
	json_object *appliance_cost_array = NULL;
	
	if(logged_user_id <= 0)
		return MHD_HTTP_UNAUTHORIZED;
	
	if(date_year_srt == NULL || sscanf(date_year_srt, "%d", &date_year) != 1)
		return MHD_HTTP_BAD_REQUEST;
	
	if(date_year < 2021)
		return MHD_HTTP_BAD_REQUEST;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_dis_energy_months, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	if(sqlite3_bind_int(ppstmt, 1, date_year) != SQLITE_OK) {
		LOG_ERROR("Failed to bind value to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	response_array = json_object_new_array();
	
	while((result = sqlite3_step(ppstmt)) == SQLITE_ROW) {
		if(sqlite3_column_int64(ppstmt, 0) != month) {
			month = sqlite3_column_int64(ppstmt, 0);
			
			response_item = json_object_new_object();
			appliance_energy_array = json_object_new_array();
			appliance_cost_array = json_object_new_array();
			
			json_object_object_add_ex(response_item, "month", json_object_new_int(month), JSON_C_OBJECT_ADD_KEY_IS_NEW);
			json_object_object_add_ex(response_item, "appliance_energy", appliance_energy_array, JSON_C_OBJECT_ADD_KEY_IS_NEW);
			json_object_object_add_ex(response_item, "appliance_cost", appliance_cost_array, JSON_C_OBJECT_ADD_KEY_IS_NEW);
			
			json_object_array_add(response_array, response_item);
		}
		
		appliance_id = sqlite3_column_int(ppstmt, 1);
		
		if(appliance_id == 0) {
			json_object_object_add_ex(response_item, "total_energy", json_object_new_double(sqlite3_column_double(ppstmt, 2)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
			json_object_object_add_ex(response_item, "standby_energy", json_object_new_double((sqlite3_column_double(ppstmt, 4) * 24 * 30) / 1000), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		} else {
			json_object_array_put_idx(appliance_energy_array, appliance_id - 1, json_object_new_double(sqlite3_column_double(ppstmt, 2)));
			json_object_array_put_idx(appliance_cost_array, appliance_id - 1, json_object_new_double(sqlite3_column_double(ppstmt, 3)));
		}
	}
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to fetch disaggregated monthly energy from database: %s", sqlite3_errstr(result));
		json_object_put(response_array);
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	*resp_data = strdup(json_object_get_string(response_array));
	
	json_object_put(response_array);
	
	if(*resp_data == NULL)
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	
	*resp_data_size = strlen(*resp_data);
	
	*resp_content_type = strdup(JSON_CONTENT_TYPE);
	
	return MHD_HTTP_OK;
}

