#include "http.h"

unsigned int http_handler_get_energy_overview(struct MHD_Connection *conn,
												int logged_user_id,
												path_parameter_t *path_parameters,
												char *req_data,
												size_t req_data_size,
												char **resp_content_type,
												char **resp_data,
												size_t *resp_data_size,
												void *arg);
									
unsigned int http_handler_get_energy_minutes(struct MHD_Connection *conn,
												int logged_user_id,
												path_parameter_t *path_parameters,
												char *req_data,
												size_t req_data_size,
												char **resp_content_type,
												char **resp_data,
												size_t *resp_data_size,
												void *arg);

unsigned int http_handler_get_energy_hours(struct MHD_Connection *conn,
											int logged_user_id,
											path_parameter_t *path_parameters,
											char *req_data,
											size_t req_data_size,
											char **resp_content_type,
											char **resp_data,
											size_t *resp_data_size,
											void *arg);

unsigned int http_handler_get_energy_days(struct MHD_Connection *conn,
											int logged_user_id,
											path_parameter_t *path_parameters,
											char *req_data,
											size_t req_data_size,
											char **resp_content_type,
											char **resp_data,
											size_t *resp_data_size,
											void *arg);

unsigned int http_handler_get_energy_months(struct MHD_Connection *conn,
											int logged_user_id,
											path_parameter_t *path_parameters,
											char *req_data,
											size_t req_data_size,
											char **resp_content_type,
											char **resp_data,
											size_t *resp_data_size,
											void *arg);

unsigned int http_handler_get_disaggregated_energy_minutes(struct MHD_Connection *conn,
															int logged_user_id,
															path_parameter_t *path_parameters,
															char *req_data,
															size_t req_data_size,
															char **resp_content_type,
															char **resp_data,
															size_t *resp_data_size,
															void *arg);

unsigned int http_handler_get_disaggregated_energy_hours(struct MHD_Connection *conn,
															int logged_user_id,
															path_parameter_t *path_parameters,
															char *req_data,
															size_t req_data_size,
															char **resp_content_type,
															char **resp_data,
															size_t *resp_data_size,
															void *arg);
