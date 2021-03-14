#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "common.h"
#include "logger.h"
#include "power.h"
#include "database.h"

int energy_get_timestamp_rate(time_t timestamp, double *rate_ptr) {
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_get_energy_rate[] = "SELECT rate,peak_rate,mid_rate,peak_start,peak_end,mid_start,mid_end FROM energy_rates WHERE start_timestamp <= ?1 ORDER BY start_timestamp DESC LIMIT 1;";
	int found = 0;
	int rate_is_tou = 0;
	double rate, peak_rate, mid_rate;
	int peak_start_dmin, peak_end_dmin, mid_start_dmin, mid_end_dmin;
	
	if(rate_ptr == NULL)
		return -1;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -2;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_energy_rate, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -2;
	}
	
	if(sqlite3_bind_int64(ppstmt, 1, timestamp) != SQLITE_OK) {
		LOG_ERROR("Failed to bind value to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -2;
	}
	
	if((result = sqlite3_step(ppstmt)) == SQLITE_ROW) {
		found = 1;
		
		rate = sqlite3_column_double(ppstmt, 0);
		
		rate_is_tou = 1;
		for(int icol = 2; icol <= 7; icol++)
			if(sqlite3_column_type(ppstmt, icol) == SQLITE_NULL) {
				rate_is_tou = 0;
				break;
			}
		
		if(rate_is_tou) {
			peak_rate = sqlite3_column_double(ppstmt, 1);
			mid_rate = sqlite3_column_double(ppstmt, 2);
			peak_start_dmin = sqlite3_column_int(ppstmt, 3);
			peak_end_dmin = sqlite3_column_int(ppstmt, 4);
			mid_start_dmin = sqlite3_column_int(ppstmt, 5);
			mid_end_dmin = sqlite3_column_int(ppstmt, 6);
		}
		
		result = sqlite3_step(ppstmt);
	}
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to read energy rate: %s", sqlite3_errstr(result));
		return -2;
	}
	
	if(!found)
		return 2;
	
	*rate_ptr = rate;
	
	if(rate_is_tou) {
		struct tm time_tm;
		
		localtime_r(&timestamp, &time_tm);
		mktime(&time_tm);
		
		if(time_tm.tm_wday != 0 && time_tm.tm_wday != 6) {
			int day_minute = time_tm.tm_hour * 60 + time_tm.tm_min;
			
			if(day_minute >= peak_start_dmin && day_minute <= peak_end_dmin)
				*rate_ptr = peak_rate;
			else if(day_minute >= mid_start_dmin && day_minute <= mid_end_dmin)
				*rate_ptr = mid_rate;
		}
	}
	
	return 0;
}

