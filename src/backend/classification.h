#ifndef CLASSIFICATION_H
#define CLASSIFICATION_H

#include "disaggregation.h"

typedef struct model_s model_t;

model_t *train_model(const load_signature_t *signatures, int signature_qty);
int predict_event(const model_t *model, load_event_t *l_event);
void free_model_content(model_t *model);

#endif
