#include <time.h>

typedef struct power_data_s {
	time_t timestamp;
	double v[2];
	double i[2];
	double p[2];
	double s[2];
	double q[2];
} power_data_t;