int energy_add_power(power_data_t *pd) {
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_store_minute[] = "INSERT INTO energy_minutes(timestamp,latest_second,active,reactive,min_p,cost) VALUES(?1,?2,?3,?4,?5,?6)"
									" ON CONFLICT(timestamp) DO UPDATE SET second_count = second_count + 1, latest_second = excluded.latest_second, active = active + excluded.active, reactive = reactive + excluded.reactive, min_p = min(min_p, excluded.min_p), cost = cost + excluded.cost WHERE latest_second < excluded.latest_second;";
	const char sql_store_hour[] = "INSERT INTO energy_hours(year,month,day,hour,active,reactive,min_p,cost) VALUES(?1,?2,?3,?4,?5,?6,?7,?8)"
									" ON CONFLICT(year,month,day,hour) DO UPDATE SET second_count = second_count + 1, active = active + excluded.active, reactive = reactive + excluded.reactive, min_p = min(min_p, excluded.min_p), cost = cost + excluded.cost;";
	const char sql_store_day[] = "INSERT INTO energy_days(year,month,day,active,reactive,min_p,cost) VALUES(?1,?2,?3,?4,?5,?6,?7)"
									" ON CONFLICT(year,month,day) DO UPDATE SET second_count = second_count + 1, active = active + excluded.active, reactive = reactive + excluded.reactive, min_p = min(min_p, excluded.min_p), cost = cost + excluded.cost;";
	const char sql_store_month[] = "INSERT INTO energy_months(year,month,active,reactive,min_p,cost) VALUES(?1,?2,?3,?4,?5,?6)"
									" ON CONFLICT(year,month) DO UPDATE SET second_count = second_count + 1, active = active + excluded.active, reactive = reactive + excluded.reactive, min_p = min(min_p, excluded.min_p), cost = cost + excluded.cost;";
	time_t timestamp_minute;
	struct tm time_tm;
	int year, month, day, hour;
	double p_total;
	double active_energy_total;
	double reactive_energy_total;
	double e_rate = 0.0;
	double cost;
	
	if(pd == NULL)
		return -1;
	
	timestamp_minute = pd->timestamp - (pd->timestamp % 60);
	localtime_r(&(pd->timestamp), &time_tm);
	year = time_tm.tm_year + 1900;
	month = time_tm.tm_mon + 1;
	day = time_tm.tm_mday;
	hour = time_tm.tm_hour;
	
	p_total = pd->p[0] + pd->p[1];
	// Energia ativa em KWh e reativa em kvarh
	active_energy_total = p_total / (3600.0 * 1000.0);
	reactive_energy_total = (pd->q[0] + pd->q[1]) / (3600.0 * 1000.0);
	
	if(energy_get_timestamp_rate(pd->timestamp, &e_rate) < 0)
		return -1;
	
	cost = e_rate * active_energy_total;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if(sqlite3_exec(db_conn, "BEGIN TRANSACTION", NULL, NULL, NULL) != SQLITE_OK) {
		LOG_ERROR("Failed to begin SQL transaction: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	/*
	 * Minuto
	 */
	if((result = sqlite3_prepare_v2(db_conn, sql_store_minute, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare the SQL statement for minute power data storage: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	// SQLITE_OK é zero, então somando todos os resultados podemos saber se algum falhou
	result = sqlite3_bind_int64(ppstmt, 1, timestamp_minute);
	result += sqlite3_bind_int64(ppstmt, 2, pd->timestamp);
	result += sqlite3_bind_double(ppstmt, 3, active_energy_total);
	result += sqlite3_bind_double(ppstmt, 4, reactive_energy_total);
	result += sqlite3_bind_double(ppstmt, 5, p_total);
	result += sqlite3_bind_double(ppstmt, 6, cost);
	
	if(result) {
		LOG_ERROR("Failed to bind value to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	result = sqlite3_step(ppstmt);
	
	sqlite3_finalize(ppstmt);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to store power data as minute: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	/*
	 * Hora
	 */
	if((result = sqlite3_prepare_v2(db_conn, sql_store_hour, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare the SQL statement for hour power data storage: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	// SQLITE_OK é zero, então somando todos os resultados podemos saber se algum falhou
	result = sqlite3_bind_int(ppstmt, 1, year);
	result += sqlite3_bind_int(ppstmt, 2, month);
	result += sqlite3_bind_int(ppstmt, 3, day);
	result += sqlite3_bind_int(ppstmt, 4, hour);
	result += sqlite3_bind_double(ppstmt, 5, active_energy_total);
	result += sqlite3_bind_double(ppstmt, 6, reactive_energy_total);
	result += sqlite3_bind_double(ppstmt, 7, p_total);
	result += sqlite3_bind_double(ppstmt, 8, cost);
	
	if(result) {
		LOG_ERROR("Failed to bind value to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	result = sqlite3_step(ppstmt);
	
	sqlite3_finalize(ppstmt);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to store power data as hour: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	/*
	 * Dia
	 */
	if((result = sqlite3_prepare_v2(db_conn, sql_store_day, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare the SQL statement for day power data storage: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	// SQLITE_OK é zero, então somando todos os resultados podemos saber se algum falhou
	result = sqlite3_bind_int(ppstmt, 1, year);
	result += sqlite3_bind_int(ppstmt, 2, month);
	result += sqlite3_bind_int(ppstmt, 3, day);
	result += sqlite3_bind_double(ppstmt, 4, active_energy_total);
	result += sqlite3_bind_double(ppstmt, 5, reactive_energy_total);
	result += sqlite3_bind_double(ppstmt, 6, p_total);
	result += sqlite3_bind_double(ppstmt, 7, cost);
	
	if(result) {
		LOG_ERROR("Failed to bind value to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	result = sqlite3_step(ppstmt);
	
	sqlite3_finalize(ppstmt);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to store power data as day: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	/*
	 * Mês
	 */
	if((result = sqlite3_prepare_v2(db_conn, sql_store_month, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare the SQL statement for month power data storage: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	// SQLITE_OK é zero, então somando todos os resultados podemos saber se algum falhou
	result = sqlite3_bind_int(ppstmt, 1, year);
	result += sqlite3_bind_int(ppstmt, 2, month);
	result += sqlite3_bind_double(ppstmt, 3, active_energy_total);
	result += sqlite3_bind_double(ppstmt, 4, reactive_energy_total);
	result += sqlite3_bind_double(ppstmt, 5, p_total);
	result += sqlite3_bind_double(ppstmt, 6, cost);
	
	if(result) {
		LOG_ERROR("Failed to bind value to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	result = sqlite3_step(ppstmt);
	
	sqlite3_finalize(ppstmt);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to store power data as month: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	if(sqlite3_exec(db_conn, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
		LOG_ERROR("Failed to commit power data to database: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -2;
	}
	
	sqlite3_close(db_conn);
	
	return 0;
}
