#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <math.h>
#include <float.h>
#include <errno.h>

#include <libsvm/svm.h>

#include "common.h"
#include "logger.h"
#include "appliances.h"
#include "disaggregation.h"
#include "classification.h"

#define SVM_PARAM_QTY_ON 6
#define SVM_PARAM_QTY_OFF 5
#define SVM_CROSSV_FOLD_NUM 5

typedef struct model_s {
	struct svm_model *model_on;
	struct svm_model *model_off;
	struct svm_node *node_pool;
	double max_on[SVM_PARAM_QTY_ON];
	double min_on[SVM_PARAM_QTY_ON];
	double max_off[SVM_PARAM_QTY_OFF];
	double min_off[SVM_PARAM_QTY_OFF];
	int nr_classes;
} model_t;

static void signature_to_features(const load_signature_t *signature, double *feature_array) {
	if(signature == NULL || feature_array == NULL)
		return;
	
	feature_array[0] = signature->delta_p[0];
	feature_array[1] = signature->delta_p[1];
	feature_array[2] = signature->delta_s[0];
	feature_array[3] = signature->delta_s[1];
	feature_array[4] = signature->duration;
	feature_array[5] = signature->peak_pt;
}

static void load_event_to_features(load_event_t *l_event, double *feature_array) {
	if(l_event == NULL || feature_array == NULL)
		return;
	
	feature_array[0] = l_event->delta_p[0];
	feature_array[1] = l_event->delta_p[1];
	feature_array[2] = l_event->delta_s[0];
	feature_array[3] = l_event->delta_s[1];
	feature_array[4] = l_event->duration;
	feature_array[5] = l_event->peak_pt;
}

static void svm_print_string_f(const char *s) {
	LOG_DEBUG(s);
}

static double do_cross_validation(const struct svm_problem *prob, const struct svm_parameter *param, int nr_fold) {
	double *cross_validation_result = NULL;
	int correct = 0;
	
	cross_validation_result = (double*) malloc(sizeof(double) * prob->l);
	
	if(cross_validation_result == NULL)
		return -1;
	
	svm_cross_validation(prob, param, nr_fold, cross_validation_result);
	
	for(int i = 0; i < prob->l; i++)
		if(prob->y[i] == cross_validation_result[i])
			correct++;
	
	free(cross_validation_result);
	
	return ((double)correct / (double) prob->l);
}

static double do_grid_search(const struct svm_problem *prob, const struct svm_parameter *param, int nr_fold, const double *C_exp_range, double *g_exp_range, double *result_C_exp, double *result_g_exp) {
	struct svm_parameter param_copy;
	
	double C_begin, C_end, C_step;
	double g_begin, g_end, g_step;
	
	const char *parameter_error;
	
	double rate;
	double best_rate = 0;
	
	if(prob == NULL || param == NULL || C_exp_range == NULL || g_exp_range == NULL || result_C_exp == NULL || result_g_exp == NULL)
		return -1;
	
	C_begin = C_exp_range[0];
	C_end = C_exp_range[1];
	C_step = C_exp_range[2];
	
	g_begin = g_exp_range[0];
	g_end = g_exp_range[1];
	g_step = g_exp_range[2];
	
	memcpy(&param_copy, param, sizeof(struct svm_parameter));
	
	for(double C_exp = C_begin; ((C_step > 0.0) ? (C_exp < C_end) : (C_exp > C_end)); C_exp += C_step) {
		param_copy.C = pow(2.0, C_exp);
		
		for(double g_exp = g_begin; ((g_step > 0.0) ? (g_exp < g_end) : (g_exp > g_end)); g_exp += g_step) {
			param_copy.gamma = pow(2.0, g_exp);
			
			parameter_error = svm_check_parameter(prob, &param_copy);
			if(parameter_error) {
				LOG_ERROR("Parameter error during grid search: %s\n", parameter_error);
				return -2;
			}
			
			rate = do_cross_validation(prob, &param_copy, nr_fold);
			
			if(rate > best_rate) {
				best_rate = rate;
				*result_C_exp = C_exp;
				*result_g_exp = g_exp;
			}
		}
	}
	
	return best_rate;
}

