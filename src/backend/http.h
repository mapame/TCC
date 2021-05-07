#ifndef HTTP_H
#define HTTP_H

#include <microhttpd.h>

#define DEFAULT_HTTP_PORT 8081
#define JSON_CONTENT_TYPE "application/json"

typedef struct path_parameter_s {
	int pos;
	char *value;
	
	struct path_parameter_s *next;
} path_parameter_t;

struct MHD_Daemon* http_init(uint16_t port);
void http_stop(struct MHD_Daemon *httpd);

#endif
