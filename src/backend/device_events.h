#ifndef METER_EVENTS_H
#define METER_EVENTS_H

#include <time.h>

int store_meter_event_db(time_t timestamp, const char *description);

#endif
