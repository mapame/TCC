#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <json-c/json.h>

#include "common.h"
#include "logger.h"
#include "http.h"
#include "config.h"
#include "database.h"
#include "power.h"


unsigned int http_handler_get_dashboard_data(struct MHD_Connection *conn,
												int logged_user_id,
												path_parameter_t *path_parameters,
												char *req_data,
												size_t req_data_size,
												char **resp_content_type,
												char **resp_data,
												size_t *resp_data_size,
												void *arg) {
	int result;
	time_t last_power_timestamp = 0;
	power_data_t pdata[5];
	
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_get_energy_today[] = "SELECT year,month,day,second_count,active,cost FROM energy_days ORDER BY year DESC,month DESC,day DESC LIMIT 1;";
	const char sql_get_energy_thismonth[] = "SELECT second_count,active,cost FROM energy_months ORDER BY year DESC,month DESC LIMIT 1;";
	const char sql_get_energy_dailyavg[] = "SELECT AVG(days.active * days.comp_factor) as avg_active_energy,AVG(days.cost * days.comp_factor) as avg_cost FROM (SELECT (86400 / CAST(second_count AS REAL)) AS comp_factor,active,cost FROM energy_days WHERE second_count > 43200 ORDER BY year DESC,month DESC LIMIT 7 OFFSET 1) AS days;";
	
	json_object *response_object = NULL;
	json_object *response_item = NULL;
	json_object *response_subitem = NULL;
	
	
	if(logged_user_id <= 0)
		return MHD_HTTP_UNAUTHORIZED;
	
	response_object = json_object_new_object();
	
	response_item = json_object_new_array_ext(5);
	
	json_object_object_add_ex(response_object, "power", response_item, JSON_C_OBJECT_ADD_KEY_IS_NEW);
	
	last_power_timestamp = power_get_last_timestamp();
	
	if(last_power_timestamp) {
		result = get_power_data(last_power_timestamp - 4, 0, pdata, 5);
		
		if(result < 0) {
			json_object_put(response_object);
			
			return MHD_HTTP_INTERNAL_SERVER_ERROR;
		}
		
		for(int i = 0; i < 5; i++) {
			response_subitem = json_object_new_object();
			json_object_array_add(response_item, response_subitem);
			
			json_object_object_add_ex(response_subitem, "timestamp", json_object_new_int64(pdata[i].timestamp), JSON_C_OBJECT_ADD_KEY_IS_NEW);
			
			json_object_object_add_ex(response_subitem, "v1", json_object_new_double(pdata[i].v[0]), JSON_C_OBJECT_ADD_KEY_IS_NEW);
			json_object_object_add_ex(response_subitem, "v2", json_object_new_double(pdata[i].v[1]), JSON_C_OBJECT_ADD_KEY_IS_NEW);
			
			json_object_object_add_ex(response_subitem, "p1", json_object_new_double(pdata[i].p[0]), JSON_C_OBJECT_ADD_KEY_IS_NEW);
			json_object_object_add_ex(response_subitem, "p2", json_object_new_double(pdata[i].p[1]), JSON_C_OBJECT_ADD_KEY_IS_NEW);
			
			json_object_object_add_ex(response_subitem, "s1", json_object_new_double(pdata[i].s[0]), JSON_C_OBJECT_ADD_KEY_IS_NEW);
			json_object_object_add_ex(response_subitem, "s2", json_object_new_double(pdata[i].s[1]), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		}
	}
	
	json_object_object_add_ex(response_object, "kwh_rate", json_object_new_double(config_get_value_double("kwh_rate", 0, 10, 0)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		json_object_put(response_object);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_energy_today, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		json_object_put(response_object);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	response_item = json_object_new_object();
	response_subitem = json_object_new_object();
	
	json_object_object_add_ex(response_object, "today", response_item, JSON_C_OBJECT_ADD_KEY_IS_NEW);
	json_object_object_add_ex(response_item, "date", response_subitem, JSON_C_OBJECT_ADD_KEY_IS_NEW);
	
	if((result = sqlite3_step(ppstmt)) == SQLITE_ROW) {
		json_object_object_add_ex(response_subitem, "year", json_object_new_int(sqlite3_column_int(ppstmt, 0)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_subitem, "month", json_object_new_int(sqlite3_column_int(ppstmt, 1)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_subitem, "day", json_object_new_int(sqlite3_column_int(ppstmt, 2)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		json_object_object_add_ex(response_item, "second_count", json_object_new_int(sqlite3_column_int(ppstmt, 3)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "energy", json_object_new_double(sqlite3_column_double(ppstmt, 4)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "cost", json_object_new_double(sqlite3_column_double(ppstmt, 5)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		result = sqlite3_step(ppstmt);
		
	}
	
	sqlite3_finalize(ppstmt);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to get today energy data for overview: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		json_object_put(response_object);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_energy_thismonth, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		json_object_put(response_object);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	response_item = json_object_new_object();
	
	json_object_object_add_ex(response_object, "thismonth", response_item, JSON_C_OBJECT_ADD_KEY_IS_NEW);
	
	if((result = sqlite3_step(ppstmt)) == SQLITE_ROW) {
		
		json_object_object_add_ex(response_item, "second_count", json_object_new_int(sqlite3_column_int(ppstmt, 0)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "energy", json_object_new_double(sqlite3_column_double(ppstmt, 1)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "cost", json_object_new_double(sqlite3_column_double(ppstmt, 2)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		result = sqlite3_step(ppstmt);
		
	}
	
	sqlite3_finalize(ppstmt);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to get energy data for overview: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		json_object_put(response_object);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_energy_dailyavg, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		json_object_put(response_object);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	response_item = json_object_new_object();
	
	json_object_object_add_ex(response_object, "dailyavg", response_item, JSON_C_OBJECT_ADD_KEY_IS_NEW);
	
	if((result = sqlite3_step(ppstmt)) == SQLITE_ROW) {
		
		json_object_object_add_ex(response_item, "energy", json_object_new_double(sqlite3_column_double(ppstmt, 0)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "cost", json_object_new_double(sqlite3_column_double(ppstmt, 1)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		result = sqlite3_step(ppstmt);
		
	}
	
	sqlite3_finalize(ppstmt);
	
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to get daily average energy for overview: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		json_object_put(response_object);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	sqlite3_close(db_conn);
	
	*resp_data = strdup(json_object_get_string(response_object));
	
	json_object_put(response_object);
	
	if(*resp_data == NULL)
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	
	*resp_data_size = strlen(*resp_data);
	
	*resp_content_type = strdup(JSON_CONTENT_TYPE);
	
	return MHD_HTTP_OK;
}
