#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <microhttpd.h>

#include "http.h"
#include "logger.h"

#include "http_auth.h"
#include "http_config.h"

#define CONNECTION_LIMIT 200
#define CONNECTION_TIMEOUT 5

typedef struct http_req_ctx {
	char *data;
	size_t data_size;
} http_req_ctx_t;

typedef unsigned int
(*http_handler_func_t)(struct MHD_Connection *conn,
							path_parameter_t *path_parameters,
							char *req_data,
							size_t req_data_size,
							char **resp_content_type,
							char **resp_data,
							size_t *resp_data_size,
							void *arg);

typedef struct path_segment_s {
	const char *text;
	void *arg;
	
	http_handler_func_t get_handler;
	http_handler_func_t put_handler;
	http_handler_func_t post_handler;
	http_handler_func_t delete_handler;
	
	const struct path_segment_s *children;
} path_segment_t;


static const path_segment_t url_path_tree = {
	.children = (const path_segment_t[]) {
		{
			.text = "auth",
			.children = (const path_segment_t[]) {
				{
					.text = "verify",
					.get_handler = url_handler_auth_verify,
				},
				{
					.text = "login",
					.post_handler = url_handler_auth_login,
				},
				{}
			}
		},{
			.text = "config",
			.get_handler = url_handler_config_list
		},{
		},
	}
};


void free_parameters(path_parameter_t *parameters) {
	path_parameter_t *next;
	
	while(parameters) {
		free(parameters->value);
		
		next = parameters->next;
		
		free(parameters);
		
		parameters = next;
	}
}

static const path_segment_t *resolve_url_path(const char *url_path, path_parameter_t **parameters) {
	const path_segment_t *child_ptr = &url_path_tree;
	char *url_path_copy = strdup(url_path);
	char *tmp_ptr;
	char *saveptr;
	char *seg_text;
	
	path_parameter_t **param_ptr = parameters;
	int pos = 0;
	
	*param_ptr = NULL;
	
	tmp_ptr = url_path_copy;
	while(child_ptr && (seg_text = strtok_r(tmp_ptr, "/", &saveptr))) {
		tmp_ptr = NULL;
		child_ptr = child_ptr->children;
		pos++;
		
		while(child_ptr && child_ptr->text) {
			if(strcmp(child_ptr->text, "*") == 0) {
				if(param_ptr) {
					*param_ptr = (path_parameter_t*) malloc(sizeof(path_parameter_t));
					
					if(*param_ptr) {
						(*param_ptr)->pos = pos;
						(*param_ptr)->value = strdup(seg_text);
						param_ptr = &((*param_ptr)->next);
					}
				}
				
				break;
			} else if(strcmp(child_ptr->text, seg_text) == 0) {
				break;
			}
			
			child_ptr++;
		}
	}
	
	if(param_ptr && *param_ptr)
		(*param_ptr)->next = NULL;
	
	free(url_path_copy);
	
	return child_ptr;
}

