int auth_verify_key(const char *key);
int auth_user_login(const char *username, const char *password, int *user_id_ptr);
char *auth_new_session(int user_id);
int auth_clean_sessions();
int auth_change_user_password(int user_id, const char *new_password);
