#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <json-c/json.h>

#include "http.h"
#include "database.h"
#include "logger.h"
#include "users.h"

unsigned int http_handler_get_user_list(struct MHD_Connection *conn,
										int logged_user_id,
										path_parameter_t *path_parameters,
										char *req_data,
										size_t req_data_size,
										char **resp_content_type,
										char **resp_data,
										size_t *resp_data_size,
										void *arg) {
	
	user_t *user_list = NULL;
	int user_count;
	struct json_object* json_user_list = NULL;
	struct json_object* json_user = NULL;
	
	if(logged_user_id <= 0 || users_check_admin(logged_user_id) != 1)
		return MHD_HTTP_UNAUTHORIZED;
	
	if((user_count = users_get_list(&user_list, 0)) < 0)
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	
	json_user_list = json_object_new_array();
	
	for(int i = 0; i < user_count; i++) {
		json_user = json_object_new_object();
		
		json_object_object_add_ex(json_user, "id", json_object_new_int(user_list[i].id), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		if(user_list[i].name) {
			json_object_object_add_ex(json_user, "name", json_object_new_string(user_list[i].name), JSON_C_OBJECT_ADD_KEY_IS_NEW);
			
			free(user_list[i].name);
		} else {
			json_object_object_add_ex(json_user, "name", json_object_new_null(), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		}
		
		json_object_object_add_ex(json_user, "is_active", json_object_new_boolean(user_list[i].is_active), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(json_user, "is_admin", json_object_new_boolean(user_list[i].is_admin), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(json_user, "creation_date", json_object_new_int64(user_list[i].creation_date), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		json_object_object_add_ex(json_user, "modification_date", json_object_new_int64(user_list[i].modification_date), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		json_object_array_add(json_user_list, json_user);
	}
	
	free(user_list);
	
	*resp_data = strdup(json_object_get_string(json_user_list));
	
	json_object_put(json_user_list);
	
	if(*resp_data == NULL)
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	
	*resp_data_size = strlen(*resp_data);
	
	*resp_content_type = strdup(JSON_CONTENT_TYPE);
	
	return MHD_HTTP_OK;
}

unsigned int http_handler_get_user(struct MHD_Connection *conn,
										int logged_user_id,
										path_parameter_t *path_parameters,
										char *req_data,
										size_t req_data_size,
										char **resp_content_type,
										char **resp_data,
										size_t *resp_data_size,
										void *arg) {
	
	int user_id = logged_user_id;
	const char *user_id_str;
	int result;
	user_t user;
	struct json_object* json_user = NULL;
	
	if(logged_user_id <= 0)
		return MHD_HTTP_UNAUTHORIZED;
	
	if((user_id_str = http_parameter_get_value(path_parameters, 2)) == NULL)
		return MHD_HTTP_BAD_REQUEST;
	
	if(strcmp(user_id_str, "self")) {
		if(users_check_admin(logged_user_id) == 0)
			return MHD_HTTP_UNAUTHORIZED;
		
		if(sscanf(user_id_str, "%d", &user_id) != 1 || user_id <= 0)
			return MHD_HTTP_BAD_REQUEST;
	}
	
	result = users_get(user_id, &user);
	
	if(result < 0)
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	else if(result == 0)
		return MHD_HTTP_NOT_FOUND;
	
	json_user = json_object_new_object();
	
	json_object_object_add_ex(json_user, "id", json_object_new_int(user.id), JSON_C_OBJECT_ADD_KEY_IS_NEW);
	
	if(user.name) {
		json_object_object_add_ex(json_user, "name", json_object_new_string(user.name), JSON_C_OBJECT_ADD_KEY_IS_NEW);
		
		free(user.name);
	} else {
		json_object_object_add_ex(json_user, "name", json_object_new_null(), JSON_C_OBJECT_ADD_KEY_IS_NEW);
	}
	
	json_object_object_add_ex(json_user, "is_active", json_object_new_boolean(user.is_active), JSON_C_OBJECT_ADD_KEY_IS_NEW);
	json_object_object_add_ex(json_user, "is_admin", json_object_new_boolean(user.is_admin), JSON_C_OBJECT_ADD_KEY_IS_NEW);
	json_object_object_add_ex(json_user, "creation_date", json_object_new_int64(user.creation_date), JSON_C_OBJECT_ADD_KEY_IS_NEW);
	json_object_object_add_ex(json_user, "modification_date", json_object_new_int64(user.modification_date), JSON_C_OBJECT_ADD_KEY_IS_NEW);
	
	*resp_data = strdup(json_object_get_string(json_user));
	
	json_object_put(json_user);
	
	if(*resp_data == NULL)
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	
	*resp_data_size = strlen(*resp_data);
	
	*resp_content_type = strdup(JSON_CONTENT_TYPE);
	
	return MHD_HTTP_OK;
}

unsigned int http_handler_create_user(struct MHD_Connection *conn,
										int logged_user_id,
										path_parameter_t *path_parameters,
										char *req_data,
										size_t req_data_size,
										char **resp_content_type,
										char **resp_data,
										size_t *resp_data_size,
										void *arg) {
	
	struct json_object *received_json;
	struct json_object *json_user_name, *json_user_is_active, *json_user_is_admin, *json_user_password;
	char *new_user_password;
	int result;
	user_t new_user;
	
	if(req_data == NULL)
		return MHD_HTTP_BAD_REQUEST;
	
	received_json = json_tokener_parse(req_data);
	
	if(received_json == NULL)
		return MHD_HTTP_BAD_REQUEST;
	
	json_object_object_get_ex(received_json, "name", &json_user_name);
	json_object_object_get_ex(received_json, "password", &json_user_password);
	json_object_object_get_ex(received_json, "is_active", &json_user_is_active);
	json_object_object_get_ex(received_json, "is_admin", &json_user_is_admin);
		
	if(json_object_get_type(json_user_name) != json_type_string
		|| json_object_get_type(json_user_password) != json_type_string
		|| (json_user_is_active && json_object_get_type(json_user_is_active) != json_type_boolean)
		|| (json_user_is_admin && json_object_get_type(json_user_is_admin) != json_type_boolean)) {
		
		json_object_put(received_json);
		
		return MHD_HTTP_BAD_REQUEST;
	}
	
	new_user.name = strdup(json_object_get_string(json_user_name));
	new_user_password = strdup(json_object_get_string(json_user_password));
	
	new_user.is_active = 0;
	new_user.is_admin = 0;
	
	if(logged_user_id > 0) {
		new_user.is_active = json_object_get_boolean(json_user_is_active);
		
		if(users_check_admin(logged_user_id) == 1)
			new_user.is_admin = json_object_get_boolean(json_user_is_admin);
	}
	
	json_object_put(received_json);
	
	if(new_user.name == NULL || new_user_password == NULL) {
		free(new_user.name);
		free(new_user_password);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	if(strlen(new_user.name) < 3 || strlen(new_user.name) > 16 || strlen(new_user_password) < 4 || strlen(new_user_password) > 16) {
		free(new_user.name);
		free(new_user_password);
		
		return MHD_HTTP_BAD_REQUEST;
	}
	
	if((result = users_get_id_by_username(new_user.name)) < 0) {
		free(new_user.name);
		free(new_user_password);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	if((*resp_data = malloc(sizeof(char) * 128)) == NULL)
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	
	if(result > 0) { // Nome de usuário já existe
		*resp_data_size = sprintf(*resp_data, "{\"result\":\"user_name_taken\"}");
	} else {
		
		if((new_user.id = users_create(&new_user, new_user_password)) > 0) {
			*resp_data_size = sprintf(*resp_data, "{\"result\":\"success\",\"user_id\":%d}", new_user.id);
		} else {
			free(new_user.name);
			free(new_user_password);
			free(*resp_data);
			
			*resp_data = NULL;
			
			return MHD_HTTP_INTERNAL_SERVER_ERROR;
		}
	}
	
	free(new_user.name);
	free(new_user_password);
	
	*resp_content_type = strdup(JSON_CONTENT_TYPE);
	
	return MHD_HTTP_OK;
}

unsigned int http_handler_update_user(struct MHD_Connection *conn,
										int logged_user_id,
										path_parameter_t *path_parameters,
										char *req_data,
										size_t req_data_size,
										char **resp_content_type,
										char **resp_data,
										size_t *resp_data_size,
										void *arg) {
	int user_id = logged_user_id;
	const char *user_id_str;
	
	struct json_object *received_json;
	struct json_object *json_user_is_active, *json_user_is_admin, *json_user_password;
	int result;
	user_t user;
	
	if(logged_user_id <= 0)
		return MHD_HTTP_UNAUTHORIZED;
	
	if(req_data == NULL)
		return MHD_HTTP_BAD_REQUEST;
	
	if((user_id_str = http_parameter_get_value(path_parameters, 2)) == NULL)
		return MHD_HTTP_BAD_REQUEST;
	
	if(strcmp(user_id_str, "self")) {
		if(users_check_admin(logged_user_id) == 0)
			return MHD_HTTP_UNAUTHORIZED;
		
		if(sscanf(user_id_str, "%d", &user_id) != 1 || user_id <= 0)
			return MHD_HTTP_BAD_REQUEST;
	}
	
	result = users_get(user_id, &user);
	
	if(result < 0)
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	else if(result == 0)
		return MHD_HTTP_NOT_FOUND;
	
	received_json = json_tokener_parse(req_data);
	
	if(received_json == NULL)
		return MHD_HTTP_BAD_REQUEST;
	
	json_object_object_get_ex(received_json, "password", &json_user_password);
	json_object_object_get_ex(received_json, "is_active", &json_user_is_active);
	json_object_object_get_ex(received_json, "is_admin", &json_user_is_admin);
	
	if((json_user_password && json_object_get_type(json_user_password) != json_type_string)
		|| (json_user_is_active && json_object_get_type(json_user_is_active) != json_type_boolean)
		|| (json_user_is_admin && json_object_get_type(json_user_is_admin) != json_type_boolean)) {
		
		free(user.name);
		json_object_put(received_json);
		
		return MHD_HTTP_BAD_REQUEST;
	}
	
	if(json_user_password) {
		const char *user_new_password = json_object_get_string(json_user_password);
		
		if(strlen(user_new_password) < 4 || strlen(user_new_password) > 16) {
			free(user.name);
			json_object_put(received_json);
			
			return MHD_HTTP_BAD_REQUEST;
		}
		
		if(users_update_password(user_id, user_new_password) <= 0) {
			free(user.name);
			json_object_put(received_json);
			
			return MHD_HTTP_INTERNAL_SERVER_ERROR;
		}
	}
	
	if(users_check_admin(logged_user_id) == 1) {
		if(json_user_is_active)
			user.is_active = json_object_get_boolean(json_user_is_active);
		
		if(json_user_is_admin)
			user.is_admin = json_object_get_boolean(json_user_is_admin);
	}
	
	json_object_put(received_json);
	
	if((result = users_update(&user)) > 0) {
		*resp_data = strdup("{\"result\":\"success\"}");
		*resp_data_size = strlen(*resp_data);
	} else {
		free(user.name);
		
		return MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	free(user.name);
	
	*resp_content_type = strdup(JSON_CONTENT_TYPE);
	
	return MHD_HTTP_OK;
}
