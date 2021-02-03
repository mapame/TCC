#include <time.h>

typedef struct power_data_s {
	time_t timestamp;
	double v[3];
	double i[3];
	double p[3];
	double s[3];
	double q[3];
} power_data_t;
