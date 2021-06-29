#ifndef CONFIG_H
#define CONFIG_H

typedef struct config_s {
	char *key;
	char *value;
	time_t modification_date;
} config_t;

struct json_object *config_get_list_json();
void config_free(config_t *config_ptr);
int config_get_value(const char *key, char *value_buf, size_t buf_size);
int config_get_value_int(const char *key, int min, int max, int default_value);
float config_get_value_float(const char *key, float min, float max, float default_value);
double config_get_value_double(const char *key, double min, double max, double default_value);

#endif
