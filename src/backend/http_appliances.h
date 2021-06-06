#include "http.h"

unsigned int http_handler_get_appliance_list(struct MHD_Connection *conn,
											int logged_user_id,
											path_parameter_t *path_parameters,
											char *req_data,
											size_t req_data_size,
											char **resp_content_type,
											char **resp_data,
											size_t *resp_data_size,
											void *arg);

unsigned int http_handler_get_appliance(struct MHD_Connection *conn,
											int logged_user_id,
											path_parameter_t *path_parameters,
											char *req_data,
											size_t req_data_size,
											char **resp_content_type,
											char **resp_data,
											size_t *resp_data_size,
											void *arg);

unsigned int http_handler_create_appliance(struct MHD_Connection *conn,
											int logged_user_id,
											path_parameter_t *path_parameters,
											char *req_data,
											size_t req_data_size,
											char **resp_content_type,
											char **resp_data,
											size_t *resp_data_size,
											void *arg);

unsigned int http_handler_update_appliance(struct MHD_Connection *conn,
											int logged_user_id,
											path_parameter_t *path_parameters,
											char *req_data,
											size_t req_data_size,
											char **resp_content_type,
											char **resp_data,
											size_t *resp_data_size,
											void *arg);

unsigned int http_handler_get_appliance_signature_list(struct MHD_Connection *conn,
														int logged_user_id,
														path_parameter_t *path_parameters,
														char *req_data,
														size_t req_data_size,
														char **resp_content_type,
														char **resp_data,
														size_t *resp_data_size,
														void *arg);

unsigned int http_handler_get_appliance_signature(struct MHD_Connection *conn,
													int logged_user_id,
													path_parameter_t *path_parameters,
													char *req_data,
													size_t req_data_size,
													char **resp_content_type,
													char **resp_data,
													size_t *resp_data_size,
													void *arg);

unsigned int http_handler_add_appliance_signatures(struct MHD_Connection *conn,
													int logged_user_id,
													path_parameter_t *path_parameters,
													char *req_data,
													size_t req_data_size,
													char **resp_content_type,
													char **resp_data,
													size_t *resp_data_size,
													void *arg);

unsigned int http_handler_delete_appliance_signature(struct MHD_Connection *conn,
													int logged_user_id,
													path_parameter_t *path_parameters,
													char *req_data,
													size_t req_data_size,
													char **resp_content_type,
													char **resp_data,
													size_t *resp_data_size,
													void *arg);
