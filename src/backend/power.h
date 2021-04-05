#ifndef POWER_DATA_H
#define POWER_DATA_H

#include <time.h>

typedef struct power_data_s {
	time_t timestamp;
	double v[2];
	double i[2];
	double p[2];
	double s[2];
	double q[2];
} power_data_t;

int load_saved_power_data();
void close_power_data_file();
int store_power_data(power_data_t *pd_ptr);
int get_power_data(time_t timestamp_start, time_t timestamp_end, power_data_t *buffer, int buffer_len);

#endif
