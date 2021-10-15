#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <math.h>
#include <float.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>

#include "common.h"
#include "logger.h"
#include "config.h"
#include "power.h"
#include "energy.h"
#include "database.h"
#include "appliances.h"
#include "classification.h"
#include "disaggregation.h"

#define LOAD_EVENT_BUFFER_SIZE 10000
#define DISAGGREGATION_BUFFER_SIZE 10
#define POWER_NORMALIZATION_EXPOENT 2
#define MAX_TIME_GAP 4
#define TRAINING_MIN_SIGNATURE_QTY 10

static pthread_mutex_t load_event_mutex = PTHREAD_MUTEX_INITIALIZER;

static load_event_t load_event_buffer[LOAD_EVENT_BUFFER_SIZE];
static int load_event_buffer_pos = 0;
static int load_event_buffer_count = 0;

static FILE *open_le_fd = NULL;
static char open_le_filename[32];

static size_t generate_le_filename(time_t time_epoch, char *buffer, size_t len);
static int add_load_event(const load_event_t *load_event, int save_to_file);
static int update_load_event(const load_event_t *load_event);


static size_t generate_le_filename(time_t time_epoch, char *buffer, size_t len) {
	struct tm time_tm;
	
	gmtime_r(&time_epoch, &time_tm);
	return strftime(buffer, len, "le-%F.csv", &time_tm);
}

static int open_load_events_file(time_t timestamp) {
	char filename[32];
	
	generate_le_filename(timestamp, filename, sizeof(filename));
	
	if(open_le_fd == NULL || strcmp(open_le_filename, filename)) {
		if(open_le_fd)
			fclose(open_le_fd);
		
		LOG_INFO("Opening load event file \"%s\".", filename);
		
		open_le_fd = fopen(filename, "a");
		if(open_le_fd == NULL) {
			LOG_FATAL("Failed to open file \"%s\": %s", filename, strerror(errno));
			
			return -1;
		}
		
		strcpy(open_le_filename, filename);
	}
	
	return 0;
}

static void close_load_events_file() {
	if(open_le_fd == NULL)
		return;
	
	fclose(open_le_fd);
	
	open_le_fd = NULL;
}

static int import_load_events_from_file(const char *filename, time_t timestamp_limit) {
	FILE *le_file = NULL;
	char line_buffer[128];
	load_event_t load_event;
	int line_counter = 0;
	int type_counter[3] = {0};
	int error_counter = 0;
	
	if((le_file = fopen(filename, "r")) == NULL) {
		LOG_ERROR("Failed to open load events file \"%s\": %s", filename, strerror(errno));
		return -1;
	}
	
	while(fgets(line_buffer, sizeof(line_buffer), le_file)) {
		memset(&load_event, 0, sizeof(load_event_t));
		
		line_counter++;
		
		if(strncmp(line_buffer, "DETECTED,", 9) == 0) {
			if(sscanf(line_buffer, "DETECTED,%li,%i,%i,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf", &load_event.timestamp, &load_event.time_gap, &load_event.duration, &load_event.raw_delta_pt, &load_event.delta_pt, &load_event.peak_pt, &load_event.delta_p[0], &load_event.delta_p[1], &load_event.delta_s[0], &load_event.delta_s[1], &load_event.delta_q[0], &load_event.delta_q[1]) == 11) {
				load_event.state = 1;
				
				if(load_event.timestamp > timestamp_limit) {
					add_load_event(&load_event, 0);
				
				type_counter[0]++;}
			} else {
				error_counter++;
			}
			
		} else if(strncmp(line_buffer, "PREDICTED,", 10) == 0) {
			if(sscanf(line_buffer, "PREDICTED,%li,%lf,%i,%i,%i\n", &load_event.timestamp, &load_event.outlier_score, &load_event.possible_appliances[0], &load_event.possible_appliances[1], &load_event.possible_appliances[2]) == 5) {
				load_event.state = 2;
				
				update_load_event(&load_event);
				
				type_counter[1]++;
			} else {
				error_counter++;
			}
			
		} else if(strncmp(line_buffer, "PAIRED,", 7) == 0) {
			if(sscanf(line_buffer, "PAIRED,%li,%i,%li,%i\n", &load_event.timestamp, &load_event.pair_appliance_id, &load_event.pair_timestamp, &load_event.pair_score) == 4) {
				load_event.state = 3;
				
				update_load_event(&load_event);
				
				type_counter[2]++;
			} else {
				error_counter++;
			}
			
		} else {
			error_counter++;
		}
	}
	
	LOG_INFO("Loaded %i lines from load event file. (%i DET, %i PRE, %i PAI, %i errors)", line_counter, type_counter[0], type_counter[1], type_counter[2], error_counter);
	
	return 0;
}

