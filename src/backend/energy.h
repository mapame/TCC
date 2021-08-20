#ifndef ENERGY_H
#define ENERGY_H

#include <time.h>

#include "power.h"

typedef struct energy_rate_s {
	time_t start_timestamp;
	double rate;
	int is_tou;
	double peak_rate;
	double mid_rate;
	int peak_start;
	int peak_end;
	int mid_start;
	int mid_end;
	time_t creation_date;
	time_t modification_date;
} energy_rate_t;

int energy_add_power(power_data_t *pd);
int energy_add_power_disaggregated(int appliance_id, time_t start_timestamp, time_t end_timestamp, double start_dp, double end_dp);

#endif