model_t *train_model(const load_signature_t *signatures, int signature_qty) {
	model_t *new_model;
	int sig_qty_on, sig_qty_off;
	
	double features[SVM_PARAM_QTY_ON];
	
	struct svm_parameter svm_on_param, svm_off_param;
	struct svm_problem svm_on_prob, svm_off_prob;
	
	struct svm_node *svm_node_pool;
	int node_pool_index;
	
	int svm_problem_index_on, svm_problem_index_off;
	
	const char *error_str;
	
	int crossv_fold = SVM_CROSSV_FOLD_NUM;
	double C_exp_range[3] = {-5.0, 15.0, 1.0};
	double g_exp_range[3] = {3.0, -15.0, -1.0};
	double C_exp_best, g_exp_best;
	
	double scaled;
	double rate_on, rate_off;
	
	if(signatures == NULL || signature_qty <= 0)
		return NULL;
	
	svm_set_print_string_function(&svm_print_string_f);
	
	new_model = (model_t*) malloc(sizeof(model_t));
	
	if(new_model == NULL)
		return NULL;
	
	svm_on_param.svm_type = C_SVC;
	svm_on_param.kernel_type = RBF;
	svm_on_param.degree = 3;
	svm_on_param.coef0 = 0;
	svm_on_param.cache_size = 100;
	svm_on_param.C = 100.0;
	svm_on_param.eps = 1e-3;
	svm_on_param.shrinking = 1;
	svm_on_param.probability = 1;
	svm_on_param.nr_weight = 0;
	svm_on_param.weight_label = NULL;
	svm_on_param.weight = NULL;
	
	memcpy(&svm_off_param, &svm_on_param, sizeof(struct svm_parameter));
	
	svm_on_param.gamma = 1.0 / (double)SVM_PARAM_QTY_ON;
	svm_off_param.gamma = 1.0 / (double)SVM_PARAM_QTY_OFF;
	
	for(int i = 0; i < SVM_PARAM_QTY_ON; i++) {
		new_model->max_on[i] = 0;
		new_model->min_on[i] = DBL_MAX;
	}
	
	for(int i = 0; i < SVM_PARAM_QTY_OFF; i++) {
		new_model->max_off[i] = 0;
		new_model->min_off[i] = DBL_MAX;
	}
	
	sig_qty_on = sig_qty_off = 0;
	
	for(int i = 0; i < signature_qty; i++) {
		signature_to_features(&signatures[i], features);
		
		if(signatures[i].delta_pt > 0.0) {
			sig_qty_on++;
			for(int j = 0; j < SVM_PARAM_QTY_ON; j++) {
				new_model->max_on[j] = MAX(new_model->max_on[j], features[j]);
				new_model->min_on[j] = MIN(new_model->min_on[j], features[j]);
			}
		} else {
			sig_qty_off++;
			for(int j = 0; j < SVM_PARAM_QTY_OFF; j++) {
				new_model->max_off[j] = MAX(new_model->max_off[j], features[j]);
				new_model->min_off[j] = MIN(new_model->min_off[j], features[j]);
			}
		}
	}
	
	if(sig_qty_on < SVM_CROSSV_FOLD_NUM || sig_qty_off < SVM_CROSSV_FOLD_NUM || MIN(((float)sig_qty_off) / ((float)sig_qty_on), ((float)sig_qty_on) / ((float)sig_qty_off)) < 0.5) {
		LOG_INFO("ON/OFF signature quantity mismatch, aborting the training.");
		
		return NULL;
	}
	
	LOG_INFO("Training models using %d ON and %d OFF power signatures.", sig_qty_on, sig_qty_off);
	
	svm_on_prob.l = sig_qty_on;
	svm_off_prob.l = sig_qty_off;
	
	svm_on_prob.y = (double*) malloc(sizeof(double) * sig_qty_on);
	svm_off_prob.y = (double*) malloc(sizeof(double) * sig_qty_off);
	
	svm_on_prob.x = (struct svm_node**) malloc(sizeof(struct svm_node*) * sig_qty_on);
	svm_off_prob.x = (struct svm_node**) malloc(sizeof(struct svm_node*) * sig_qty_off);
	
	svm_node_pool = (struct svm_node*) malloc(((SVM_PARAM_QTY_ON + 1) * sig_qty_on + (SVM_PARAM_QTY_OFF + 1) * sig_qty_off) * sizeof(struct svm_node));
	
	node_pool_index = 0;
	svm_problem_index_on = svm_problem_index_off = 0;
	
	for(int i = 0; i < signature_qty; i++) {
		signature_to_features(&signatures[i], features);
		
		if(signatures[i].delta_pt > 0.0) {
			svm_on_prob.y[svm_problem_index_on] = (double) signatures[i].appliance_id;
			svm_on_prob.x[svm_problem_index_on] = &svm_node_pool[node_pool_index];
			
			svm_problem_index_on++;
			
			for(int j = 0; j < SVM_PARAM_QTY_ON; j++) {
				scaled = (features[j] - new_model->min_on[j]) / (new_model->max_on[j] - new_model->min_on[j]);
				
				if(isnormal(scaled)) {
					svm_node_pool[node_pool_index].index = j + 1;
					svm_node_pool[node_pool_index].value = scaled;
					
					node_pool_index++;
				}
			}
			
			svm_node_pool[node_pool_index++].index = -1;
		} else {
			svm_off_prob.y[svm_problem_index_off] = (double) signatures[i].appliance_id;
			svm_off_prob.x[svm_problem_index_off] = &svm_node_pool[node_pool_index];
			
			svm_problem_index_off++;
			
			for(int j = 0; j < SVM_PARAM_QTY_OFF; j++) {
				scaled = (features[j] - new_model->min_off[j]) / (new_model->max_off[j] - new_model->min_off[j]);
				
				if(isnormal(scaled)) {
					svm_node_pool[node_pool_index].index = j + 1;
					svm_node_pool[node_pool_index].value = scaled;
					
					node_pool_index++;
				}
			}
			
			svm_node_pool[node_pool_index++].index = -1;
		}
	}
	
	if((error_str = svm_check_parameter(&svm_on_prob, &svm_on_param))) {
		LOG_ERROR("SVM parameter error: %s", error_str);
		
		free(new_model);
		free(svm_on_prob.y);
		free(svm_off_prob.y);
		free(svm_on_prob.x);
		free(svm_off_prob.x);
		free(svm_node_pool);
		
		return NULL;
	}
	
	if((error_str = svm_check_parameter(&svm_off_prob, &svm_off_param))) {
		LOG_ERROR("SVM parameter error: %s", error_str);
		
		free(new_model);
		free(svm_on_prob.y);
		free(svm_off_prob.y);
		free(svm_on_prob.x);
		free(svm_off_prob.x);
		free(svm_node_pool);
		
		return NULL;
	}
	
	rate_on = do_cross_validation(&svm_on_prob, &svm_on_param, crossv_fold);
	rate_off = do_cross_validation(&svm_off_prob, &svm_off_param, crossv_fold);
	
	LOG_INFO("Initial rates: ON %.1lf %% --- OFF %.1lf %%", rate_on * 100.0, rate_off * 100.0);
	LOG_INFO("Starting parameter grid search.");
	
	rate_on = do_grid_search(&svm_on_prob, &svm_on_param, crossv_fold, C_exp_range, g_exp_range, &C_exp_best, &g_exp_best);
	svm_on_param.C = pow(2.0, C_exp_best);
	svm_on_param.gamma = pow(2.0, g_exp_best);
	
	rate_off = do_grid_search(&svm_off_prob, &svm_off_param, crossv_fold, C_exp_range, g_exp_range, &C_exp_best, &g_exp_best);
	svm_off_param.C = pow(2.0, C_exp_best);
	svm_off_param.gamma = pow(2.0, g_exp_best);
	
	if(rate_on < 0 || rate_off < 0) {
		LOG_ERROR("Grid search failed!");
		
		free(new_model);
		free(svm_on_prob.y);
		free(svm_off_prob.y);
		free(svm_on_prob.x);
		free(svm_off_prob.x);
		free(svm_node_pool);
		
		return NULL;
	}
	
	LOG_INFO("Grid search result: ON  C: %.3lf | gamma: %.3lf --- OFF C: %.3lf | gamma: %.3lf", svm_on_param.C, svm_on_param.gamma, rate_on * 100.0, svm_off_param.C, svm_off_param.gamma, rate_off * 100.0);
	
	LOG_INFO("Final rates: ON %.1lf %% --- OFF %.1lf %%", rate_on * 100.0, rate_off * 100.0);
	
	new_model->model_on = svm_train(&svm_on_prob, &svm_on_param);
	new_model->model_off = svm_train(&svm_off_prob, &svm_off_param);
	
	new_model->node_pool = svm_node_pool;
	
	free(svm_on_prob.y);
	free(svm_off_prob.y);
	free(svm_on_prob.x);
	free(svm_off_prob.x);
	
	new_model->nr_classes = svm_get_nr_class(new_model->model_on);
	
	if(new_model->nr_classes < 1 || new_model->nr_classes != svm_get_nr_class(new_model->model_off)) {
		free(new_model);
		free(svm_node_pool);
		
		return NULL;
	}
	
	return new_model;
}

