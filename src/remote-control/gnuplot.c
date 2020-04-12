#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "gnuplot.h"

const char * gnuplot_valid_styles[] = {"lines", "points", "linespoints", "dots", "impulses"};

gnuplot_ctrl * gnuplot_init(void) {
	gnuplot_ctrl * handle;
	
	handle = (gnuplot_ctrl*)malloc(sizeof(gnuplot_ctrl));
	handle->num_plots = 0;
	
	for(int i = 0; i < GNUPLOT_MAX_PLOTS; i++)
		handle->plot_tmp_filename_list[i] = NULL;
	
	handle->process_pipe = popen("gnuplot -p", "w");
	if (handle->process_pipe == NULL) {
		fprintf(stderr, "Error starting gnuplot process.\n");
		free(handle);
		return NULL ;
	}
	
	return handle;
}

void gnuplot_close(gnuplot_ctrl * handle) {
	if(handle == NULL) 
		return;
	
	if (pclose(handle->process_pipe) == -1) {
		fprintf(stderr, "Unable to close gnuplot process.\n");
		return;
	}
	
	for(int i = 0; i < GNUPLOT_MAX_PLOTS; i++) {
		if(handle->plot_tmp_filename_list[i] != NULL) {
			remove(handle->plot_tmp_filename_list[i]);
			free(handle->plot_tmp_filename_list[i]);
			handle->plot_tmp_filename_list[i] = NULL;
		}
	}
	
	free(handle);
}

int gnuplot_plot_data(gnuplot_ctrl * handle, const float * x, const float * y, const int n, const char * title, const char * axes, const char * style) {
	if(handle == NULL)
		return -1;
	
	if(handle->num_plots == GNUPLOT_MAX_PLOTS - 1) {
		fprintf(stderr, "Maximum number of plots reached.\n");
		return -1;
	}
	
	char * filename = NULL;
	
	if(handle->plot_tmp_filename_list[handle->num_plots] != NULL) {
		filename = handle->plot_tmp_filename_list[handle->num_plots];
	} else {
		filename = gnuplot_create_tmp_file();
		if(filename == NULL)
			return -1;
		
		handle->plot_tmp_filename_list[handle->num_plots] = filename;
	}
	
	FILE * fd = fopen(filename, "w");
	
	fprintf(fd, "# %s\n", title);
	
	for(int i = 0; i < n; i++) {
		fprintf(fd, "%f", y[i]);
		if(x != NULL)
			fprintf(fd, " %f", x[i]);
		fprintf(fd, "\n");
	}
	
	fclose(fd);
	
	char command[64];
	
	sprintf(command, "%s \"%s\" title \"%s\"", (handle->num_plots == 0) ? "plot" : "replot", filename, title);
	
	if(axes != NULL) {
		strcat(command, " axes ");
		strcat(command, axes);
	}
	
	if(style != NULL) {
		strcat(command, " with ");
		strcat(command, style);
	}
	
	strcat(command, "\n");
	
	gnuplot_command(handle, command);
	
	return handle->num_plots++;
}

int gnuplot_append_data(gnuplot_ctrl * handle, const int plot_number, const float * x, const float * y, const int n) {
	if(handle == NULL) 
		return -1;
	
	if(plot_number >= GNUPLOT_MAX_PLOTS - 1)
		return -1;
	
	if(handle->plot_tmp_filename_list[plot_number] == NULL)
		return -1;
	
	FILE * fd = fopen(handle->plot_tmp_filename_list[plot_number], "a");
	
	for(int i = 0; i < n; i++) {
		fprintf(fd, "%f", y[i]);
		if(x != NULL)
			fprintf(fd, " %f", x[i]);
		fprintf(fd, "\n");
	}
	
	fclose(fd);
	
	return plot_number;
}

void gnuplot_replot(gnuplot_ctrl * handle) {
	gnuplot_command(handle, "replot");
}

void gnuplot_command(gnuplot_ctrl * handle, const char * command) {
	fprintf(handle->process_pipe, "%s\n", command);
	fflush(handle->process_pipe);
}

void gnuplot_clear(gnuplot_ctrl * handle) {
	if(handle == NULL) 
		return;
	
	handle->num_plots = 0;
}

char * gnuplot_create_tmp_file() {
	static const char tmp_filename_template[] = "/tmp/gnuplot_data_XXXXXX";
	
	char * tmp_filename = NULL;
	
	tmp_filename = (char*) malloc(sizeof(tmp_filename_template));
	
	strcpy(tmp_filename, tmp_filename_template);
	
	int fd = mkstemp(tmp_filename);
	if(fd == -1) {
		fprintf(stderr, "Failed to create temporary file.\n");
		return NULL;
	}
	close(fd);
	
	return tmp_filename;
}
