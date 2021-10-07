#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <math.h>
#include <float.h>
#include <errno.h>

#include "common.h"
#include "logger.h"
#include "appliances.h"
#include "disaggregation.h"
#include "classification.h"

#define DIMENSION_ON 7
#define DIMENSION_OFF 6
#define K_VALUE_MIN 5
#define K_VALUE_MAX 30
#define CROSSV_FOLDS 10

typedef struct knn_s {
	int dim;
	int n;
	
	double *points;
	int *labels;
	
	int nr_classes;
	
	int minpts;
	double *k_distance;
	double *lrd;
	
	double lof_min;
	double lof_max;
	double lof_avg;
	double lof_90p;
} knn_t;

typedef struct model_s {
	knn_t on;
	knn_t off;
	
	int kvalue_on;
	int kvalue_off;
} model_t;

static void signature_to_features(const load_signature_t *signature, double *feature_array) {
	if(signature == NULL || feature_array == NULL)
		return;
	
	feature_array[0] = signature->delta_pt;
	feature_array[1] = signature->delta_p[0];
	feature_array[2] = signature->delta_p[1];
	feature_array[3] = signature->delta_s[0];
	feature_array[4] = signature->delta_s[1];
	feature_array[5] = signature->duration;
	feature_array[6] = signature->peak_pt;
}

static void load_event_to_features(const load_event_t *l_event, double *feature_array) {
	if(l_event == NULL || feature_array == NULL)
		return;
	
	feature_array[0] = l_event->delta_pt;
	feature_array[1] = l_event->delta_p[0];
	feature_array[2] = l_event->delta_p[1];
	feature_array[3] = l_event->delta_s[0];
	feature_array[4] = l_event->delta_s[1];
	feature_array[5] = l_event->duration;
	feature_array[6] = l_event->peak_pt;
}

void knn_init(knn_t *model, int dim, int n) {
	
	if(!model || dim <= 0 || n <= 0)
		return;
	
	model->dim = dim;
	model->n = n;
	
	model->points = (double*) calloc(n * dim, sizeof(double));
	model->labels = (int*) calloc(n, sizeof(int));
	
	model->minpts = 0;
	model->k_distance = NULL;
	model->lrd = NULL;
}

void knn_free(knn_t *model) {
	if(!model)
		return;
	
	free(model->points);
	free(model->labels);
	
	free(model->k_distance);
	free(model->lrd);
	
	model->points = NULL;
	model->labels = NULL;
	
	model->k_distance = NULL;
	model->lrd = NULL;
}

static int count_classes(const int *input_labels, int input_labels_len, int **output_labels, int **output_counts) {
	int classes = 0;
	int max_class_count = 16;
	int *labels;
	int *counts;
	int found;
	
	labels = (int*) malloc(max_class_count * sizeof(int));
	counts = (int*) malloc(max_class_count * sizeof(int));
	
	for(int i = 0; i < input_labels_len; i++) {
		found = 0;
		
		for(int j = 0; j < classes; j++)
			if(labels[j] == input_labels[i]) {
				counts[j] += 1;
				found = 1;
				break;
			}
		
		if(!found) {
			labels[classes] = input_labels[i];
			counts[classes] = 1;
			
			classes++;
			
			if(classes == max_class_count) {
				max_class_count *= 2;
				labels = (int*) realloc(labels, max_class_count * sizeof(int));
				counts = (int*) realloc(counts, max_class_count * sizeof(int));
			}
		}
	}
	
	if(output_labels)
		*output_labels = labels;
	else
		free(labels);
	
	if(output_counts)
		*output_counts = counts;
	else
		free(counts);
	
	return classes;
}


static int group_classes(const int *input_labels, int input_labels_len, int **output_labels, int **output_counts, int **output_permutation) {
	int n_classes;
	int pos = 0;
	
	if(!input_labels || !output_labels || !output_counts || !output_permutation)
		return -1;
	
	n_classes = count_classes(input_labels, input_labels_len, output_labels, output_counts);
	
	*output_permutation = (int*) calloc(input_labels_len, sizeof(int));
	
	for(int c = 0; c < n_classes; c++)
		for(int i = 0; i < input_labels_len; i++)
			if(input_labels[i] == (*output_labels)[c])
				(*output_permutation)[pos++] = i;
	
	return n_classes;
}

