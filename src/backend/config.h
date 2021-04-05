#include <json-c/json.h>

struct json_object *config_get_list_json();
int config_get_value(const char *name, char *value_buf, size_t buf_size);
int config_get_value_int(const char *name, int min, int max, int default_value);
float config_get_value_float(const char *name, float min, float max, float default_value);
double config_get_value_double(const char *name, double min, double max, double default_value);
