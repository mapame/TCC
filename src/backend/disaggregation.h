#ifndef DISAGGREGATION_H
#define DISAGGREGATION_H

#include <time.h>

typedef struct load_event_s {
	time_t timestamp;
	int time_gap;
	int duration;
	double delta_pt;
	double peak_pt;
	double delta_p[2];
	double delta_s[2];
	double delta_q[2];
	
	double p_sd;
	int appliance_ids[3];
	double appliance_probs[3];
	
} load_event_t;

int get_load_events(time_t timestamp_start, time_t timestamp_end, load_event_t *buffer, int buffer_len);

#endif