static void sort_classes(int *labels, int *counts, int len) {
	double pivot;
	
	int i = 0;
	int j = len - 1;
	
	int aux;
	
	if(len < 2)
		return;
	
	pivot = counts[len / 2];
	
	while(1) {
		while (counts[i] > pivot)
			i++;
		
		while (counts[j] < pivot)
			j--;
		
		if (i >= j)
			break;
		
		aux = counts[i];
		counts[i] = counts[j];
		counts[j] = aux;
		
		aux = labels[i];
		labels[i] = labels[j];
		labels[j] = aux;
		
		i++;
		j--;
	}
	
	sort_classes(labels, counts, i);
	sort_classes(&labels[i], &counts[i], len - i);
}

static int cmp_double(const void *p1, const void *p2) {
	if(*((double*)p1) > *((double*)p2))
		return 1;
	
	if(*((double*)p1) < *((double*)p2))
		return -1;
	
	return 0;
}

static void knn_prepare_lof(knn_t *model, int minpts) {
	int n, dim;
	double *tmp_distances;
	double *distance_matrix;
	int *neighborhood_matrix;
	int *neighborhood_size;
	double *lof;
	
	if(!model || minpts < 1 || minpts > model->n)
		return;
	
	n = model->n;
	dim = model->dim;
	
	if(model->k_distance || model->lrd)
		return;
	
	model->k_distance = (double*) calloc(n, sizeof(double));
	model->lrd = (double*) calloc(n, sizeof(double));
	
	distance_matrix = (double*) calloc(n * n, sizeof(double));
	neighborhood_matrix = (int*) calloc(n * n, sizeof(int));
	neighborhood_size = (int*) calloc(n, sizeof(int));
	lof = (double*) calloc(n, sizeof(double));
	
	tmp_distances = (double*) calloc(n, sizeof(double));
	
	for(int i = 0; i < n; i++) {
		tmp_distances[i] = DBL_MAX;
		distance_matrix[i * n + i] = DBL_MAX;
		
		for(int j = 0; j < n; j++) {
			if(i == j)
				continue;
			
			distance_matrix[i * n + j] = 0;
			
			for(int d = 0; d < dim; d++)
				distance_matrix[i * n + j] += pow((model->points[j * dim + d] - model->points[i * dim + d]), 2);
			
			distance_matrix[i * n + j] = sqrt(distance_matrix[i * n + j]);
			
			tmp_distances[j] = distance_matrix[i * n + j];
		}
		
		qsort(tmp_distances, n, sizeof(double), cmp_double);
		
		model->k_distance[i] = tmp_distances[minpts - 1];
	}
	
	free(tmp_distances);
	
	for(int i = 0; i < n; i++) {
		neighborhood_size[i] = 0;
		
		for(int j = 0; j < n; j++) {
			if(i == j)
				continue;
			
			if(distance_matrix[i * n + j] <= model->k_distance[i]) {
				neighborhood_matrix[i * n + j] = 1;
				neighborhood_size[i]++;
			} else {
				neighborhood_matrix[i * n + j] = 0;
			}
		}
	}
	
	for(int i = 0; i < n; i++) {
		double rd_sum = 0;
		
		for(int j = 0; j < n; j++) {
			if(i == j)
				continue;
			
			if(neighborhood_matrix[i * n + j])
				rd_sum += MAX(model->k_distance[j], distance_matrix[i * n + j]);
		}
		
		model->lrd[i] = ((double)neighborhood_size[i]) / rd_sum;
	}
	
	for(int i = 0; i < n; i++) {
		double lrd_sum = 0;
		
		for(int j = 0; j < n; j++) {
			if(i == j)
				continue;
			
			if(neighborhood_matrix[i * n + j])
				lrd_sum += model->lrd[j];
		}
		
		lof[i] = lrd_sum / (((double)neighborhood_size[i]) * model->lrd[i]);
	}
	
	model->lof_min = DBL_MAX;
	model->lof_max = 0;
	model->lof_avg = 0;
	
	for(int i = 0; i < n; i++) {
		model->lof_avg += lof[i];
		
		if(lof[i] > model->lof_max)
			model->lof_max = lof[i];
		
		if(lof[i] < model->lof_min)
			model->lof_min = lof[i];
	}
	
	model->lof_avg = model->lof_avg / (double)n;
	
	qsort(lof, n, sizeof(double), cmp_double);
	
	model->lof_90p = lof[(n * 90)/100];
	
	free(distance_matrix);
	free(neighborhood_matrix);
	free(neighborhood_size);
	free(lof);
	
	model->minpts = minpts;
}