int load_saved_load_events() {
	time_t timestamp_now = time(NULL);
	char filename[32];
	
	generate_le_filename(timestamp_now - (24 * 3600), filename, sizeof(filename));
	
	if(access(filename, F_OK) == 0) {
		LOG_INFO("Importing yesterday's load events from file \"%s\".", filename);
		
		if(import_load_events_from_file(filename, timestamp_now - (24 * 3600)) < 0) {
			LOG_ERROR("Failed to import load events from file.");
			
			return -1;
		}
	}
	
	generate_le_filename(timestamp_now, filename, sizeof(filename));
	
	if(access(filename, F_OK) == 0) {
		LOG_INFO("Importing today's load events from file \"%s\".", filename);
		
		if(import_load_events_from_file(filename, 0) < 0) {
			LOG_ERROR("Failed to import load events from file.");
			
			return -1;
		}
	}
	
	return 0;
}

static int add_load_event(const load_event_t *load_event, int save_to_file) {
	if(load_event == NULL)
		return -1;
	
	pthread_mutex_lock(&load_event_mutex);
	
	if(save_to_file) {
		if(open_load_events_file(load_event->timestamp)) {
			pthread_mutex_unlock(&load_event_mutex);
			
			return -2;
		}
		
		if(fprintf(open_le_fd, "DETECTED,%li,%i,%i,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf,%.4lf\n", load_event->timestamp, load_event->time_gap, load_event->duration, load_event->raw_delta_pt, load_event->delta_pt, load_event->peak_pt, load_event->delta_p[0], load_event->delta_p[1], load_event->delta_s[0], load_event->delta_s[1], load_event->delta_q[0], load_event->delta_q[1]) < 0) {
			LOG_ERROR("Failed to write new load event to file.");
			
			pthread_mutex_unlock(&load_event_mutex);
			return -3;
		}
	
		fflush(open_le_fd);
	}
	
	memcpy(&load_event_buffer[load_event_buffer_pos], load_event, sizeof(load_event_t));
	
	load_event_buffer_pos = (load_event_buffer_pos + 1) % LOAD_EVENT_BUFFER_SIZE;
	if(load_event_buffer_count < LOAD_EVENT_BUFFER_SIZE)
		load_event_buffer_count++;
	
	pthread_mutex_unlock(&load_event_mutex);
	
	return 0;
}

static int update_load_event(const load_event_t *updated_load_event) {
	int pos;
	load_event_t *load_event;
	
	if(updated_load_event == NULL)
		return -1;
	
	if(pthread_mutex_lock(&load_event_mutex))
		return -2;
	
	pos = (load_event_buffer_count < LOAD_EVENT_BUFFER_SIZE) ? 0 : load_event_buffer_pos;
	
	for(int count = 0; count < load_event_buffer_count; count++) {
		load_event = &load_event_buffer[pos];
		
		if(load_event->timestamp == updated_load_event->timestamp) {
			if(updated_load_event->state == 2) {
				load_event->outlier_score = updated_load_event->outlier_score;
				load_event->possible_appliances[0] = updated_load_event->possible_appliances[0];
				load_event->possible_appliances[1] = updated_load_event->possible_appliances[1];
				load_event->possible_appliances[2] = updated_load_event->possible_appliances[2];
			} else if(updated_load_event->state == 3) {
				load_event->pair_timestamp = updated_load_event->pair_timestamp;
				load_event->pair_appliance_id = updated_load_event->pair_appliance_id;
				load_event->pair_score = updated_load_event->pair_score;
			}
			
			load_event->state = updated_load_event->state;
			
			break;
		}
		
		pos = (pos + 1) % LOAD_EVENT_BUFFER_SIZE;
	}
	
	pthread_mutex_unlock(&load_event_mutex);
	
	return 0;
}

