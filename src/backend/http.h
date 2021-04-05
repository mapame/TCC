#ifndef HTTP_H
#define HTTP_H


#include <microhttpd.h>

#define DEFAULT_HTTP_PORT 8081
#define JSON_CONTENT_TYPE "application/json"

typedef unsigned int
(*http_url_handler_func_t)(struct MHD_Connection *conn,
							const char *url,
							char *req_data,
							size_t req_data_size,
							char **resp_content_type,
							char **resp_data,
							size_t *resp_data_size,
							void *arg);


struct MHD_Daemon* http_init(uint16_t port);
void http_stop(struct MHD_Daemon *httpd);

#endif
