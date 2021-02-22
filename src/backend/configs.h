struct json_object *configs_get_json();
int configs_get_value(const char *name, char *value_buf, size_t buf_size);
int configs_get_value_int(const char *name, int min, int max, int default_value);
float configs_get_value_float(const char *name, float min, float max, float default_value);
