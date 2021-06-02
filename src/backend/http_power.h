#include "http.h"

unsigned int http_handler_get_power_data(struct MHD_Connection *conn,
										int logged_user_id,
										path_parameter_t *path_parameters,
										char *req_data,
										size_t req_data_size,
										char **resp_content_type,
										char **resp_data,
										size_t *resp_data_size,
										void *arg);
