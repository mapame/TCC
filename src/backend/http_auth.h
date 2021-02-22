int http_check_authorization(struct MHD_Connection *conn);

unsigned int url_handler_auth_login_post(struct MHD_Connection *conn,
									const char *url,
									char *req_data,
									size_t req_data_size,
									char **resp_content_type,
									char **resp_data,
									size_t *resp_data_size,
									void *arg);

unsigned int url_handler_auth_check(struct MHD_Connection *conn,
									const char *url,
									char *req_data,
									size_t req_data_size,
									char **resp_content_type,
									char **resp_data,
									size_t *resp_data_size,
									void *arg);
