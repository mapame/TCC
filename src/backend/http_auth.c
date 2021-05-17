#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <json-c/json.h>

#include "http.h"
#include "logger.h"
#include "users.h"
#include "auth.h"

unsigned int http_handler_auth_login(struct MHD_Connection *conn,
									int logged_user_id,
									path_parameter_t *path_parameters,
									char *req_data,
									size_t req_data_size,
									char **resp_content_type,
									char **resp_data,
									size_t *resp_data_size,
									void *arg) {
	
	json_object *top_json = NULL;
	json_object *username_json = NULL, *password_json = NULL;
	char *username = NULL, *password = NULL;
	int user_id = 0;
	char *key;
	
	if(req_data == NULL)
		return MHD_HTTP_BAD_REQUEST;
	
	top_json = json_tokener_parse(req_data);
	
	if(json_object_object_get_ex(top_json, "username", &username_json) == 0 || json_object_object_get_ex(top_json, "password", &password_json) == 0) {
		json_object_put(top_json);
		return MHD_HTTP_BAD_REQUEST;
	}
	
	if(json_object_get_type(username_json) != json_type_string || json_object_get_type(password_json) != json_type_string) {
		json_object_put(top_json);
		return MHD_HTTP_BAD_REQUEST;
	}
	
	username = strdup(json_object_get_string(username_json));
	password = strdup(json_object_get_string(password_json));
	
	json_object_put(top_json);
	
	user_id = auth_user_login(username, password);
	
	free(username);
	free(password);
	
	if(user_id < 0)
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	
	*resp_data = (char*) malloc(sizeof(char) * 256);
	
	if(user_id == 0) {
		*resp_data_size = sprintf(*resp_data, "{\"result\":\"wrong\"}");
		
	} else if(users_check_active(user_id) == 0) {
		*resp_data_size = sprintf(*resp_data, "{\"result\":\"inactive\"}");
		
	} else {
		if((key = auth_new_session(user_id)) == NULL) {
			free(*resp_data);
			
			return MHD_HTTP_INTERNAL_SERVER_ERROR;
		}
		
		*resp_data_size = sprintf(*resp_data, "{\"result\":\"success\",\"access_key\":\"%s\"}", key);
		
		free(key);
	}
	
	*resp_content_type = strdup(JSON_CONTENT_TYPE);
	
	return MHD_HTTP_OK;
}

unsigned int http_handler_auth_verify(struct MHD_Connection *conn,
									int logged_user_id,
									path_parameter_t *path_parameters,
									char *req_data,
									size_t req_data_size,
									char **resp_content_type,
									char **resp_data,
									size_t *resp_data_size,
									void *arg) {
	
	*resp_data = (char*) malloc(sizeof(char) * 30);
	*resp_data_size = sprintf(*resp_data, "{\"result\":\"%s\"}", (logged_user_id > 0) ? "valid" : "invalid");
	
	*resp_content_type = strdup(JSON_CONTENT_TYPE);
	
	return MHD_HTTP_OK;
}

unsigned int http_handler_auth_logout(struct MHD_Connection *conn,
										int logged_user_id,
										path_parameter_t *path_parameters,
										char *req_data,
										size_t req_data_size,
										char **resp_content_type,
										char **resp_data,
										size_t *resp_data_size,
										void *arg) {
	
	const char *authorization_header_value = NULL;
	
	if(logged_user_id <= 0)
		return MHD_HTTP_UNAUTHORIZED;
	
	authorization_header_value = MHD_lookup_connection_value(conn, MHD_HEADER_KIND, "Authorization");
	
	if(authorization_header_value)
		auth_delete_session(&authorization_header_value[7]);
	
	return MHD_HTTP_OK;
}
