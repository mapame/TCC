char *auth_hash_password(const char *salt, const char *password);
char *auth_new_salt();
int auth_verify_key(const char *key);
int auth_user_login(const char *username, const char *password);
char *auth_new_session(int user_id);
int auth_delete_session(const char *session_key);