static time_t detect_load_events(time_t last_timestamp, double nominal_line_voltage, double detection_threshold, double min_event_power) {
	power_data_t pd_buffer[DISAGGREGATION_BUFFER_SIZE];
	double ptotal_buffer[DISAGGREGATION_BUFFER_SIZE];
	double raw_ptotal_buffer[DISAGGREGATION_BUFFER_SIZE];
	load_event_t new_load_event;
	int time_gap;
	double pavg_before, pavg_after;
	
	memset(&new_load_event, 0, sizeof(load_event_t));
	
	while(get_power_data(last_timestamp, 0, pd_buffer, DISAGGREGATION_BUFFER_SIZE) == DISAGGREGATION_BUFFER_SIZE) {
		new_load_event.time_gap = (last_timestamp > 0) ? (pd_buffer[0].timestamp - last_timestamp) : 0;
		
		last_timestamp = pd_buffer[1].timestamp;
		
		time_gap = 0;
		for(int i = 0; i < DISAGGREGATION_BUFFER_SIZE; i++) {
			if(i != 0)
				time_gap += (pd_buffer[i].timestamp - pd_buffer[i - 1].timestamp) - 1;
			
			raw_ptotal_buffer[i] = pd_buffer[i].p[0] + pd_buffer[i].p[1];
			
			pd_buffer[i].p[0] = pd_buffer[i].p[0] * pow((nominal_line_voltage / pd_buffer[i].v[0]), POWER_NORMALIZATION_EXPOENT);
			pd_buffer[i].p[1] = pd_buffer[i].p[1] * pow((nominal_line_voltage / pd_buffer[i].v[1]), POWER_NORMALIZATION_EXPOENT);
			pd_buffer[i].q[0] = pd_buffer[i].q[0] * pow((nominal_line_voltage / pd_buffer[i].v[0]), POWER_NORMALIZATION_EXPOENT);
			pd_buffer[i].q[1] = pd_buffer[i].q[1] * pow((nominal_line_voltage / pd_buffer[i].v[1]), POWER_NORMALIZATION_EXPOENT);
			pd_buffer[i].s[0] = pd_buffer[i].s[0] * pow((nominal_line_voltage / pd_buffer[i].v[0]), POWER_NORMALIZATION_EXPOENT);
			pd_buffer[i].s[1] = pd_buffer[i].s[1] * pow((nominal_line_voltage / pd_buffer[i].v[1]), POWER_NORMALIZATION_EXPOENT);
			
			ptotal_buffer[i] = pd_buffer[i].p[0] + pd_buffer[i].p[1];
		}
		
		if(time_gap > MAX_TIME_GAP)
			continue;
		
		if(fabs(ptotal_buffer[1] - ptotal_buffer[0]) < detection_threshold && fabs(ptotal_buffer[2] - ptotal_buffer[1]) > detection_threshold && fabs(ptotal_buffer[3] - ptotal_buffer[1]) > detection_threshold) {
			
			pavg_before = (ptotal_buffer[0] + ptotal_buffer[1]) / 2.0;
			
			for(int k = 3; k < DISAGGREGATION_BUFFER_SIZE - 2; k++) {
				pavg_after = (ptotal_buffer[k] + ptotal_buffer[k + 1]) / 2.0;
				
				if(fabs(ptotal_buffer[k + 1] - ptotal_buffer[k]) < detection_threshold && fabs(pavg_after - pavg_before) > min_event_power) {
					
					new_load_event.timestamp = pd_buffer[1].timestamp;
					new_load_event.duration = k - 1;
					
					new_load_event.raw_delta_pt = ((raw_ptotal_buffer[k] + raw_ptotal_buffer[k + 1]) / 2.0) - ((raw_ptotal_buffer[0] + raw_ptotal_buffer[1]) / 2.0);
					
					new_load_event.delta_pt = (pavg_after - pavg_before);
					new_load_event.delta_p[0] = ((pd_buffer[k].p[0] + pd_buffer[k + 1].p[0]) / 2.0) - ((pd_buffer[0].p[0] + pd_buffer[1].p[0]) / 2.0);
					new_load_event.delta_p[1] = ((pd_buffer[k].p[1] + pd_buffer[k + 1].p[1]) / 2.0) - ((pd_buffer[0].p[1] + pd_buffer[1].p[1]) / 2.0);
					new_load_event.delta_s[0] = ((pd_buffer[k].s[0] + pd_buffer[k + 1].s[0]) / 2.0) - ((pd_buffer[0].s[0] + pd_buffer[1].s[0]) / 2.0);
					new_load_event.delta_s[1] = ((pd_buffer[k].s[1] + pd_buffer[k + 1].s[1]) / 2.0) - ((pd_buffer[0].s[1] + pd_buffer[1].s[1]) / 2.0);
					new_load_event.delta_q[0] = ((pd_buffer[k].q[0] + pd_buffer[k + 1].q[0]) / 2.0) - ((pd_buffer[0].q[0] + pd_buffer[1].q[0]) / 2.0);
					new_load_event.delta_q[1] = ((pd_buffer[k].q[1] + pd_buffer[k + 1].q[1]) / 2.0) - ((pd_buffer[0].q[1] + pd_buffer[1].q[1]) / 2.0);
					
					new_load_event.peak_pt = 0;
					if(new_load_event.delta_pt > 0.0) {
						new_load_event.peak_pt = new_load_event.delta_pt;
						
						for(int z = 2; z <= k; z++)
							if((ptotal_buffer[z] - pavg_before) > new_load_event.peak_pt)
								new_load_event.peak_pt = (ptotal_buffer[z] - pavg_before);
					}
					
					new_load_event.state = 1;
					
					add_load_event(&new_load_event, 1);
					
					last_timestamp = pd_buffer[k].timestamp;
					
					break;
				}
			}
		}
	}
	
	return last_timestamp;
}

