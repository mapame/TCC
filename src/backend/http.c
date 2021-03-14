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

typedef struct http_url_handler {
	const char *url;
	void *arg;
	http_url_handler_func_t get_handler;
	http_url_handler_func_t post_handler;
	http_url_handler_func_t put_handler;
	http_url_handler_func_t delete_handler;
} http_url_handler_t;

static const http_url_handler_t http_url_handler_list[] = {
	{"", NULL, NULL, NULL, NULL, NULL},
	{"/auth", NULL, url_handler_auth_check, url_handler_auth_check, NULL, NULL},
	{"/auth/login", NULL, NULL, url_handler_auth_login_post, NULL},
};

#if (MHD_VERSION < 0x00097002)
static int
#else
static enum MHD_Result
#endif
http_handler(void *cls, struct MHD_Connection *connection,
						const char *url,
						const char *method,
						const char *version,
						const char *upload_data, size_t *upload_data_size,
						void **con_cls) {
	
	http_req_ctx_t *req_context;
	const http_url_handler_t *handler = NULL;
	struct MHD_Response *response;
	unsigned int status = MHD_HTTP_OK;
	char *resp_content_type = NULL;
	char *resp_data = NULL;
	size_t resp_data_size = 0;
	int result;
	
	/* Na primeira chamada do callback con_cls é NULL e apenas os headers HTTP estão disponíveis */
	if(*con_cls == NULL) {
		*con_cls = calloc(1, sizeof(http_req_ctx_t));
		if (*con_cls == NULL) {
			LOG_ERROR("Failed to allocate memory for HTTP request context.");
			return MHD_NO;
		}
		
		((http_req_ctx_t*) *con_cls)->data = NULL;
		
		return MHD_YES;
	}
	
	req_context = (http_req_ctx_t*) *con_cls;
	
	/* Os dados no corpo da requisição são passados em partes, em diferentes chamadas do callback, no final 
	 * é feita mais uma chamada, com upload_data_size igual a 0, para indicar o termino dos dados. */
	if (*upload_data_size != 0) {
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
	
	LOG_DEBUG("Received %s request for url: %s", method, url);
	
	if(status == MHD_HTTP_OK) {
		http_url_handler_func_t handler_f = NULL;
		
		for(int i = 0; i < (sizeof(http_url_handler_list)/sizeof(http_url_handler_t)); i++)
			if(strcmp(url, http_url_handler_list[i].url) == 0) {
				handler = &http_url_handler_list[i];
				break;
			}
		
		if(handler == NULL)
			status = MHD_HTTP_NOT_FOUND;
		else if(strcmp(method, MHD_HTTP_METHOD_GET) == 0 && handler->get_handler)
			handler_f = handler->get_handler;
		else if(strcmp(method, MHD_HTTP_METHOD_POST) == 0 && handler->post_handler)
			handler_f = handler->post_handler;
		else if(strcmp(method, MHD_HTTP_METHOD_PUT) == 0 && handler->put_handler)
			handler_f = handler->put_handler;
		else if(strcmp(method, MHD_HTTP_METHOD_DELETE) == 0 && handler->delete_handler)
			handler_f = handler->delete_handler;
		else
			status = MHD_HTTP_METHOD_NOT_ALLOWED;
		
		if(handler_f)
			status = (handler_f)(connection, url, req_context->data, req_context->data_size, &resp_content_type, &resp_data, &resp_data_size, handler->arg);
	}
	
	free(req_context->data);
	free(req_context);
	
	response = MHD_create_response_from_buffer(resp_data_size, resp_data, resp_data ? MHD_RESPMEM_MUST_FREE : MHD_RESPMEM_PERSISTENT);
	
	/* Se um método não suportado foi recebido, envia o cabeçalho "Allow" com os métodos permitidos pela URL */
	if (status == MHD_HTTP_METHOD_NOT_ALLOWED) {
		char allow_str[32] = "\0";
		
		if (handler->get_handler)
			strcat(allow_str, "GET, ");
		if (handler->put_handler)
			strcat(allow_str, "PUT, ");
		if (handler->post_handler)
			strcat(allow_str, "POST, ");
		if (handler->delete_handler)
			strcat(allow_str, "DELETE, ");
		
		if (allow_str[0] == '\0') {
			status = MHD_HTTP_NOT_FOUND;
		} else {
			/* Remove virgula e espaço */
			allow_str[strlen(allow_str) - 2] = '\0';

			MHD_add_response_header(response, MHD_HTTP_HEADER_ALLOW, allow_str);
		}
	}
	
	/* Se dados forem retornados, adiciona o cabeçalho "Content-Type" com o tipo MIME da resposta */
	if(resp_data && resp_data_size) {
		if (resp_content_type) {
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
 		&http_handler, NULL, /* request handler */
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
