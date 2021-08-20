#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "common.h"
#include "logger.h"
#include "config.h"
#include "power.h"
#include "database.h"

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
	time_t timestamp_minute;
	struct tm time_tm;
	int year, month, day, hour;
	double p_total;
	double active_energy_total;
	double reactive_energy_total;
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
	
	cost = config_get_value_double("kwh_rate", 0, 10, 0) * active_energy_total;
	
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
	
	if(sqlite3_exec(db_conn, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
		LOG_ERROR("Failed to commit power data to database: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -2;
	}
	
	sqlite3_close(db_conn);
	
	return 0;
}

static int store_disaggregated_minute_energy(sqlite3 *db_conn, int appliance_id, double energy_rate, time_t start_timestamp, time_t end_timestamp, double start_dp, double end_dp) {
	int result;
	const char sql_store_minute[] = "INSERT INTO disaggregated_energy_minutes(timestamp,appliance_id,active,cost,second_count) VALUES(?1,?2,?3,?4,?5)"
									" ON CONFLICT(timestamp,appliance_id) DO UPDATE SET second_count = second_count + excluded.second_count, active = active + excluded.active, cost = cost + excluded.cost;";
	sqlite3_stmt *ppstmt = NULL;
	int minute_count = 0;
	
	double dp = MIN(start_dp, end_dp);
	
	time_t step_start_timestamp, step_end_timestamp;
	double step_energy, step_cost;
	
	if((result = sqlite3_prepare_v2(db_conn, sql_store_minute, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare the SQL statement for minute disaggregated energy storage: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	if(sqlite3_bind_int(ppstmt, 2, appliance_id) != SQLITE_OK) {
		LOG_ERROR("Failed to bind appliance_id to minute disaggregated energy prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	step_start_timestamp = start_timestamp;
	step_end_timestamp = start_timestamp - (start_timestamp % 60) + 59;
	
	if(step_end_timestamp > end_timestamp)
		step_end_timestamp = end_timestamp;
	
	while(step_start_timestamp < end_timestamp) {
		step_energy = (step_end_timestamp - step_start_timestamp) * dp / (3600.0 * 1000.0);
		step_cost = step_energy * energy_rate;
		
		// SQLITE_OK é zero, então somando todos os resultados podemos saber se algum falhou
		result = sqlite3_bind_int64(ppstmt, 1, (step_start_timestamp - step_start_timestamp % 60));
		result += sqlite3_bind_double(ppstmt, 3, step_energy);
		result += sqlite3_bind_double(ppstmt, 4, step_cost);
		result += sqlite3_bind_int(ppstmt, 5, (step_end_timestamp - step_start_timestamp));
		
		if(result) {
			LOG_ERROR("Failed to bind value to prepared statement.");
			sqlite3_finalize(ppstmt);
			sqlite3_close(db_conn);
			
			return -1;
		}
		
		if((result = sqlite3_step(ppstmt)) != SQLITE_DONE) {
			LOG_ERROR("Failed to store minute disaggregated energy data: %s", sqlite3_errstr(result));
			sqlite3_finalize(ppstmt);
			sqlite3_close(db_conn);
			
			return -1;
		}
		
		sqlite3_reset(ppstmt);
		
		step_start_timestamp += 60 - step_start_timestamp % 60;
		step_end_timestamp += 60;
		
		if(step_end_timestamp > end_timestamp)
			step_end_timestamp = end_timestamp;
		
		minute_count++;
	}
	
	sqlite3_finalize(ppstmt);
	
	return minute_count;
}

static int store_disaggregated_hour_energy(sqlite3 *db_conn, int appliance_id, double energy_rate, time_t start_timestamp, time_t end_timestamp, double start_dp, double end_dp) {
	int result;
	const char sql_store_hour[] = "INSERT INTO disaggregated_energy_hours(year,month,day,hour,appliance_id,active,cost,second_count) VALUES(?1,?2,?3,?4,?5,?6,?7,?8)"
									" ON CONFLICT(year,month,day,hour,appliance_id) DO UPDATE SET second_count = second_count + excluded.second_count, active = active + excluded.active, cost = cost + excluded.cost;";
	sqlite3_stmt *ppstmt = NULL;
	int hour_count = 0;
	
	double dp = MIN(start_dp, end_dp);
	
	struct tm date;
	time_t step_start_timestamp, step_end_timestamp;
	double step_energy, step_cost;
	
	if((result = sqlite3_prepare_v2(db_conn, sql_store_hour, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare the SQL statement for hour disaggregated energy storage: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	if(sqlite3_bind_int(ppstmt, 5, appliance_id) != SQLITE_OK) {
		LOG_ERROR("Failed to bind appliance_id to hour disaggregated energy prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	step_start_timestamp = start_timestamp;
	step_end_timestamp = start_timestamp - (start_timestamp % 3600) + 3599; // Não funciona com fusos horários fracionados
	
	if(step_end_timestamp > end_timestamp)
		step_end_timestamp = end_timestamp;
	
	while(step_start_timestamp < end_timestamp) {
		step_energy = (step_end_timestamp - step_start_timestamp) * dp / (3600.0 * 1000.0);
		step_cost = step_energy * energy_rate;
		
		localtime_r(&step_start_timestamp, &date);
		
		// SQLITE_OK é zero, então somando todos os resultados podemos saber se algum falhou
		result = sqlite3_bind_int(ppstmt, 1, date.tm_year + 1900);
		result += sqlite3_bind_int(ppstmt, 2, date.tm_mon + 1);
		result += sqlite3_bind_int(ppstmt, 3, date.tm_mday);
		result += sqlite3_bind_int(ppstmt, 4, date.tm_hour);
		result += sqlite3_bind_double(ppstmt, 6, step_energy);
		result += sqlite3_bind_double(ppstmt, 7, step_cost);
		result += sqlite3_bind_int(ppstmt, 8, (step_end_timestamp - step_start_timestamp));
		
		if(result) {
			LOG_ERROR("Failed to bind value to prepared statement.");
			sqlite3_finalize(ppstmt);
			sqlite3_close(db_conn);
			
			return -1;
		}
		
		if((result = sqlite3_step(ppstmt)) != SQLITE_DONE) {
			LOG_ERROR("Failed to store hour disaggregated energy data: %s", sqlite3_errstr(result));
			sqlite3_finalize(ppstmt);
			sqlite3_close(db_conn);
			
			return -1;
		}
		
		sqlite3_reset(ppstmt);
		
		step_start_timestamp += 3600 - step_start_timestamp % 3600;
		step_end_timestamp += 3600;
		
		if(step_end_timestamp > end_timestamp)
			step_end_timestamp = end_timestamp;
		
		hour_count++;
	}
	
	sqlite3_finalize(ppstmt);
	
	return hour_count;
}

int energy_add_power_disaggregated(int appliance_id, time_t start_timestamp, time_t end_timestamp, double start_dp, double end_dp) {
	int result;
	sqlite3 *db_conn = NULL;
	
	double energy_rate;
	
	if(start_timestamp > end_timestamp)
		return -3;
	
	energy_rate = config_get_value_double("kwh_rate", 0, 10, 0);
	
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
	
	result = store_disaggregated_minute_energy(db_conn, appliance_id, energy_rate, start_timestamp, end_timestamp, start_dp, end_dp);
	
	if(result <= 0) {
		LOG_ERROR("Failed to store minute disaggregated energy.");
		sqlite3_close(db_conn);
		
		return -2;
	} else {
		LOG_DEBUG("Storing %d minutes of diseggregated energy to database.", result);
	}
	
	store_disaggregated_hour_energy(db_conn, appliance_id, energy_rate, start_timestamp, end_timestamp, start_dp, end_dp);
	
	if(result <= 0) {
		LOG_ERROR("Failed to store hour disaggregated energy.");
		sqlite3_close(db_conn);
		
		return -2;
	} else {
		LOG_DEBUG("Storing %d hours of diseggregated energy to database.", result);
	}
	
	
	if(sqlite3_exec(db_conn, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
		LOG_ERROR("Failed to commit disaggregated energy data to database: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -2;
	}
	
	sqlite3_close(db_conn);
	
	return 0;
}