void detect_all_load_events() {
	double nominal_line_voltage;
	double detection_threshold, min_power;
	time_t detection_timestamp, last_timestamp_detection = 0;
	
	nominal_line_voltage = config_get_value_double("nominal_line_voltage", 110, 230, 127);
	
	detection_threshold = config_get_value_double("load_event_detection_threshold", 5, 50, 20);
	min_power = config_get_value_double("load_event_min_power", 20, 100, 50);
	
	do {
		detection_timestamp = last_timestamp_detection;
		
		last_timestamp_detection = detect_load_events(detection_timestamp, nominal_line_voltage, detection_threshold, min_power);
	} while(detection_timestamp != last_timestamp_detection);
}

static int predict_load_events(const model_t *model) {
	int pos;
	load_event_t *load_event;
	
	if(model == NULL)
		return -1;
	
	pthread_mutex_lock(&load_event_mutex);
	
	pos = (load_event_buffer_count < LOAD_EVENT_BUFFER_SIZE) ? 0 : load_event_buffer_pos;
	
	for(int count = 0; count < load_event_buffer_count; count++) {
		load_event = &load_event_buffer[pos];
		
		pos = (pos + 1) % LOAD_EVENT_BUFFER_SIZE;
		
		if(load_event->state != 1)
			continue;
			
		if(predict_event(model, load_event) == 0) {
			load_event->state = 2;
			
			if(open_load_events_file(load_event->timestamp)) {
				pthread_mutex_unlock(&load_event_mutex);
				
				return -2;
			}
			
			if(open_le_fd == NULL || fprintf(open_le_fd, "PREDICTED,%li,%.4lf,%i,%i,%i\n", load_event->timestamp, load_event->outlier_score, load_event->possible_appliances[0], load_event->possible_appliances[1], load_event->possible_appliances[2]) < 0) {
				LOG_ERROR("Failed to write load event prediction to file.");
				
				pthread_mutex_unlock(&load_event_mutex);
				return -3;
			}
			
			fflush(open_le_fd);
		}
	}
	
	pthread_mutex_unlock(&load_event_mutex);
	
	return 0;
}

static int has_common_appliance_id(const load_event_t *le1, const load_event_t *le2) {
	int i, j;
	int qty = 0;
	
	if(le1 == NULL || le2 == NULL)
		return 0;
	
	for(i = 0; i < 3; i++)
		for(j = 0; j < 3; j++)
			if(le1->possible_appliances[i] != 0 && le1->possible_appliances[i] == le2->possible_appliances[j])
				qty++;
	
	return qty;
}

