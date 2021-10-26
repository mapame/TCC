#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "common.h"
#include "logger.h"
#include "database.h"
#include "appliances.h"

int fetch_signatures(load_signature_t **sig_buffer_ptr, double min_power) {
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_get_signatures[] = "SELECT signatures.appliance_id,appliances.is_hardwired,delta_pt,peak_pt,duration,delta_pa,delta_pb,delta_qa,delta_qb,delta_sa,delta_sb FROM signatures INNER JOIN appliances ON signatures.appliance_id = appliances.id WHERE appliances.is_active = 1 AND ABS(signatures.delta_pt) >= ?1;";
	int buf_size = 16;
	load_signature_t *new_buf_ptr;
	int count = 0;
	
	if(sig_buffer_ptr == NULL)
		return -1;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -2;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_signatures, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -2;
	}
	
	if(sqlite3_bind_double(ppstmt, 1, min_power) != SQLITE_OK) {
		LOG_ERROR("Failed to bind value to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -2;
	}
	
	*sig_buffer_ptr = (load_signature_t*) malloc(buf_size * sizeof(load_signature_t));
	
	if(*sig_buffer_ptr == NULL) {
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -3;
	}
	
	do {
		result = sqlite3_step(ppstmt);
		
		if(result == SQLITE_ROW) {
			if(count + 2 > buf_size) {
				buf_size = buf_size * 2;
				
				new_buf_ptr = (load_signature_t*) realloc(*sig_buffer_ptr, buf_size * sizeof(load_signature_t));
				
				if(new_buf_ptr)
					*sig_buffer_ptr = new_buf_ptr;
				else
					break;
			}
			
			(*sig_buffer_ptr)[count].appliance_id = sqlite3_column_int(ppstmt, 0);
			
			(*sig_buffer_ptr)[count].delta_pt = sqlite3_column_double(ppstmt, 2);
			(*sig_buffer_ptr)[count].peak_pt = sqlite3_column_double(ppstmt, 3);
			(*sig_buffer_ptr)[count].duration = sqlite3_column_double(ppstmt, 4);
			(*sig_buffer_ptr)[count].delta_p[0] = sqlite3_column_double(ppstmt, 5);
			(*sig_buffer_ptr)[count].delta_p[1] = sqlite3_column_double(ppstmt, 6);
			(*sig_buffer_ptr)[count].delta_q[0] = sqlite3_column_double(ppstmt, 7);
			(*sig_buffer_ptr)[count].delta_q[1] = sqlite3_column_double(ppstmt, 8);
			(*sig_buffer_ptr)[count].delta_s[0] = sqlite3_column_double(ppstmt, 9);
			(*sig_buffer_ptr)[count].delta_s[1] = sqlite3_column_double(ppstmt, 10);
			
			count++;
			
			// Verifica o parametro "is_hardwired", se a carga não for fixa, a assinatura é duplicada com as fases trocadas.
			if(sqlite3_column_int(ppstmt, 1) == 0) {
				(*sig_buffer_ptr)[count].appliance_id = sqlite3_column_int(ppstmt, 0);
				
				(*sig_buffer_ptr)[count].delta_pt = (*sig_buffer_ptr)[count - 1].delta_pt;
				(*sig_buffer_ptr)[count].peak_pt = (*sig_buffer_ptr)[count - 1].peak_pt;
				(*sig_buffer_ptr)[count].duration = (*sig_buffer_ptr)[count - 1].duration;
				(*sig_buffer_ptr)[count].delta_p[0] = (*sig_buffer_ptr)[count - 1].delta_p[1];
				(*sig_buffer_ptr)[count].delta_p[1] = (*sig_buffer_ptr)[count - 1].delta_p[0];
				(*sig_buffer_ptr)[count].delta_q[0] = (*sig_buffer_ptr)[count - 1].delta_q[1];
				(*sig_buffer_ptr)[count].delta_q[1] = (*sig_buffer_ptr)[count - 1].delta_q[0];
				(*sig_buffer_ptr)[count].delta_s[0] = (*sig_buffer_ptr)[count - 1].delta_s[1];
				(*sig_buffer_ptr)[count].delta_s[1] = (*sig_buffer_ptr)[count - 1].delta_s[0];
				
				count++;
			}
		}
		
	} while(result == SQLITE_ROW);
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to fetch appliance signatures for training: %s", sqlite3_errstr(result));
		
		free(*sig_buffer_ptr);
		*sig_buffer_ptr = NULL;
		
		return -2;
	}
	
	if(count == 0) {
		free(*sig_buffer_ptr);
		*sig_buffer_ptr = NULL;
	}
	
	return count;
}

time_t get_last_signature_modification() {
	time_t last_modification_time = 0;
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_get_signatures[] = "SELECT MAX(modification_date) AS last_modification FROM appliances;";
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -2;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_signatures, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -2;
	}
	
	result = sqlite3_step(ppstmt);
	
	if(result == SQLITE_ROW) {
		last_modification_time = sqlite3_column_int64(ppstmt, 0);
		
		result = sqlite3_step(ppstmt);
	}
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to fetch the appliancec/signature last modification date from database.");
		
		return -2;
	}
	
	return last_modification_time;
}

int get_appliances_max_time_on(int **result_output) {
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_get_max_time_on[] = "SELECT id,max_time_on FROM appliances ORDER BY id DESC;";
	int last_appliance_id = 0;
	int buffer_size = 16;
	
	if(!result_output)
		return -1;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -2;
	}

	sqlite3_busy_timeout(db_conn, 1000);

	if((result = sqlite3_prepare_v2(db_conn, sql_get_max_time_on, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -2;
	}
	
	*result_output = (int*) malloc(buffer_size * sizeof(int));
	
	while((result = sqlite3_step(ppstmt)) == SQLITE_ROW) {
		int appliance_id = sqlite3_column_int(ppstmt, 0);
		
		if(appliance_id >= buffer_size) {
			int *new_buffer;
			
			buffer_size = appliance_id + 1;
			
			new_buffer = (int*) realloc(*result_output, buffer_size * sizeof(int));
			
			if(!new_buffer) {
				free(*result_output);
				sqlite3_finalize(ppstmt);
				sqlite3_close(db_conn);
				
				return -3;
			}
			
			*result_output = new_buffer;
		}
		
		(*result_output)[appliance_id] = sqlite3_column_int(ppstmt, 1);
		
		if(appliance_id > last_appliance_id)
			last_appliance_id = appliance_id;
	}
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to read appliances max_time_on from database: %s", sqlite3_errstr(result));
		
		return -2;
	}
	
	return last_appliance_id;
}
