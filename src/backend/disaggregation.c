#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <float.h>
#include <signal.h>
#include <pthread.h>

#include "common.h"
#include "logger.h"
#include "config.h"
#include "power.h"
#include "energy.h"
#include "database.h"
#include "disaggregation.h"

#define LOAD_EVENT_BUFFER_SIZE (24 * 3600)
#define DISAGGREGATION_BUFFER_SIZE 10
#define MAX_TIME_GAP 4
#define SVM_PARAM_QTY_ON 5
#define SVM_PARAM_QTY_OFF 4
#define SVM_CROSSV_FOLD_NUM 5


static pthread_mutex_t load_event_mutex = PTHREAD_MUTEX_INITIALIZER;

static load_event_t load_event_buffer[LOAD_EVENT_BUFFER_SIZE];
static int load_event_buffer_pos = 0;
static int load_event_buffer_count = 0;

void svm_print_string_f(const char *s) {
	LOG_DEBUG(s);
}

void *disaggregation_loop(void *argp) {
	int *terminate = (int*) argp;
	
	int result;
	double detection_threshold;
	time_t last_timestamp = 0;
	power_data_t pd_buffer[DISAGGREGATION_BUFFER_SIZE];
	double ptotal_buffer[DISAGGREGATION_BUFFER_SIZE];
	load_event_t load_event;
	int time_gap;
	double pavg_before, pavg_after;
	
	srand(time(NULL));
	svm_set_print_string_function(&svm_print_string_f);
	
	detection_threshold = config_get_value_double("load_event_detection_threshold", 10, 100, 50);
	
	LOG_INFO("Load event detection threshold: %.1lf W", detection_threshold);
	
	while(!(*terminate)) {
		if((result = get_power_data(last_timestamp, 0, pd_buffer, DISAGGREGATION_BUFFER_SIZE)) != DISAGGREGATION_BUFFER_SIZE) {
			sleep(1);
			continue;
		}
		
		load_event.time_gap = (last_timestamp > 0) ? (pd_buffer[0].timestamp - last_timestamp) : 0;
		
		last_timestamp = pd_buffer[1].timestamp;
		
		time_gap = 0;
		for(int i = 0; i < DISAGGREGATION_BUFFER_SIZE; i++) {
			if(i != 0)
				time_gap += (pd_buffer[i].timestamp - pd_buffer[i - 1].timestamp) - 1;
			
			ptotal_buffer[i] = pd_buffer[i].p[0] + pd_buffer[i].p[1];
		}
		
		if(time_gap > MAX_TIME_GAP)
			continue;
		
		if(fabs(ptotal_buffer[1] - ptotal_buffer[0]) < (fabs(ptotal_buffer[3] - ptotal_buffer[1]) * 0.5) && fabs(ptotal_buffer[2] - ptotal_buffer[1]) > (detection_threshold * 0.2) && fabs(ptotal_buffer[3] - ptotal_buffer[1]) > (detection_threshold * 0.2)) {
			pavg_before = (ptotal_buffer[0] + ptotal_buffer[1]) / 2.0;
			
			for(int k = 3; k < DISAGGREGATION_BUFFER_SIZE - 2; k++) {
				pavg_after = (ptotal_buffer[k] + ptotal_buffer[k + 1]) / 2.0;
				
				if(fabs(pavg_after - pavg_before) > detection_threshold && fabs(ptotal_buffer[k + 1] - ptotal_buffer[k]) < (fabs(ptotal_buffer[3] - ptotal_buffer[1]) * 0.5) && ((pavg_after - pavg_before) * (ptotal_buffer[3] - ptotal_buffer[1]) > 0.0)) {
					load_event.timestamp = pd_buffer[1].timestamp;
					load_event.duration = k - 1;
					load_event.delta_pt = (pavg_after - pavg_before);
					load_event.delta_p[0] = ((pd_buffer[k].p[0] + pd_buffer[k + 1].p[0]) / 2.0) - ((pd_buffer[0].p[0] + pd_buffer[1].p[0]) / 2.0);
					load_event.delta_p[1] = ((pd_buffer[k].p[1] + pd_buffer[k + 1].p[1]) / 2.0) - ((pd_buffer[0].p[1] + pd_buffer[1].p[1]) / 2.0);
					load_event.delta_s[0] = ((pd_buffer[k].s[0] + pd_buffer[k + 1].s[0]) / 2.0) - ((pd_buffer[0].s[0] + pd_buffer[1].s[0]) / 2.0);
					load_event.delta_s[1] = ((pd_buffer[k].s[1] + pd_buffer[k + 1].s[1]) / 2.0) - ((pd_buffer[0].s[1] + pd_buffer[1].s[1]) / 2.0);
					load_event.delta_q[0] = ((pd_buffer[k].q[0] + pd_buffer[k + 1].q[0]) / 2.0) - ((pd_buffer[0].q[0] + pd_buffer[1].q[0]) / 2.0);
					load_event.delta_q[1] = ((pd_buffer[k].q[1] + pd_buffer[k + 1].q[1]) / 2.0) - ((pd_buffer[0].q[1] + pd_buffer[1].q[1]) / 2.0);
					
					if(load_event.delta_pt > 0.0) {
						load_event.peak_pt = load_event.delta_pt;
						
						for(int z = 2; z <= k; z++)
							if((ptotal_buffer[z] - pavg_before) > load_event.peak_pt)
								load_event.peak_pt = (ptotal_buffer[z] - pavg_before);
					}
					
					load_event.top_appliance_id = -1;
					pthread_mutex_lock(&load_event_mutex);
					
					memcpy(&load_event_buffer[load_event_buffer_pos], &load_event, sizeof(load_event_t));
					
					load_event_buffer_pos = (load_event_buffer_pos + 1) % LOAD_EVENT_BUFFER_SIZE;
					if(load_event_buffer_count < LOAD_EVENT_BUFFER_SIZE)
						load_event_buffer_count++;
					
					pthread_mutex_unlock(&load_event_mutex);
					
					last_timestamp = pd_buffer[k].timestamp;
					
					break;
				}
			}
		}
	}
	
	return NULL;
}

int get_load_events(time_t timestamp_start, time_t timestamp_end, load_event_t *buffer, int buffer_len) {
	int pos;
	int output_count = 0;
	
	if(buffer == NULL)
		return -1;
	
	if(buffer_len == 0 || timestamp_end < timestamp_start)
		return 0;
	
	if(pthread_mutex_lock(&load_event_mutex))
		return -2;
	
	pos = (load_event_buffer_count < LOAD_EVENT_BUFFER_SIZE) ? 0 : load_event_buffer_pos;
	
	for(int count = 0; (count < load_event_buffer_count && output_count < buffer_len); count++) {
		
		if(timestamp_end > 0 && load_event_buffer[pos].timestamp > timestamp_end)
			break;
		
		if(load_event_buffer[pos].timestamp >= timestamp_start) {
			memcpy(&buffer[output_count], &load_event_buffer[pos], sizeof(load_event_t));
			
			output_count++;
		}
		
		pos = (pos + 1) % LOAD_EVENT_BUFFER_SIZE;
	}
	
	pthread_mutex_unlock(&load_event_mutex);
	
	return output_count;
}