static int get_pair_score(int appliance_id, const load_event_t *load_event_off, const load_event_t *load_event_on) {
	int score = 0;
	int i, j;
	
	if(load_event_off == NULL || load_event_on == NULL || appliance_id <= 0)
		return 0;
	
	if((load_event_off->timestamp - load_event_on->timestamp) > (12*3600))
		score -= 5000;
	
	score -= load_event_off->time_gap * 1000;
	score -= load_event_on->time_gap * 1000;
	score -= 20000 * (fabs(load_event_on->delta_pt + load_event_off->delta_pt)/load_event_on->delta_pt);
	
	for(i = 0; i < 3; i++)
		for(j = 0; j < 3; j++)
			if(load_event_off->possible_appliances[i] == load_event_on->possible_appliances[j] && load_event_off->possible_appliances[i] == appliance_id) {
				score += 10000;
				
				
				return score;
			}
	
	return score;
}

static int pair_load_events() {
	time_t timestamp_limit = time(NULL) - (3600 * 24);
	int count_off, pos_off;
	int count_on, pos_on;
	load_event_t *load_event_off = NULL;
	int appliance_idx, best_pair_appliance_id;
	int score_penalty;
	load_event_t *load_event_on = NULL;
	int pair_score, best_pair_score;
	load_event_t *best_pair_load_event = NULL;
	double classification_inliner_threshold, classification_outlier_threshold, classification_absolute_outlier_threshold;
	
	classification_inliner_threshold = config_get_value_double("classification_inliner_threshold", 0.5, 2, 1);
	classification_outlier_threshold = config_get_value_double("classification_outlier_threshold", 1, 10, 1.5);
	classification_absolute_outlier_threshold = config_get_value_double("classification_absolute_outlier_threshold", 5, 50, 20);
	
	pthread_mutex_lock(&load_event_mutex);
	
	pos_off = (load_event_buffer_count < LOAD_EVENT_BUFFER_SIZE) ? 0 : load_event_buffer_pos;
	
	for(count_off = 0; count_off < load_event_buffer_count; count_off++) {
		load_event_off = &load_event_buffer[pos_off];
		
		pos_off = (pos_off + 1) % LOAD_EVENT_BUFFER_SIZE;
		
		if(load_event_off->state != 2 || load_event_off->delta_pt >= 0.0)
			continue;
		
		if(load_event_off->timestamp < timestamp_limit || load_event_off->outlier_score >= classification_absolute_outlier_threshold)
			continue;
		
		score_penalty = 0;
		best_pair_appliance_id = 0;
		best_pair_score = 0;
		best_pair_load_event = NULL;
		
		pos_on = pos_off - 2;
		if(pos_on < 0)
			pos_on += LOAD_EVENT_BUFFER_SIZE;
		
		for(count_on = 0; count_on < load_event_buffer_count; count_on++) {
			load_event_on = &load_event_buffer[pos_on];
			
			if(--pos_on < 0)
				pos_on += LOAD_EVENT_BUFFER_SIZE;
			
			if(load_event_on->timestamp < timestamp_limit || load_event_on->timestamp >= load_event_off->timestamp)
				break;
			
			if(load_event_on->state != 2)
				continue;
			
			if(has_common_appliance_id(load_event_off, load_event_on) == 0 || load_event_on->outlier_score >= classification_absolute_outlier_threshold)
				continue;
			
			// Se encontrar outro evento de desligamento com ID em comum, aumenta a penalidade e pula pro próximo
			if(load_event_on->delta_pt <= 0.0) {
				score_penalty += 1000;
				
				// Se o valor de potência for próximo, aumenta ainda mais a penalidade
				if(fabs(load_event_on->delta_pt - load_event_off->delta_pt)/(-load_event_on->delta_pt) < 0.1)
					score_penalty += 1000;
				
				continue;
			}
			
			for(appliance_idx = 0; appliance_idx < 3; appliance_idx++) {
				
				pair_score = get_pair_score(load_event_off->possible_appliances[appliance_idx], load_event_off, load_event_on);
				
				if(load_event_off->outlier_score <= classification_inliner_threshold)
					pair_score += 1000;
				else if(load_event_off->outlier_score >= classification_outlier_threshold)
					pair_score -= 1000;
				
				if(load_event_on->outlier_score <= classification_inliner_threshold)
					pair_score += 1000;
				else if(load_event_on->outlier_score >= classification_outlier_threshold)
					pair_score -= 1000;
				
				pair_score -= score_penalty;
				
				// Como o valor inicial de best_pair_score é zero, pontuações iguais ou abaixo de zero não serão pareadas
				if(pair_score > best_pair_score) {
					best_pair_score = pair_score;
					best_pair_load_event = load_event_on;
					best_pair_appliance_id = load_event_off->possible_appliances[appliance_idx];
				}
			}
			
			score_penalty += 200;
		}
		
		if(best_pair_load_event) {
			load_event_off->state = best_pair_load_event->state = 3;
			
			load_event_off->pair_appliance_id = best_pair_appliance_id;
			best_pair_load_event->pair_appliance_id = best_pair_appliance_id;
			
			load_event_off->pair_timestamp = best_pair_load_event->timestamp;
			best_pair_load_event->pair_timestamp = load_event_off->timestamp;
			
			load_event_off->pair_score = best_pair_score;
			best_pair_load_event->pair_score = best_pair_score;
			
			if(open_load_events_file(load_event_off->timestamp)) {
				pthread_mutex_unlock(&load_event_mutex);
				
				return -2;
			}
			
			if(open_le_fd == NULL || fprintf(open_le_fd, "PAIRED,%li,%i,%li,%i\n", load_event_off->timestamp, best_pair_appliance_id, best_pair_load_event->timestamp, best_pair_score) < 0 || fprintf(open_le_fd, "PAIRED,%li,%i,%li,%i\n", best_pair_load_event->timestamp, best_pair_appliance_id, load_event_off->timestamp, best_pair_score) < 0) {
				LOG_ERROR("Failed to write load event pairing to file.");
				
				pthread_mutex_unlock(&load_event_mutex);
				return -3;
			}
			
			fflush(open_le_fd);
			
			energy_add_power_disaggregated(best_pair_appliance_id, best_pair_load_event->timestamp, load_event_off->timestamp, best_pair_load_event->raw_delta_pt, fabs(load_event_off->raw_delta_pt));
		}
	}
	
	pthread_mutex_unlock(&load_event_mutex);
	
	return 0;
}

