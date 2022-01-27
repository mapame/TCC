#ifndef ENERGY_H
#define ENERGY_H

#include <time.h>

#include "power.h"

int energy_add_power(power_data_t *pd);
int energy_add_power_disaggregated(int appliance_id, time_t start_timestamp, time_t end_timestamp, double start_dp, double end_dp);

#endif
