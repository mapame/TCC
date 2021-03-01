#ifndef DEVICE_EVENTS_H
#define DEVICE_EVENTS_H

#include <time.h>

int store_device_event_db(time_t timestamp, const char *description);

#endif
