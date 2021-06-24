#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "logger.h"
#include "database.h"

int store_meter_event_db(time_t timestamp, const char *description) {
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_store_event[] = "INSERT INTO meter_events(timestamp,description) VALUES(?1,?2);";
	
	if(description == NULL)
		return -1;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_store_event, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare the SQL statement for meter event storage: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	result = sqlite3_bind_int64(ppstmt, 1, timestamp);
	result += sqlite3_bind_text(ppstmt, 2, description, -1, SQLITE_STATIC);
	
	if(result) {
		LOG_ERROR("Failed to bind value to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	result = sqlite3_step(ppstmt);
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to store meter event in database: %s", sqlite3_errstr(result));
		
		return -1;
	}
	
	return 0;
}