time_t get_last_detected_load_event_timestamp() {
	int pos;
	time_t last_timestamp = 0;
	
	pthread_mutex_lock(&load_event_mutex);
	
	pos = (load_event_buffer_count < LOAD_EVENT_BUFFER_SIZE) ? 0 : load_event_buffer_pos;
	
	for(int count = 0; count < load_event_buffer_count; count++) {
		if(load_event_buffer[pos].state != 0)
			last_timestamp = load_event_buffer[pos].timestamp;
		
		pos = (pos + 1) % LOAD_EVENT_BUFFER_SIZE;
	}
	
	pthread_mutex_unlock(&load_event_mutex);
	
	return last_timestamp;
}

void *disaggregation_loop(void *argp) {
	int *terminate = (int*) argp;
	
	double nominal_line_voltage;
	double detection_threshold, min_power;
	time_t last_timestamp_detection = get_last_detected_load_event_timestamp();
	
	load_signature_t *signatures;
	int signature_qty;
	time_t model_signature_last_modification = 0, signature_last_modification;
	model_t *current_model = NULL, *new_model;
	
	srand(time(NULL));
	
	while(!(*terminate)) {
		nominal_line_voltage = config_get_value_double("nominal_line_voltage", 110, 230, 127);
		
		detection_threshold = config_get_value_double("load_event_detection_threshold", 5, 50, 20);
		min_power = config_get_value_double("load_event_min_power", 20, 100, 50);
		
		last_timestamp_detection = detect_load_events(last_timestamp_detection, nominal_line_voltage, detection_threshold, min_power);
		
		if(config_get_value_int("perform_disaggregation", 0, 1, 0) == 0) {
			sleep(2);
			continue;
		}
		
		signature_last_modification = get_last_signature_modification();
		
		if(signature_last_modification > model_signature_last_modification && (time(NULL) - signature_last_modification) > 30) {
			model_signature_last_modification = signature_last_modification;
			
			signature_qty = fetch_signatures(&signatures, min_power);
			
			if(signature_qty > TRAINING_MIN_SIGNATURE_QTY) {
				LOG_INFO("Training model using %i appliance signatures.", signature_qty);
				new_model = train_model(signatures, signature_qty);
				
				free(signatures);
				
				if(new_model) {
					LOG_INFO("Training completed.");
					free_model_content(current_model);
					free(current_model);
					
					current_model = new_model;
				}
			}
		}
		
		predict_load_events(current_model);
		
		pair_load_events();
		
		sleep(1);
	}
	
	close_load_events_file();
	
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
