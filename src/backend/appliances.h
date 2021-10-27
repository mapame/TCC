#ifndef APPLIANCES_H
#define APPLIANCES_H

#include <time.h>

typedef struct load_signature_s {
	int appliance_id;
	double delta_pt;
	double peak_pt;
	double duration;
	double delta_p[2];
	double delta_q[2];
	double delta_s[2];
} load_signature_t;

int fetch_signatures(load_signature_t **sig_buffer_ptr, double min_power);
time_t get_last_signature_modification();
int get_appliances_max_time_on(int **result_output);
double get_closest_signature_power(int appliance_id, double power);

#endif
