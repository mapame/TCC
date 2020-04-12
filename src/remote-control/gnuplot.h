#define GNUPLOT_MAX_PLOTS 8

typedef struct _gnuplot_ctrl_ {
	FILE * process_pipe;
	int num_plots;
	char * plot_tmp_filename_list[GNUPLOT_MAX_PLOTS];
} gnuplot_ctrl;

gnuplot_ctrl * gnuplot_init(void);

void gnuplot_close(gnuplot_ctrl * handle);

void gnuplot_command(gnuplot_ctrl * handle, const char * command);

void gnuplot_replot(gnuplot_ctrl * handle);

int gnuplot_plot_data(gnuplot_ctrl * handle, const float * x, const float * y, const int n, const char * title, const char * axes, const char * style);

int gnuplot_append_data(gnuplot_ctrl * handle, const int plot_number, const float * x, const float * y, const int n);

void gnuplot_clear(gnuplot_ctrl * handle);

char * gnuplot_create_tmp_file();