static int knn_predict(const knn_t *model, const double *point, int kvalue, double *lof, int *output_class_n, int **output_labels, int **output_counts) {
	int dim, n;
	double *distances;
	double *tmp_distances;
	double k_distance;
	int neighborhood_size;
	int *neighborhood;
	int neighborhood_array_size;
	double rd_sum = 0;
	double lrd_sum = 0;
	double lrd;
	int neighborhood_class_n;
	int *neighborhood_labels;
	int *neighborhood_counts;
	int max_count = 0, max_count_label = -1;
	
	if(!model || !point || kvalue < 1 || kvalue > model->n)
		return -1;
	
	dim = model->dim;
	n = model->n;
	
	distances = (double*) calloc(n, sizeof(double));
	tmp_distances = (double*) calloc(n, sizeof(double));
	
	for(int i = 0; i < n; i++) {
		distances[i] = 0;
		
		for(int d = 0; d < dim; d++)
			distances[i] += pow((point[d] - model->points[i * dim + d]), 2);
		
		distances[i] = sqrt(distances[i]);
		
		tmp_distances[i] = distances[i];
	}
	
	qsort(tmp_distances, n, sizeof(double), cmp_double);
	
	k_distance = tmp_distances[kvalue - 1];
	
	free(tmp_distances);
	
	neighborhood_size = 0;
	neighborhood_array_size = kvalue;
	neighborhood = (int*) malloc(neighborhood_array_size * sizeof(int));
	
	for(int i = 0; i < n; i++)
		if(distances[i] <= k_distance) {
			
			if(neighborhood_size == neighborhood_array_size) {
				neighborhood_array_size *= 2;
				neighborhood = (int*) realloc(neighborhood, neighborhood_array_size * sizeof(int));
			}
			
			neighborhood[neighborhood_size] = model->labels[i];
			
			neighborhood_size++;
		}
	
	if(lof && model->k_distance && model->lrd && model->minpts > 0 && model->minpts <= n) {
		for(int i = 0; i < n; i++)
			if(distances[i] <= k_distance)
				rd_sum += MAX(model->k_distance[i], distances[i]);
		
		lrd = ((double)neighborhood_size) / rd_sum;
		
		for(int i = 0; i < n; i++)
			if(distances[i] <= k_distance)
				lrd_sum += model->lrd[i];
		
		*lof = lrd_sum / (((double)neighborhood_size) * lrd);
	}
	
	free(distances);
	
	neighborhood_class_n = count_classes(neighborhood, neighborhood_size, &neighborhood_labels, &neighborhood_counts);
	
	for(int i = 0; i < neighborhood_class_n; i++)
		if(neighborhood_counts[i] > max_count) {
			max_count = neighborhood_counts[i];
			max_count_label = neighborhood_labels[i];
		}
	
	if(output_class_n && output_labels && output_counts) {
		*output_class_n = neighborhood_class_n;
		*output_labels = neighborhood_labels;
		*output_counts = neighborhood_counts;
	} else {
		free(neighborhood_labels);
		free(neighborhood_counts);
	}
	
	return max_count_label;
}