static void sort_prob(int *labels, double *probs, int len) {
	double pivot;
	
	int i = 0;
	int j = len - 1;
	
	double aux_p;
	int aux_l;
	
	if(len < 2)
		return;
	
	pivot = probs[len / 2];
	
	while(1) {
		while (probs[i] > pivot)
			i++;
		
		while (probs[j] < pivot)
			j--;
		
		if(i >= j)
			break;
		
		aux_p = probs[i];
		aux_l = labels[i];
		
		probs[i] = probs[j];
		labels[i] = labels[j];
		
		probs[j] = aux_p;
		labels[j] = aux_l;
		
		i++;
		j--;
	}
	
	sort_prob(labels, probs, i);
	sort_prob(&labels[i], &probs[i], len - i);
}

int predict_event(const model_t *model, load_event_t *l_event) {
	struct svm_model *suitable_svm_model;
	int nr_classes;
	double features[SVM_PARAM_QTY_ON];
	struct svm_node nodes[10];
	int node_index;
	double scaled;
	int *class_labels = NULL;
	double *prob_classes = NULL;
	double avg_p;
	
	if(model == NULL || l_event == NULL)
		return -1;
	
	if(model->model_on == NULL || model->model_off == NULL)
		return -2;
	
	if(svm_check_probability_model(model->model_on) == 0 || svm_check_probability_model(model->model_off) == 0)
		return -4;
	
	suitable_svm_model = (l_event->delta_pt > 0.0) ? model->model_on : model->model_off;
	
	if((nr_classes = svm_get_nr_class(suitable_svm_model)) < 3) {
		return -3;
	}
	
	avg_p = 1.0 / (double) nr_classes;
	
	class_labels = (int*) malloc(nr_classes * sizeof(int));
	prob_classes = (double*) malloc(nr_classes * sizeof(double));
	
	if(class_labels == NULL || prob_classes == NULL) {
		free(class_labels);
		free(prob_classes);
		return -5;
	}
	
	load_event_to_features(l_event, features);
	
	node_index = 0;
	for(int i = 0; i < ((l_event->delta_pt > 0.0) ? SVM_PARAM_QTY_ON : SVM_PARAM_QTY_OFF); i++) {
		if(l_event->delta_pt > 0.0)
			scaled = (features[i] - model->min_on[i]) / (model->max_on[i] - model->min_on[i]);
		else
			scaled = (features[i] - model->min_off[i]) / (model->max_off[i] - model->min_off[i]);
		
		if(isnormal(scaled)) {
			nodes[node_index].index = i + 1;
			nodes[node_index].value = scaled;
			node_index++;
		}
	}
	
	nodes[node_index].index = -1;
	
	svm_predict_probability(suitable_svm_model, nodes, prob_classes);
	svm_get_labels(suitable_svm_model, class_labels);
	
	sort_prob(class_labels, prob_classes, nr_classes);
	
	l_event->appliance_ids[0] = class_labels[0];
	l_event->appliance_ids[1] = class_labels[1];
	l_event->appliance_ids[2] = class_labels[2];
	
	l_event->appliance_probs[0] = prob_classes[0];
	l_event->appliance_probs[1] = prob_classes[1];
	l_event->appliance_probs[2] = prob_classes[2];
	
	l_event->p_sd = 0.0;
	for(int i = 0; i < nr_classes; i++)
		l_event->p_sd += pow((prob_classes[i] - avg_p), 2);
	
	l_event->p_sd = sqrt(l_event->p_sd / (double) (nr_classes - 1)); // Desvio padrão das probabilidades
	
	free(class_labels);
	free(prob_classes);
	
	// Se existir uma diferença muito grande entre as probabilidades, remove os aparelhos que tem baixa probabilidade
	for(int i = 0; i < (3 - 1); i++)
		if(l_event->appliance_probs[i] > l_event->appliance_probs[i + 1] * 2)
			for(int j = i + 1; j < 3; j++) {
				l_event->appliance_ids[j] = 0;
				l_event->appliance_probs[j] = 0;
			}
	
	return 0;
}

void free_model_content(model_t *model) {
	if(model) {
		svm_free_and_destroy_model(&model->model_on);
		svm_free_and_destroy_model(&model->model_off);
		free(model->node_pool);
	}
}
