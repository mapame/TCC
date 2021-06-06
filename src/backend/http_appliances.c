#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <json-c/json.h>

#include "http.h"
#include "database.h"
#include "logger.h"
#include "users.h"

static int check_appliance_id(int appliance_id) {
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_check_appliance_id[] = "SELECT id FROM appliances WHERE id = ?1;";
	int count = 0;
	
	if(appliance_id <= 0)
		return 0;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_check_appliance_id, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	if(sqlite3_bind_int(ppstmt, 1, appliance_id) != SQLITE_OK) {
		LOG_ERROR("Failed to bind appliance ID to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	while((result = sqlite3_step(ppstmt)) == SQLITE_ROW)
		count++;
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to read appliance signatures: %s", sqlite3_errstr(result));
		
		return -1;
	}
	
	return count;
}

unsigned int http_handler_get_appliance_list(struct MHD_Connection *conn,
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
	const char sql_get_appliances[] = "SELECT appliances.id,name,appliances.is_active,power,is_hardwired,appliances.creation_date,appliances.modification_date,IFNULL(signatures.qty, 0) AS signature_qty,signatures.avgp AS signatures_avg_p FROM appliances LEFT JOIN (SELECT appliance_id,COUNT(*) AS qty,AVG(ABS(peak_pt)) AS avgp FROM signatures GROUP BY appliance_id) AS signatures ON signatures.appliance_id = appliances.id;";
	
	struct json_object* json_response = NULL;
	struct json_object* json_appliance_item = NULL;
	
	if(logged_user_id <= 0)
		return MHD_HTTP_UNAUTHORIZED;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_appliances, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	json_response = json_object_new_array();
	
	while((result = sqlite3_step(ppstmt)) == SQLITE_ROW) {
		json_appliance_item = json_object_new_object();
		
		json_object_array_add(json_response, json_appliance_item);
		
		json_object_object_add_ex(json_appliance_item, "id", json_object_new_int(sqlite3_column_int(ppstmt, 0)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		if((str_ptr = (const char*) sqlite3_column_text(ppstmt, 1)) == NULL)
			break;
		
		json_object_object_add_ex(json_appliance_item, "name", json_object_new_string(str_ptr), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(json_appliance_item, "is_active", json_object_new_boolean(sqlite3_column_int(ppstmt, 2)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(json_appliance_item, "power", json_object_new_double(sqlite3_column_double(ppstmt, 3)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(json_appliance_item, "is_hardwired", json_object_new_boolean(sqlite3_column_int(ppstmt, 4)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(json_appliance_item, "creation_date", json_object_new_int64(sqlite3_column_int64(ppstmt, 5)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(json_appliance_item, "modification_date", json_object_new_int64(sqlite3_column_int64(ppstmt, 6)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(json_appliance_item, "signature_qty", json_object_new_int(sqlite3_column_int(ppstmt, 7)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(json_appliance_item, "signatures_avg_p", json_object_new_double(sqlite3_column_double(ppstmt, 8)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
	}
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to read appliances: %s", sqlite3_errstr(result));
		
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

unsigned int http_handler_get_appliance(struct MHD_Connection *conn,
											int logged_user_id,
											path_parameter_t *path_parameters,
											char *req_data,
											size_t req_data_size,
											char **resp_content_type,
											char **resp_data,
											size_t *resp_data_size,
											void *arg) {
	
	const char *appliance_id_str;
	int appliance_id;
	
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char *str_ptr;
	const char sql_get_appliance[] = "SELECT name,is_active,power,is_hardwired,creation_date,modification_date,signature_qty,signatures_avg_p FROM appliances,(SELECT COUNT(*) AS signature_qty,AVG(ABS(delta_pt)) AS signatures_avg_p FROM signatures WHERE appliance_id = ?1) WHERE id = ?1;";
	int count = 0;
	
	struct json_object* json_response = NULL;
	
	if(logged_user_id <= 0)
		return MHD_HTTP_UNAUTHORIZED;
	
	if((appliance_id_str = http_parameter_get_value(path_parameters, 2)) == NULL)
		return MHD_HTTP_BAD_REQUEST;
	
	if(sscanf(appliance_id_str, "%d", &appliance_id) != 1 || appliance_id <= 0)
		return MHD_HTTP_BAD_REQUEST;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_appliance, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	if((result = sqlite3_bind_int(ppstmt, 1, appliance_id)) != SQLITE_OK) {
		LOG_ERROR("Failed to bind value to prepared statement: %s", sqlite3_errstr(result));
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	json_response = json_object_new_object();
	
	while((result = sqlite3_step(ppstmt)) == SQLITE_ROW) {
		if(count++)
			continue;
		
		json_object_object_add_ex(json_response, "id", json_object_new_int(appliance_id), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		if((str_ptr = (const char*) sqlite3_column_text(ppstmt, 0)) == NULL)
			break;
		
		json_object_object_add_ex(json_response, "name", json_object_new_string(str_ptr), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(json_response, "is_active", json_object_new_boolean(sqlite3_column_int(ppstmt, 1)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(json_response, "power", json_object_new_double(sqlite3_column_double(ppstmt, 2)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(json_response, "is_hardwired", json_object_new_boolean(sqlite3_column_int(ppstmt, 3)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(json_response, "creation_date", json_object_new_int64(sqlite3_column_int64(ppstmt, 4)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(json_response, "modification_date", json_object_new_int64(sqlite3_column_int64(ppstmt, 5)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(json_response, "signature_qty", json_object_new_int(sqlite3_column_int(ppstmt, 6)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(json_response, "signatures_avg_p", json_object_new_int(sqlite3_column_int(ppstmt, 7)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
	}
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to read appliances: %s", sqlite3_errstr(result));
		
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

unsigned int http_handler_create_appliance(struct MHD_Connection *conn,
											int logged_user_id,
											path_parameter_t *path_parameters,
											char *req_data,
											size_t req_data_size,
											char **resp_content_type,
											char **resp_data,
											size_t *resp_data_size,
											void *arg) {
	
	struct json_object *received_json;
	struct json_object *json_name, *json_is_active, *json_power, *json_is_hardwired;
	
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_get_appliance[] = "INSERT INTO appliances(name,is_active,power,is_hardwired,creation_date,modification_date) VALUES(?1, ?2, ?3, ?4, ?5, ?5);";
	int new_appliance_id;
	
	if(logged_user_id <= 0 || users_check_admin(logged_user_id) == 0)
		return MHD_HTTP_UNAUTHORIZED;
	
	if(req_data == NULL)
		return MHD_HTTP_BAD_REQUEST;
	
	received_json = json_tokener_parse(req_data);
	
	if(received_json == NULL)
		return MHD_HTTP_BAD_REQUEST;
	
	json_object_object_get_ex(received_json, "name", &json_name);
	json_object_object_get_ex(received_json, "is_active", &json_is_active);
	json_object_object_get_ex(received_json, "power", &json_power);
	json_object_object_get_ex(received_json, "is_hardwired", &json_is_hardwired);
	
	if(json_object_get_type(json_name) != json_type_string || json_object_get_string_len(json_name) < 3 ||  json_object_get_string_len(json_name) > 50
		|| json_object_get_type(json_is_active) != json_type_boolean
		|| (json_object_get_type(json_power) != json_type_double && json_object_get_type(json_power) != json_type_int) || json_object_get_double(json_power) < 0
		|| json_object_get_type(json_is_hardwired) != json_type_boolean) {
		
		json_object_put(received_json);
		
		return MHD_HTTP_BAD_REQUEST;
	}
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		json_object_put(received_json);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_appliance, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		json_object_put(received_json);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	// SQLITE_OK é zero, então somando todos os resultados podemos saber se algum falhou
	result = sqlite3_bind_text(ppstmt, 1, json_object_get_string(json_name), -1, SQLITE_STATIC);
	result += sqlite3_bind_int(ppstmt, 2, json_object_get_boolean(json_is_active));
	result += sqlite3_bind_double(ppstmt, 3, json_object_get_double(json_power));
	result += sqlite3_bind_int(ppstmt, 4, json_object_get_boolean(json_is_hardwired));
	result += sqlite3_bind_int64(ppstmt, 5, time(NULL));
	
	if(result) {
		LOG_ERROR("Failed to bind appliance values to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		json_object_put(received_json);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	result = sqlite3_step(ppstmt);
	
	new_appliance_id = sqlite3_last_insert_rowid(db_conn);
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	json_object_put(received_json);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to add new appliance: %s", sqlite3_errstr(result));
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	if((*resp_data = malloc(sizeof(char) * 128)) == NULL)
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	
	if(new_appliance_id > 0)
		*resp_data_size = sprintf(*resp_data, "{\"result\":\"success\",\"appliance_id\":%d}", new_appliance_id);
	else
		*resp_data_size = sprintf(*resp_data, "{\"result\":\"failed\"}");
	
	*resp_content_type = strdup(JSON_CONTENT_TYPE);
	
	return MHD_HTTP_OK;
}

unsigned int http_handler_update_appliance(struct MHD_Connection *conn,
											int logged_user_id,
											path_parameter_t *path_parameters,
											char *req_data,
											size_t req_data_size,
											char **resp_content_type,
											char **resp_data,
											size_t *resp_data_size,
											void *arg) {
	const char *appliance_id_str;
	int appliance_id;
	
	struct json_object *received_json;
	struct json_object *json_name, *json_is_active, *json_power, *json_is_hardwired;
	
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_update_appliance[] = "UPDATE appliances SET (name,is_active,power,is_hardwired,modification_date) = (IFNULL(?2, current_values.name), IFNULL(?3, current_values.is_active), IFNULL(?4, current_values.power), IFNULL(?5, current_values.is_hardwired), ?6) FROM (SELECT name,is_active,power,is_hardwired FROM appliances WHERE id = ?1) AS current_values WHERE id = ?1;";
	int changes;
	
	if(logged_user_id <= 0 || users_check_admin(logged_user_id) == 0)
		return MHD_HTTP_UNAUTHORIZED;
	
	if((appliance_id_str = http_parameter_get_value(path_parameters, 2)) == NULL)
		return MHD_HTTP_BAD_REQUEST;
	
	if(sscanf(appliance_id_str, "%d", &appliance_id) != 1 || appliance_id <= 0)
		return MHD_HTTP_BAD_REQUEST;
	
	if(req_data == NULL)
		return MHD_HTTP_BAD_REQUEST;
	
	received_json = json_tokener_parse(req_data);
	
	if(received_json == NULL)
		return MHD_HTTP_BAD_REQUEST;
	
	json_object_object_get_ex(received_json, "name", &json_name);
	json_object_object_get_ex(received_json, "is_active", &json_is_active);
	json_object_object_get_ex(received_json, "power", &json_power);
	json_object_object_get_ex(received_json, "is_hardwired", &json_is_hardwired);
	
	if((json_name && (json_object_get_type(json_name) != json_type_string || json_object_get_string_len(json_name) < 3 ||  json_object_get_string_len(json_name) > 50))
		|| (json_is_active && json_object_get_type(json_is_active) != json_type_boolean)
		|| (json_power && ((json_object_get_type(json_power) != json_type_double && json_object_get_type(json_power) != json_type_int) || json_object_get_double(json_power) < 0))
		|| (json_is_hardwired && json_object_get_type(json_is_hardwired) != json_type_boolean)) {
		
		json_object_put(received_json);
		
		return MHD_HTTP_BAD_REQUEST;
	}
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		json_object_put(received_json);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_update_appliance, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		json_object_put(received_json);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	result = sqlite3_bind_int(ppstmt, 1, appliance_id);
	
	if(json_name)
		result += sqlite3_bind_text(ppstmt, 2, json_object_get_string(json_name), -1, SQLITE_STATIC);
	else
		result += sqlite3_bind_null(ppstmt, 2);
	
	if(json_is_active)
		result += sqlite3_bind_int(ppstmt, 3, json_object_get_boolean(json_is_active));
	else
		result += sqlite3_bind_null(ppstmt, 3);
	
	if(json_power)
		result += sqlite3_bind_double(ppstmt, 4, json_object_get_double(json_power));
	else
		result += sqlite3_bind_null(ppstmt, 5);
	
	if(json_is_hardwired)
		result += sqlite3_bind_int(ppstmt, 5, json_object_get_boolean(json_is_hardwired));
	else
		result += sqlite3_bind_null(ppstmt, 5);
	
	result += sqlite3_bind_int64(ppstmt, 6, time(NULL));
	
	if(result) {
		LOG_ERROR("Failed to bind appliance values to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		json_object_put(received_json);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	result = sqlite3_step(ppstmt);
	
	changes = sqlite3_changes(db_conn);
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	json_object_put(received_json);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to modify appliance: %s", sqlite3_errstr(result));
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	if(changes == 1) {
		*resp_data = strdup("{\"result\":\"success\"}");
		
		if(*resp_data)
			*resp_data_size = strlen(*resp_data);
		else
			return MHD_HTTP_INTERNAL_SERVER_ERROR;
		
	} else {
		return MHD_HTTP_NOT_FOUND;
	}
	
	*resp_content_type = strdup(JSON_CONTENT_TYPE);
	
	return MHD_HTTP_OK;
}

/*
 * 
 * 
 * Appliance Signatures
 * 
 * 
 */

unsigned int http_handler_get_appliance_signature_list(struct MHD_Connection *conn,
														int logged_user_id,
														path_parameter_t *path_parameters,
														char *req_data,
														size_t req_data_size,
														char **resp_content_type,
														char **resp_data,
														size_t *resp_data_size,
														void *arg) {
	int appliance_id = 0;
	
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_get_signatures[] = "SELECT timestamp,appliance_id,creator_id,date_added,delta_pt,peak_pt,delta_pa,delta_pb,delta_sa,delta_sb,delta_qa,delta_qb FROM signatures WHERE NOT ?1 OR appliance_id = ?1;";
	
	struct json_object* json_response = NULL;
	struct json_object* json_signature_item = NULL;
	struct json_object* json_power_data_array = NULL;
	
	if(logged_user_id <= 0)
		return MHD_HTTP_UNAUTHORIZED;
	
	if(arg && strcmp((const char*)arg, "has_appliance_id") == 0) {
		const char *appliance_id_str;
		
		if((appliance_id_str = http_parameter_get_value(path_parameters, 2)) == NULL)
			return MHD_HTTP_BAD_REQUEST;
		
		if(sscanf(appliance_id_str, "%d", &appliance_id) != 1 || appliance_id <= 0)
			return MHD_HTTP_BAD_REQUEST;
		
		if((result = check_appliance_id(appliance_id)) < 0)
			return MHD_HTTP_INTERNAL_SERVER_ERROR;
		else if(result == 0)
			return MHD_HTTP_NOT_FOUND;
	}
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_signatures, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	if(sqlite3_bind_int(ppstmt, 1, appliance_id) != SQLITE_OK) {
		LOG_ERROR("Failed to bind appliance ID to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	json_response = json_object_new_array();
	
	while((result = sqlite3_step(ppstmt)) == SQLITE_ROW) {
		json_signature_item = json_object_new_object();
		
		json_object_array_add(json_response, json_signature_item);
		
		json_object_object_add_ex(json_signature_item, "timestamp", json_object_new_int64(sqlite3_column_int64(ppstmt, 0)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(json_signature_item, "appliance_id", json_object_new_int(sqlite3_column_int(ppstmt, 1)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(json_signature_item, "creator_id", json_object_new_int(sqlite3_column_int(ppstmt, 2)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(json_signature_item, "date_added", json_object_new_int64(sqlite3_column_int64(ppstmt, 3)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		json_object_object_add_ex(json_signature_item, "delta_pt", json_object_new_double(sqlite3_column_double(ppstmt, 4)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(json_signature_item, "peak_pt", json_object_new_double(sqlite3_column_double(ppstmt, 5)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		json_power_data_array = json_object_new_array_ext(2);
		json_object_object_add_ex(json_signature_item, "delta_p", json_power_data_array, JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_array_add(json_power_data_array, json_object_new_double(sqlite3_column_double(ppstmt, 6)));
		json_object_array_add(json_power_data_array, json_object_new_double(sqlite3_column_double(ppstmt, 7)));
		
		json_power_data_array = json_object_new_array_ext(2);
		json_object_object_add_ex(json_signature_item, "delta_s", json_power_data_array, JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_array_add(json_power_data_array, json_object_new_double(sqlite3_column_double(ppstmt, 8)));
		json_object_array_add(json_power_data_array, json_object_new_double(sqlite3_column_double(ppstmt, 9)));
		
		json_power_data_array = json_object_new_array_ext(2);
		json_object_object_add_ex(json_signature_item, "delta_q", json_power_data_array, JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_array_add(json_power_data_array, json_object_new_double(sqlite3_column_double(ppstmt, 10)));
		json_object_array_add(json_power_data_array, json_object_new_double(sqlite3_column_double(ppstmt, 11)));
	}
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to read appliance signatures: %s", sqlite3_errstr(result));
		
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

unsigned int http_handler_get_appliance_signature(struct MHD_Connection *conn,
													int logged_user_id,
													path_parameter_t *path_parameters,
													char *req_data,
													size_t req_data_size,
													char **resp_content_type,
													char **resp_data,
													size_t *resp_data_size,
													void *arg) {
	
	const char *signature_timestamp_str;
	time_t signature_timestamp;
	
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_get_signature[] = "SELECT appliance_id,creator_id,date_added,delta_pt,peak_pt,delta_pa,delta_pb,delta_sa,delta_sb,delta_qa,delta_qb FROM signatures WHERE timestamp = ?1;";
	int count = 0;
	
	struct json_object *json_response;
	struct json_object *json_power_data_array;
	
	if(logged_user_id <= 0)
		return MHD_HTTP_UNAUTHORIZED;
	
	if((signature_timestamp_str = http_parameter_get_value(path_parameters, 3)) == NULL)
		return MHD_HTTP_BAD_REQUEST;
	
	if(sscanf(signature_timestamp_str, "%ld", &signature_timestamp) != 1 || signature_timestamp <= 0)
		return MHD_HTTP_BAD_REQUEST;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_signature, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	if((result = sqlite3_bind_int64(ppstmt, 1, signature_timestamp)) != SQLITE_OK) {
		LOG_ERROR("Failed to bind value to prepared statement: %s", sqlite3_errstr(result));
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	json_response = json_object_new_object();
	
	while((result = sqlite3_step(ppstmt)) == SQLITE_ROW) {
		if(count++)
			continue;
		
		json_object_object_add_ex(json_response, "timestamp", json_object_new_int64(signature_timestamp), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(json_response, "appliance_id", json_object_new_int(sqlite3_column_int(ppstmt, 0)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(json_response, "creator_id", json_object_new_int(sqlite3_column_int(ppstmt, 1)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(json_response, "date_added", json_object_new_int64(sqlite3_column_int64(ppstmt, 2)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		json_object_object_add_ex(json_response, "delta_pt", json_object_new_double(sqlite3_column_double(ppstmt, 3)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(json_response, "peak_pt", json_object_new_double(sqlite3_column_double(ppstmt, 4)), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		json_power_data_array = json_object_new_array_ext(2);
		json_object_object_add_ex(json_response, "delta_p", json_power_data_array, JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_array_add(json_power_data_array, json_object_new_double(sqlite3_column_double(ppstmt, 5)));
		json_object_array_add(json_power_data_array, json_object_new_double(sqlite3_column_double(ppstmt, 6)));
		
		json_power_data_array = json_object_new_array_ext(2);
		json_object_object_add_ex(json_response, "delta_s", json_power_data_array, JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_array_add(json_power_data_array, json_object_new_double(sqlite3_column_double(ppstmt, 7)));
		json_object_array_add(json_power_data_array, json_object_new_double(sqlite3_column_double(ppstmt, 8)));
		
		json_power_data_array = json_object_new_array_ext(2);
		json_object_object_add_ex(json_response, "delta_q", json_power_data_array, JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_array_add(json_power_data_array, json_object_new_double(sqlite3_column_double(ppstmt, 9)));
		json_object_array_add(json_power_data_array, json_object_new_double(sqlite3_column_double(ppstmt, 10)));
	}
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to read appliances: %s", sqlite3_errstr(result));
		
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

unsigned int http_handler_add_appliance_signatures(struct MHD_Connection *conn,
													int logged_user_id,
													path_parameter_t *path_parameters,
													char *req_data,
													size_t req_data_size,
													char **resp_content_type,
													char **resp_data,
													size_t *resp_data_size,
													void *arg) {
	
	const char *appliance_id_str;
	int appliance_id = 0;
	
	int item_count;
	struct json_object *received_json;
	struct json_object *received_json_item;
	struct json_object *json_timestamp, *json_delta_pt, *json_peak_pt, *json_delta_p, *json_delta_s, *json_delta_q;
	struct json_object *json_delta_pa, *json_delta_pb, *json_delta_sa, *json_delta_sb, *json_delta_qa, *json_delta_qb;
	
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_insert_signature[] = "INSERT OR REPLACE INTO signatures(appliance_id,creator_id,date_added,timestamp,delta_pt,peak_pt,delta_pa,delta_pb,delta_sa,delta_sb,delta_qa,delta_qb) VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12);";
	int insert_counter = 0;
	
	if(logged_user_id <= 0 || users_check_admin(logged_user_id) == 0)
		return MHD_HTTP_UNAUTHORIZED;
	
	if((appliance_id_str = http_parameter_get_value(path_parameters, 2)) == NULL)
		return MHD_HTTP_BAD_REQUEST;
	
	if(sscanf(appliance_id_str, "%d", &appliance_id) != 1 || appliance_id <= 0)
		return MHD_HTTP_BAD_REQUEST;
	
	if((result = check_appliance_id(appliance_id)) < 0)
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	else if(result == 0)
		return MHD_HTTP_NOT_FOUND;
	
	if(req_data == NULL)
		return MHD_HTTP_BAD_REQUEST;
	
	received_json = json_tokener_parse(req_data);
	
	if(received_json == NULL)
		return MHD_HTTP_BAD_REQUEST;
	
	if(json_object_get_type(received_json) != json_type_array || (item_count = json_object_array_length(received_json)) < 1) {
		json_object_put(received_json);
		
		return MHD_HTTP_BAD_REQUEST;
	}
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		json_object_put(received_json);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_insert_signature, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		json_object_put(received_json);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	result = sqlite3_bind_int(ppstmt, 1, appliance_id);
	result += sqlite3_bind_int(ppstmt, 2, logged_user_id);
	result += sqlite3_bind_int64(ppstmt, 3, time(NULL));
	
	if(result) {
		LOG_ERROR("Failed to bind appliance values to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		json_object_put(received_json);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	for(int index = 0; index < item_count; index++) {
		received_json_item = json_object_array_get_idx(received_json, index);
		
		if(json_object_get_type(received_json_item) != json_type_object) {
			sqlite3_finalize(ppstmt);
			sqlite3_close(db_conn);
			json_object_put(received_json);
			
			return MHD_HTTP_BAD_REQUEST;
		}
		
		json_object_object_get_ex(received_json_item, "timestamp", &json_timestamp);
		json_object_object_get_ex(received_json_item, "delta_pt", &json_delta_pt);
		json_object_object_get_ex(received_json_item, "peak_pt", &json_peak_pt);
		json_object_object_get_ex(received_json_item, "delta_p", &json_delta_p);
		json_object_object_get_ex(received_json_item, "delta_s", &json_delta_s);
		json_object_object_get_ex(received_json_item, "delta_q", &json_delta_q);
		
		if(json_object_get_type(json_timestamp) != json_type_int || json_object_get_int64(json_timestamp) <= 0
			|| json_object_get_type(json_delta_pt) != json_type_double
			|| json_object_get_type(json_peak_pt) != json_type_double
			|| json_object_get_type(json_delta_p) != json_type_array || json_object_array_length(json_delta_p) != 2
			|| json_object_get_type(json_delta_s) != json_type_array || json_object_array_length(json_delta_s) != 2
			|| json_object_get_type(json_delta_q) != json_type_array || json_object_array_length(json_delta_q) != 2
			|| json_object_get_type(json_delta_pa = json_object_array_get_idx(json_delta_p, 0)) != json_type_double
			|| json_object_get_type(json_delta_pb = json_object_array_get_idx(json_delta_p, 1)) != json_type_double
			|| json_object_get_type(json_delta_sa = json_object_array_get_idx(json_delta_s, 0)) != json_type_double
			|| json_object_get_type(json_delta_sb = json_object_array_get_idx(json_delta_s, 1)) != json_type_double
			|| json_object_get_type(json_delta_qa = json_object_array_get_idx(json_delta_q, 0)) != json_type_double
			|| json_object_get_type(json_delta_qb = json_object_array_get_idx(json_delta_q, 1)) != json_type_double
			) {
			
			sqlite3_finalize(ppstmt);
			sqlite3_close(db_conn);
			json_object_put(received_json);
			
			return MHD_HTTP_BAD_REQUEST;
		}
		
		// SQLITE_OK é zero, então somando todos os resultados podemos saber se algum falhou
		result = sqlite3_bind_int64(ppstmt, 4, json_object_get_int64(json_timestamp));
		result += sqlite3_bind_int(ppstmt, 5, json_object_get_double(json_delta_pt));
		result += sqlite3_bind_int(ppstmt, 6, json_object_get_double(json_peak_pt));
		result += sqlite3_bind_int(ppstmt, 7, json_object_get_double(json_delta_pa));
		result += sqlite3_bind_int(ppstmt, 8, json_object_get_double(json_delta_pb));
		result += sqlite3_bind_int(ppstmt, 9, json_object_get_double(json_delta_sa));
		result += sqlite3_bind_int(ppstmt, 10, json_object_get_double(json_delta_sb));
		result += sqlite3_bind_int(ppstmt, 11, json_object_get_double(json_delta_qa));
		result += sqlite3_bind_int(ppstmt, 12, json_object_get_double(json_delta_qb));
		
		if(result) {
			LOG_ERROR("Failed to bind appliance values to prepared statement.");
			sqlite3_finalize(ppstmt);
			sqlite3_close(db_conn);
			json_object_put(received_json);
			
			return MHD_HTTP_INTERNAL_SERVER_ERROR;
		}
		
		if((result = sqlite3_step(ppstmt)) != SQLITE_DONE) {
			LOG_ERROR("Failed to add new appliance signature: %s", sqlite3_errstr(result));
			sqlite3_finalize(ppstmt);
			sqlite3_close(db_conn);
			json_object_put(received_json);
			
			return MHD_HTTP_INTERNAL_SERVER_ERROR;
		}
		
		insert_counter += sqlite3_changes(db_conn);
		
		sqlite3_reset(ppstmt);
	}
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	json_object_put(received_json);
	
	if((*resp_data = malloc(sizeof(char) * 128)) == NULL)
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	
	if(insert_counter == item_count)
		*resp_data_size = sprintf(*resp_data, "{\"result\":\"success\",\"quantity\":%d}", insert_counter);
	else
		*resp_data_size = sprintf(*resp_data, "{\"result\":\"failed\"}");
	
	*resp_content_type = strdup(JSON_CONTENT_TYPE);
	
	return MHD_HTTP_OK;
}

unsigned int http_handler_delete_appliance_signature(struct MHD_Connection *conn,
													int logged_user_id,
													path_parameter_t *path_parameters,
													char *req_data,
													size_t req_data_size,
													char **resp_content_type,
													char **resp_data,
													size_t *resp_data_size,
													void *arg) {
	
	const char *signature_timestamp_str;
	time_t signature_timestamp;
	
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_delete_signature[] = "DELETE FROM signatures WHERE timestamp = ?1;";
	int changes = 0;
	
	if(logged_user_id <= 0)
		return MHD_HTTP_UNAUTHORIZED;
	
	if((signature_timestamp_str = http_parameter_get_value(path_parameters, 3)) == NULL)
		return MHD_HTTP_BAD_REQUEST;
	
	if(sscanf(signature_timestamp_str, "%ld", &signature_timestamp) != 1 || signature_timestamp <= 0)
		return MHD_HTTP_BAD_REQUEST;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_delete_signature, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	if((result = sqlite3_bind_int64(ppstmt, 1, signature_timestamp)) != SQLITE_OK) {
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
		LOG_ERROR("Failed to delete appliance signature: %s", sqlite3_errstr(result));
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	if(changes == 0)
		return MHD_HTTP_NOT_FOUND;
	
	return MHD_HTTP_OK;
}