static double knn_cross_validation(int kfold, const double *points, const int *labels, int dim, int n, int kvalue) {
	int n_classes;
	int *class_labels;
	int *class_counts;
	int *class_start;
	int *perm;
	int *fold_count;
	int *fold_start;
	int *fold_pos;
	int *fold_perm;
	knn_t crossv_model;
	int *result_labels;
	int correct_count = 0;
	
	if(kfold > n)
		kfold = n;
	
	n_classes = group_classes(labels, n, &class_labels, &class_counts, &perm);
	
	if(n_classes < 1)
		return 0;
	
	class_start = (int*) calloc(n_classes, sizeof(int));
	
	class_start[0] = 0;
	for(int i = 1; i < n_classes; i++)
		class_start[i] = class_start[i - 1] + class_counts[i - 1];
	
	// Embaralha os elementos de cada classe usando o algoritmo de Sattolo
	for(int c = 0; c < n_classes; c++) {
		int i = class_counts[c] - 1;
		
		while(i > 0) {
			int j = rand() % i;
			int aux;
			
			aux = perm[class_start[c] + i];
			perm[class_start[c] + i] = perm[class_start[c] + j];
			perm[class_start[c] + j] = aux;
			
			i--;
		}
	}
	
	fold_count = (int*) calloc(kfold, sizeof(int));
	
	// Calcula a quantidade de elementos em cada divisão
	for(int f = 0; f < kfold; f++) {
		fold_count[f] = 0;
		
		for(int c = 0; c < n_classes; c++)
			fold_count[f] += ((f + 1) * class_counts[c]) / kfold - (f * class_counts[c]) / kfold;
	}
	
	fold_start = (int*) calloc(kfold+1, sizeof(int));
	
	fold_start[0] = 0;
	for(int f = 1; f <= kfold; f++)
		fold_start[f] = fold_start[f - 1] + fold_count[f - 1];
	
	fold_pos = (int*) calloc(kfold, sizeof(int));
	
	for(int f = 0; f < kfold; f++)
		fold_pos[f] = fold_start[f];
	
	fold_perm = (int*) calloc(n, sizeof(int));
	
	// Distribui os elementos de cada classe entre as divisões
	for(int c = 0; c < n_classes; c++)
		for(int f = 0; f < kfold; f++)
			for(int ci = ((f * class_counts[c]) / kfold); ci < (((f + 1) * class_counts[c]) / kfold); ci++) {
				fold_perm[fold_pos[f]] = perm[class_start[c] + ci];
				fold_pos[f]++;
			}
	
	free(fold_pos);
	free(class_start);
	
	result_labels = (int*) calloc(n, sizeof(int));
	
	for(int f = 0; f < kfold; f++) {
		int j = 0;
		
		knn_init(&crossv_model, dim, n - fold_count[f]);
		
		for(int i = 0; i < fold_start[f]; i++) {
			for(int d = 0; d < dim; d++)
				crossv_model.points[j*dim + d] = points[fold_perm[i]*dim + d];
			
			crossv_model.labels[j] = labels[fold_perm[i]];
			
			j++;
		}
		
		for(int i = fold_start[f+1]; i < n; i++) {
			for(int d = 0; d < dim; d++)
				crossv_model.points[j*dim + d] = points[fold_perm[i]*dim + d];
			
			crossv_model.labels[j] = labels[fold_perm[i]];
			
			j++;
		}
		
		for(int i = fold_start[f]; i < fold_start[f+1]; i++)
			result_labels[fold_perm[i]] = knn_predict(&crossv_model, &(points[fold_perm[i]*dim]), kvalue, NULL, NULL, NULL, NULL);
		
		knn_free(&crossv_model);
	}
	
	free(fold_count);
	free(fold_start);
	free(fold_perm);
	
	for(int i = 0; i < n; i++)
		if(result_labels[i] == labels[i])
			correct_count++;
	
	free(result_labels);
	
	return ((double)correct_count)/((double)n);
}

void free_model_content(model_t *model) {
	if(model) {
		knn_free(&model->on);
		knn_free(&model->off);
	}
}

static int knn_find_best_k(const knn_t *model, int min_k, int max_k, int folds, double *rate) {
	int best_k = min_k;
	double best_rate = 0;
	double current_rate;
	
	if(!model)
		return -1;
	
	for(int current_k = min_k; current_k <= max_k; current_k++) {
		current_rate = knn_cross_validation(folds, model->points, model->labels, model->dim, model->n, current_k);
		
		if(current_rate > best_rate) {
			best_rate = current_rate;
			best_k = current_k;
		}
	}
	
	if(rate)
		*rate = best_rate;
	
	return best_k;
}

