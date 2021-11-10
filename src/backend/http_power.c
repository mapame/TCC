#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <json-c/json.h>

#include "common.h"
#include "logger.h"
#include "http.h"
#include "power.h"
#include "disaggregation.h"

enum power_get_type {
	POWER_GET_PT,
	POWER_GET_PV,
	POWER_GET_V,
	POWER_GET_I,
	POWER_GET_P,
	POWER_GET_S,
	POWER_GET_Q
};

unsigned int http_handler_get_power_data(struct MHD_Connection *conn,
										int logged_user_id,
										path_parameter_t *path_parameters,
										char *req_data,
										size_t req_data_size,
										char **resp_content_type,
										char **resp_data,
										size_t *resp_data_size,
										void *arg) {
	
	const char *type_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "type");
	const char *last_secs_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "last");
	const char *start_timestamp_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "start");
	const char *end_timestamp_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "end");
	
	enum power_get_type type;
	int last_secs;
	time_t start_timestamp, end_timestamp;
	
	power_data_t *pd_buffer = NULL;
	int pd_qty;
	
	if(logged_user_id <= 0)
		return MHD_HTTP_UNAUTHORIZED;
	
	if(type_str == NULL || !strcmp(type_str, "pt"))
		type = POWER_GET_PT;
	else if(!strcmp(type_str, "pv"))
		type = POWER_GET_PV;
	else if(!strcmp(type_str, "v"))
		type = POWER_GET_V;
	else if(!strcmp(type_str, "p"))
		type = POWER_GET_P;
	else if(!strcmp(type_str, "i"))
		type = POWER_GET_I;
	else if(!strcmp(type_str, "s"))
		type = POWER_GET_S;
	else if(!strcmp(type_str, "q"))
		type = POWER_GET_Q;
	else
		return MHD_HTTP_BAD_REQUEST;
	
	if(last_secs_str) {
		if(sscanf(last_secs_str, "%d", &last_secs) != 1)
			return MHD_HTTP_BAD_REQUEST;
		
		if(last_secs < 0 || last_secs > 12 * 3600)
			return MHD_HTTP_BAD_REQUEST;
		
		end_timestamp = power_get_last_timestamp();
		start_timestamp = end_timestamp - last_secs;
		
	} else if(start_timestamp_str && end_timestamp_str) {
		if(sscanf(start_timestamp_str, "%ld", &start_timestamp) != 1 || sscanf(end_timestamp_str, "%ld", &end_timestamp) != 1)
			return MHD_HTTP_BAD_REQUEST;
		
		if(end_timestamp <= 0 || start_timestamp<= 0 || end_timestamp < start_timestamp || end_timestamp - start_timestamp > 12 * 3600)
			return MHD_HTTP_BAD_REQUEST;
		
	} else {
		return MHD_HTTP_BAD_REQUEST;
	}
	
	pd_buffer = (power_data_t*) malloc(sizeof(power_data_t) * (1 + end_timestamp - start_timestamp));
	
	if(pd_buffer == NULL)
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	
	pd_qty = get_power_data(start_timestamp, end_timestamp, pd_buffer, (1 + end_timestamp - start_timestamp));
	
	if(pd_qty < 0) {
		free(pd_buffer);
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	// Gerar o JSON de resposta diretamente em texto neste caso é mais fácil e eficiente.
	*resp_data = (char*) malloc(sizeof(char) * (3 + pd_qty * 42));
	
	if(*resp_data == NULL) {
		free(pd_buffer);
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	*resp_data[0] = '[';
	*resp_data_size = 1;
	
	for(int i = 0; i < pd_qty; i++) {
		if(type == POWER_GET_PT)
			*resp_data_size += sprintf(&(*resp_data)[*resp_data_size], "[%ld,%.2lf],", pd_buffer[i].timestamp, (pd_buffer[i].p[0] + pd_buffer[i].p[1]));
		else if(type == POWER_GET_PV)
			*resp_data_size += sprintf(&(*resp_data)[*resp_data_size], "[%ld,%.2lf,%.2lf,%.2lf,%.2lf],", pd_buffer[i].timestamp, pd_buffer[i].p[0], pd_buffer[i].p[1], pd_buffer[i].v[0], pd_buffer[i].v[1]);
		else if(type == POWER_GET_V)
			*resp_data_size += sprintf(&(*resp_data)[*resp_data_size], "[%ld,%.2lf,%.2lf],", pd_buffer[i].timestamp, pd_buffer[i].v[0], pd_buffer[i].v[1]);
		else if(type == POWER_GET_P)
			*resp_data_size += sprintf(&(*resp_data)[*resp_data_size], "[%ld,%.2lf,%.2lf],", pd_buffer[i].timestamp, pd_buffer[i].p[0], pd_buffer[i].p[1]);
		else if(type == POWER_GET_I)
			*resp_data_size += sprintf(&(*resp_data)[*resp_data_size], "[%ld,%.2lf,%.2lf],", pd_buffer[i].timestamp, pd_buffer[i].i[0], pd_buffer[i].i[1]);
		else if(type == POWER_GET_S)
			*resp_data_size += sprintf(&(*resp_data)[*resp_data_size], "[%ld,%.2lf,%.2lf],", pd_buffer[i].timestamp, pd_buffer[i].s[0], pd_buffer[i].s[1]);
		else if(type == POWER_GET_Q)
			*resp_data_size += sprintf(&(*resp_data)[*resp_data_size], "[%ld,%.2lf,%.2lf],", pd_buffer[i].timestamp, pd_buffer[i].q[0], pd_buffer[i].q[1]);
	}
	
	free(pd_buffer);
	
	if(pd_qty) {
		(*resp_data)[*resp_data_size - 1] = ']';
	} else {
		(*resp_data)[*resp_data_size] = ']';
		(*resp_data_size)++;
	}
	
	*resp_data_size *= sizeof(char);
	
	*resp_content_type = strdup(JSON_CONTENT_TYPE);
	
	return MHD_HTTP_OK;
}

unsigned int http_handler_get_load_events(struct MHD_Connection *conn,
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
	
	json_object *response_array = NULL;
	json_object *response_item = NULL;
	json_object *appliance_array = NULL;
	
	load_event_t *loadev_buffer = NULL;
	int qty;
	
	if(logged_user_id <= 0)
		return MHD_HTTP_UNAUTHORIZED;
	
	if(last_secs_str) {
		if(sscanf(last_secs_str, "%d", &last_secs) != 1)
			return MHD_HTTP_BAD_REQUEST;
		
		if(last_secs < 0 || last_secs > 12 * 3600)
			return MHD_HTTP_BAD_REQUEST;
		
		end_timestamp = power_get_last_timestamp();
		start_timestamp = end_timestamp - last_secs;
		
	} else if(start_timestamp_str && end_timestamp_str) {
		if(sscanf(start_timestamp_str, "%ld", &start_timestamp) != 1 || sscanf(end_timestamp_str, "%ld", &end_timestamp) != 1)
			return MHD_HTTP_BAD_REQUEST;
		
		if(end_timestamp <= 0 || start_timestamp<= 0 || end_timestamp < start_timestamp || end_timestamp - start_timestamp > 12 * 3600)
			return MHD_HTTP_BAD_REQUEST;
		
	} else {
		return MHD_HTTP_BAD_REQUEST;
	}
	
	loadev_buffer = (load_event_t*) malloc(sizeof(load_event_t) * (1 + end_timestamp - start_timestamp));
	
	if(loadev_buffer == NULL)
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	
	qty = get_load_events(start_timestamp, end_timestamp, loadev_buffer, (1 + end_timestamp - start_timestamp));
	
	if(qty < 0) {
		free(loadev_buffer);
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	response_array = json_object_new_array();
	
	for(int i = 0; i < qty; i++) {
		response_item = json_object_new_object();
		
		json_object_object_add_ex(response_item, "timestamp", json_object_new_int64(loadev_buffer[i].timestamp), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "state", json_object_new_int64(loadev_buffer[i].state), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "time_gap", json_object_new_int(loadev_buffer[i].time_gap), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "duration", json_object_new_int(loadev_buffer[i].duration), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "raw_delta_pt", json_object_new_double(loadev_buffer[i].raw_delta_pt), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "delta_pt", json_object_new_double(loadev_buffer[i].delta_pt), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "peak_pt", json_object_new_double(loadev_buffer[i].peak_pt), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "outlier_score",  json_object_new_double(loadev_buffer[i].outlier_score), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "pair_timestamp",  json_object_new_int64(loadev_buffer[i].pair_timestamp), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "pair_appliance_id",  json_object_new_int(loadev_buffer[i].pair_appliance_id), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(response_item, "pair_score",  json_object_new_int(loadev_buffer[i].pair_score), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		appliance_array = json_object_new_array();
		json_object_array_add(appliance_array, json_object_new_double(loadev_buffer[i].delta_p[0]));
		json_object_array_add(appliance_array, json_object_new_double(loadev_buffer[i].delta_p[1]));
		
		json_object_object_add_ex(response_item, "delta_p", appliance_array, JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		appliance_array = json_object_new_array();
		json_object_array_add(appliance_array, json_object_new_double(loadev_buffer[i].delta_s[0]));
		json_object_array_add(appliance_array, json_object_new_double(loadev_buffer[i].delta_s[1]));
		
		json_object_object_add_ex(response_item, "delta_s", appliance_array, JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		appliance_array = json_object_new_array();
		json_object_array_add(appliance_array, json_object_new_double(loadev_buffer[i].delta_q[0]));
		json_object_array_add(appliance_array, json_object_new_double(loadev_buffer[i].delta_q[1]));
		
		json_object_object_add_ex(response_item, "delta_q", appliance_array, JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		appliance_array = json_object_new_array();
		json_object_array_add(appliance_array, json_object_new_int(loadev_buffer[i].possible_appliances[0]));
		json_object_array_add(appliance_array, json_object_new_int(loadev_buffer[i].possible_appliances[1]));
		json_object_array_add(appliance_array, json_object_new_int(loadev_buffer[i].possible_appliances[2]));
		
		json_object_object_add_ex(response_item, "possible_appliances", appliance_array, JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		json_object_array_add(response_array, response_item);
	}
	
	free(loadev_buffer);
	
	*resp_data = strdup(json_object_get_string(response_array));
	
	json_object_put(response_array);
	
	if(*resp_data == NULL)
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	
	*resp_data_size = strlen(*resp_data);
	*resp_content_type = strdup(JSON_CONTENT_TYPE);
	
	return MHD_HTTP_OK;
}