#if (MHD_VERSION < 0x00097002)
static int
#else
static enum MHD_Result
#endif
http_global_handler(void *cls, struct MHD_Connection *connection,
						const char *url_path,
						const char *method,
						const char *version,
						const char *upload_data, size_t *upload_data_size,
						void **con_cls) {
	
	http_req_ctx_t *req_context;
	const path_segment_t *path_seg = NULL;
	path_parameter_t *path_parameters = NULL;
	struct MHD_Response *response;
	unsigned int status = MHD_HTTP_OK;
	int options_request = 0;
	char *resp_content_type = NULL;
	char *resp_data = NULL;
	size_t resp_data_size = 0;
	int result;
	
	/* Na primeira chamada do callback con_cls é NULL e apenas os headers HTTP estão disponíveis */
	if(*con_cls == NULL) {
		*con_cls = calloc(1, sizeof(http_req_ctx_t));
		if(*con_cls == NULL) {
			LOG_ERROR("Failed to allocate memory for HTTP request context.");
			return MHD_NO;
		}
		
		((http_req_ctx_t*) *con_cls)->data = NULL;
		
		return MHD_YES;
	}
	
	req_context = (http_req_ctx_t*) *con_cls;
	
	/* Os dados no corpo da requisição são passados em partes, em diferentes chamadas do callback, no final 
	 * é feita mais uma chamada, com upload_data_size igual a 0, para indicar o termino dos dados. */
	if(*upload_data_size != 0) {
		char *new_buffer;
		
		new_buffer = (char*) realloc(req_context->data, req_context->data_size + *upload_data_size);
		if(new_buffer) {
			req_context->data = new_buffer;
			memcpy(req_context->data + req_context->data_size, upload_data, *upload_data_size);
			req_context->data_size += *upload_data_size;
			*upload_data_size = 0;
			
			return MHD_YES;
		}
		
		LOG_ERROR("Failed to allocate memory for request data buffer.");
		status = MHD_HTTP_INTERNAL_SERVER_ERROR;
	}
	
	LOG_DEBUG("Received %s request for url: %s", method, url_path);
	
	if(status == MHD_HTTP_OK) {
		http_handler_func_t handler_f = NULL;
		
		path_seg = resolve_url_path(url_path, &path_parameters);
		
		if(path_seg == NULL)
			status = MHD_HTTP_NOT_FOUND;
		else if(strcmp(method, MHD_HTTP_METHOD_GET) == 0 && path_seg->get_handler)
			handler_f = path_seg->get_handler;
		else if(strcmp(method, MHD_HTTP_METHOD_POST) == 0 && path_seg->post_handler)
			handler_f = path_seg->post_handler;
		else if(strcmp(method, MHD_HTTP_METHOD_PUT) == 0 && path_seg->put_handler)
			handler_f = path_seg->put_handler;
		else if(strcmp(method, MHD_HTTP_METHOD_DELETE) == 0 && path_seg->delete_handler)
			handler_f = path_seg->delete_handler;
		else if(strcmp(method, MHD_HTTP_METHOD_OPTIONS) == 0)
			options_request = 1;
		else
			status = MHD_HTTP_METHOD_NOT_ALLOWED;
		
		if(handler_f)
			status = (handler_f)(connection, path_parameters, req_context->data, req_context->data_size, &resp_content_type, &resp_data, &resp_data_size, path_seg->arg);
		
		if(path_parameters)
			free_parameters(path_parameters);
	}
	
	free(req_context->data);
	free(req_context);
	
	response = MHD_create_response_from_buffer(resp_data_size, resp_data, resp_data ? MHD_RESPMEM_MUST_FREE : MHD_RESPMEM_PERSISTENT);
	
	/* Se um método não suportado foi recebido, envia o cabeçalho "Allow" com os métodos permitidos pela URL */
	if (status == MHD_HTTP_METHOD_NOT_ALLOWED || options_request) {
		char allow_str[32] = "\0";
		
		if(path_seg->get_handler)
			strcat(allow_str, "GET, ");
		if(path_seg->put_handler)
			strcat(allow_str, "PUT, ");
		if(path_seg->post_handler)
			strcat(allow_str, "POST, ");
		if(path_seg->delete_handler)
			strcat(allow_str, "DELETE, ");
		
		if(allow_str[0] == '\0') {
			status = MHD_HTTP_NOT_FOUND;
		} else {
			/* Remove virgula e espaço */
			allow_str[strlen(allow_str) - 2] = '\0';

			MHD_add_response_header(response, MHD_HTTP_HEADER_ALLOW, allow_str);
		}
	}
	
	/* Se dados forem retornados, adiciona o cabeçalho "Content-Type" com o tipo MIME da resposta */
	if(resp_data && resp_data_size) {
		if(resp_content_type) {
			MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, resp_content_type);
			free(resp_content_type);
		} else {
			MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/plain");
		}
	}
	
	/* Adiciona cabeçalhos para permitir multiplas requisições em uma mesma conexão TCP */
	MHD_add_response_header(response, MHD_HTTP_HEADER_CONNECTION, "keep-alive");
	MHD_add_response_header(response, MHD_HTTP_HEADER_KEEP_ALIVE, "timeout=5");
	
	/* Adiciona cabeçalho para permitir acesso por outras origens */
	MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
	MHD_add_response_header(response, "Access-Control-Allow-Methods", "*");
	MHD_add_response_header(response, "Access-Control-Allow-Headers", "*");
	
	result = MHD_queue_response(connection, status, response);
	MHD_destroy_response(response);
	
	return result;
}

struct MHD_Daemon* http_init(uint16_t port) {
	struct MHD_Daemon *httpd;
	struct MHD_OptionItem opta[] = {
		{ MHD_OPTION_CONNECTION_LIMIT,		CONNECTION_LIMIT,	NULL },
		{ MHD_OPTION_CONNECTION_TIMEOUT,	CONNECTION_TIMEOUT,	NULL },
		{ MHD_OPTION_END, 0, NULL }
	};
	
	httpd = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_THREAD_PER_CONNECTION | MHD_USE_ERROR_LOG, port,
		NULL, NULL, /* access control */
 		&http_global_handler, NULL, /* request handler */
		MHD_OPTION_ARRAY, opta,
		MHD_OPTION_END);
	
	if(httpd == NULL) {
		LOG_ERROR("Failed to start HTTP daemon.");
		return NULL;
	}
	
	LOG_INFO("HTTP server started on port %hu.", port);
	
	return httpd;
}

void http_stop(struct MHD_Daemon *httpd) {
	MHD_stop_daemon(httpd);
	
	LOG_INFO("HTTP server stopped.");
}