model_t *train_model(const load_signature_t *signatures, int signature_qty) {
	model_t *new_model;
	
	int sig_qty_on, sig_qty_off;
	int sig_on_idx, sig_off_idx;
	
	double features[DIMENSION_ON];
	
	double rate_on, rate_off;
	
	
	if(signatures == NULL || signature_qty <= 0)
		return NULL;
	
	sig_qty_on = sig_qty_off = 0;
	
	for(int i = 0; i < signature_qty; i++) {
		if(signatures[i].delta_pt > 0.0)
			sig_qty_on++;
		else
			sig_qty_off++;
	}
	
	if(sig_qty_on < CROSSV_FOLDS || sig_qty_off < CROSSV_FOLDS) {
		LOG_INFO("Insufficient signatures for training, aborting...");
		
		return NULL;
	}
	
	if(MIN(((float)sig_qty_off) / ((float)sig_qty_on), ((float)sig_qty_on) / ((float)sig_qty_off)) < 0.5) {
		LOG_INFO("ON/OFF signature quantity mismatch, aborting the training...");
		
		return NULL;
	}
	
	new_model = (model_t*) calloc(1, sizeof(model_t));
	
	if(new_model == NULL)
		return NULL;
	
	knn_init(&new_model->on, DIMENSION_ON, sig_qty_on);
	knn_init(&new_model->off, DIMENSION_OFF, sig_qty_off);
	
	sig_on_idx = sig_off_idx = 0;
	
	for(int i = 0; i < signature_qty; i++) {
		signature_to_features(&signatures[i], features);
		
		if(signatures[i].delta_pt > 0.0) {
			new_model->on.labels[sig_on_idx] = signatures[i].appliance_id;
			
			for(int d = 0; d < DIMENSION_ON; d++)
				new_model->on.points[sig_on_idx*DIMENSION_ON + d] = features[d];
			
			sig_on_idx++;
		} else {
			new_model->off.labels[sig_off_idx] = signatures[i].appliance_id;
			
			for(int d = 0; d < DIMENSION_OFF; d++)
				new_model->off.points[sig_off_idx*DIMENSION_OFF + d] = features[d];
			
			sig_off_idx++;
		}
	}
	
	LOG_INFO("Finding the best k values.");
	
	new_model->kvalue_on = knn_find_best_k(&new_model->on, K_VALUE_MIN, K_VALUE_MAX, CROSSV_FOLDS, &rate_on);
	new_model->kvalue_off = knn_find_best_k(&new_model->off, K_VALUE_MIN, K_VALUE_MAX, CROSSV_FOLDS, &rate_off);
	
	LOG_INFO("Best k values: ON %i (%.1lf %%) \\ OFF %i (%.1lf %%)", new_model->kvalue_on, rate_on * 100.0, new_model->kvalue_off, rate_off * 100.0);
	
	LOG_INFO("Calculating training set LOF values.");
	
	knn_prepare_lof(&new_model->on, 3);
	knn_prepare_lof(&new_model->off, 3);
	
	if(new_model->on.nr_classes != new_model->off.nr_classes) {
		LOG_ERROR("ON and OFF models have different number of classes, aborting training.");
		free_model_content(new_model);
		free(new_model);
		
		return NULL;
	}
	
	LOG_INFO("ON LOF:  min %4.1lf | 90thp %6.1lf | avg %6.1lf | max %6.1lf", new_model->on.lof_min, new_model->on.lof_90p, new_model->on.lof_avg, new_model->on.lof_max);
	LOG_INFO("OFF LOF: min %4.1lf | 90thp %6.1lf | avg %6.1lf | max %6.1lf", new_model->off.lof_min, new_model->off.lof_90p, new_model->off.lof_avg, new_model->off.lof_max);
	
	return new_model;
}

int predict_event(const model_t *model, load_event_t *l_event) {
	const knn_t *suitable_model;
	int kvalue;
	double features[DIMENSION_ON];
	int neighborhood_class_n;
	int *neighborhood_labels;
	int *neighborhood_counts;
	
	if(model == NULL || l_event == NULL)
		return -1;
	
	suitable_model = (l_event->delta_pt > 0.0) ? &model->on : &model->off;
	kvalue = (l_event->delta_pt > 0.0) ? model->kvalue_on : model->kvalue_off;
	
	load_event_to_features(l_event, features);
	
	knn_predict(suitable_model, features, kvalue, &(l_event->outlier_score), &neighborhood_class_n, &neighborhood_labels, &neighborhood_counts);
	
	sort_classes(neighborhood_labels, neighborhood_counts, neighborhood_class_n);
	
	for(int i = 0; i < 3 && i < neighborhood_class_n; i++)
		l_event->possible_appliances[i] = neighborhood_labels[i];
	
	free(neighborhood_labels);
	free(neighborhood_counts);
	
	return 0;
}
