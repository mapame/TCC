#include <time.h>

typedef struct user_s {
	int id;
	char *name;
	int is_active;
	int is_admin;
	time_t creation_date;
	time_t modification_date;
} user_t;

int users_get_id_by_username(const char *username);
int users_check_active(int user_id);
int users_check_admin(int user_id);
int users_get_list(user_t **user_list_ptr, int filter_inactive);
int users_get(int user_id, user_t *user_ptr);
int users_create(const user_t *user, const char *password);
int users_update(const user_t *user);
int users_update_password(int user_id, const char *password);
