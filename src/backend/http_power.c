#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <json-c/json.h>

#include "common.h"
#include "logger.h"
#include "http.h"
#include "power.h"

enum power_get_type {
	POWER_GET_PT,
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
	
	const char *start_timestamp_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "start");
	const char *end_timestamp_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "end");
	const char *type_str = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "type");
	time_t start_timestamp, end_timestamp;
	enum power_get_type type;
	
	power_data_t *pd_buffer = NULL;
	int pd_qty;
	
	/*
	if(logged_user_id <= 0)
		return MHD_HTTP_UNAUTHORIZED;
	*/
	
	if(type_str == NULL || !strcmp(type_str, "pt"))
		type = POWER_GET_PT;
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
	
	if(start_timestamp_str == NULL || sscanf(start_timestamp_str, "%ld", &start_timestamp) != 1)
		return MHD_HTTP_BAD_REQUEST;
	
	if(end_timestamp_str == NULL || sscanf(end_timestamp_str, "%ld", &end_timestamp) != 1)
		end_timestamp = start_timestamp + 12 * 3600;
	
	if(end_timestamp < start_timestamp)
		return MHD_HTTP_BAD_REQUEST;
	
	if(end_timestamp > time(NULL))
		end_timestamp = time(NULL);
	
	if(end_timestamp - start_timestamp > 12 * 3600)
		return MHD_HTTP_BAD_REQUEST;
	
	pd_buffer = (power_data_t*) malloc(sizeof(power_data_t) * (1 + end_timestamp - start_timestamp));
	
	if(pd_buffer == NULL)
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	
	pd_qty = get_power_data(start_timestamp, end_timestamp, pd_buffer, (1 + end_timestamp - start_timestamp));
	
	if(pd_qty < 0) {
		free(pd_buffer);
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	// Gerar o JSON de resposta diretamente em texto neste caso é mais fácil e eficiente.
	*resp_data = (char*) malloc(sizeof(char) * (3 + pd_qty * 35));
	
	if(*resp_data == NULL) {
		free(pd_buffer);
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	*resp_data[0] = '[';
	*resp_data_size = 1;
	
	for(int i = 0; i < pd_qty; i++) {
		if(type == POWER_GET_PT)
			*resp_data_size += sprintf(&(*resp_data)[*resp_data_size], "[%ld,%.2lf],", pd_buffer[i].timestamp, pd_buffer[i].p[0] + pd_buffer[i].p[1]);
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
