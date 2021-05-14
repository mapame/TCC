#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "database.h"
#include "logger.h"
#include "auth.h"
#include "users.h"


int users_get_id_by_username(const char *username) {
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_get_user_id[] = "SELECT id FROM users WHERE name=?1;";
	int user_id = 0;
	
	if(username == NULL || strlen(username) < 1)
		return 0;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_user_id, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	if((result = sqlite3_bind_text(ppstmt, 1, username, -1, SQLITE_STATIC)) != SQLITE_OK) {
		LOG_ERROR("Failed to bind value to prepared statement: %s", sqlite3_errstr(result));
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	if((result = sqlite3_step(ppstmt)) == SQLITE_ROW) {
		user_id = sqlite3_column_int(ppstmt, 0);
		
		result = sqlite3_step(ppstmt);
	}
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to find user ID: %s", sqlite3_errstr(result));
		
		return -1;
	}
	
	return user_id;
}

int users_check_active(int user_id) {
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_get_user[] = "SELECT is_active FROM users WHERE id=?1;";
	int is_active = 0;
	
	if(user_id <= 0)
		return -2;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_user, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	if((result = sqlite3_bind_int(ppstmt, 1, user_id)) != SQLITE_OK) {
		LOG_ERROR("Failed to bind value to prepared statement: %s", sqlite3_errstr(result));
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	if((result = sqlite3_step(ppstmt)) == SQLITE_ROW) {
		
		is_active = sqlite3_column_int(ppstmt, 0);
		
		result = sqlite3_step(ppstmt);
	}
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to read user: %s", sqlite3_errstr(result));
		return -1;
	}
	
	return is_active;
}

int users_check_admin(int user_id) {
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_get_user[] = "SELECT is_admin FROM users WHERE id=?1;";
	int is_admin = 0;
	
	if(user_id <= 0)
		return -2;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_user, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	if((result = sqlite3_bind_int(ppstmt, 1, user_id)) != SQLITE_OK) {
		LOG_ERROR("Failed to bind value to prepared statement: %s", sqlite3_errstr(result));
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	if((result = sqlite3_step(ppstmt)) == SQLITE_ROW) {
		
		is_admin = sqlite3_column_int(ppstmt, 0);
		
		result = sqlite3_step(ppstmt);
	}
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to read user: %s", sqlite3_errstr(result));
		return -1;
	}
	
	return is_admin;
}

int users_get_list(user_t **user_list_ptr, int filter_inactive) {
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_get_users[] = "SELECT id,name,is_active,is_admin,creation_date,modification_date FROM users WHERE ?1 OR is_active=1;";
	const char *str_ptr;
	int size = 8, count = 0;
	
	if(user_list_ptr == NULL)
		return 0;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_users, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	if((result = sqlite3_bind_int(ppstmt, 1, (filter_inactive) ? 0 : 1)) != SQLITE_OK) {
		LOG_ERROR("Failed to bind value to prepared statement: %s", sqlite3_errstr(result));
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	*user_list_ptr = (user_t*) malloc(sizeof(user_t) * size);
	
	if(*user_list_ptr == NULL) {
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -2;
	}
	
	while((result = sqlite3_step(ppstmt)) == SQLITE_ROW) {
		if(count >= size) {
			user_t *tmp_ptr;
			
			size *= 2;
			
			if((tmp_ptr = (user_t*) realloc(*user_list_ptr, sizeof(user_t) * size)))
				*user_list_ptr = tmp_ptr;
			else
				break;
		}
		
		if((str_ptr = (const char*) sqlite3_column_text(ppstmt, 1)) == NULL)
			break;
		
		(*user_list_ptr)[count].name = strdup(str_ptr);
		
		(*user_list_ptr)[count].id = sqlite3_column_int(ppstmt, 0);
		(*user_list_ptr)[count].is_active = sqlite3_column_int(ppstmt, 2);
		(*user_list_ptr)[count].is_admin = sqlite3_column_int(ppstmt,3);
		(*user_list_ptr)[count].creation_date = sqlite3_column_int64(ppstmt, 4);
		(*user_list_ptr)[count].modification_date = sqlite3_column_int64(ppstmt, 5);
		
		count++;
	}
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to read users: %s", sqlite3_errstr(result));
		
		for(int pos = 0; pos < count; pos++)
			free((*user_list_ptr)[count].name);
		
		free(*user_list_ptr);
		
		return -1;
	}
	
	return count;
}

int users_get(int user_id, user_t *user_ptr) {
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_get_user[] = "SELECT name,is_active,is_admin,creation_date,modification_date FROM users WHERE id=?1;";
	const char *str_ptr = NULL;
	int found = 0;
	
	if(user_id <= 0)
		return -2;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_get_user, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	if((result = sqlite3_bind_int(ppstmt, 1, user_id)) != SQLITE_OK) {
		LOG_ERROR("Failed to bind value to prepared statement: %s", sqlite3_errstr(result));
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	if((result = sqlite3_step(ppstmt)) == SQLITE_ROW) {
		found = 1;
		
		if(user_ptr) {
			if((str_ptr = (const char*) sqlite3_column_text(ppstmt, 0)) == NULL) {
				sqlite3_finalize(ppstmt);
				sqlite3_close(db_conn);
				
				return -1;
			}
			
			user_ptr->name = strdup(str_ptr);
			
			user_ptr->id = user_id;
			user_ptr->is_active = sqlite3_column_int(ppstmt, 1);
			user_ptr->is_admin = sqlite3_column_int(ppstmt, 2);
			user_ptr->creation_date = sqlite3_column_int64(ppstmt, 3);
			user_ptr->modification_date = sqlite3_column_int64(ppstmt, 4);
		}
		
		result = sqlite3_step(ppstmt);
	}
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to read user: %s", sqlite3_errstr(result));
		
		free(user_ptr->name);
		
		return -1;
	}
	
	return (found) ? user_id : 0;
}

int users_create(const user_t *user, const char *password) {
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_create_user[] = "INSERT INTO users(name,is_active,is_admin,password_hash,password_salt,creation_date,modification_date) VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?6);";
	char *salt, *hash;
	int user_id;
	
	if(user == NULL || user->name == NULL || password == NULL)
		return 0;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_create_user, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement for inserting new user: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	salt = auth_new_salt();
	
	if((hash = auth_hash_password(salt, password)) == NULL) {
		LOG_ERROR("Failed to calculate password hash for new user.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -2;
	}
	
	// SQLITE_OK é zero, então somando todos os resultados podemos saber se algum falhou
	result = sqlite3_bind_text(ppstmt, 1, user->name, -1, SQLITE_STATIC);
	result += sqlite3_bind_int(ppstmt, 2, user->is_active);
	result += sqlite3_bind_int(ppstmt, 3, user->is_admin);
	result += sqlite3_bind_text(ppstmt, 4, hash, -1, SQLITE_TRANSIENT);
	result += sqlite3_bind_text(ppstmt, 5, salt, -1, SQLITE_TRANSIENT);
	result += sqlite3_bind_int64(ppstmt, 6, time(NULL));
	
	free(salt);
	free(hash);
	
	if(result) {
		LOG_ERROR("Failed to bind values to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	result = sqlite3_step(ppstmt);
	
	sqlite3_finalize(ppstmt);
	
	user_id = sqlite3_last_insert_rowid(db_conn);
	
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to insert new user: %s", sqlite3_errstr(result));
		
		return -1;
	}
	
	return user_id;
}

int users_update(const user_t *user) {
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_update_user[] = "UPDATE users SET (name,is_active,is_admin,modification_date) = (?2, ?3, ?4, ?5) WHERE id = ?1;";
	int changes = 0;
	
	if(user == NULL || user->name == NULL)
		return 0;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_update_user, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement for user update: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	// SQLITE_OK é zero, então somando todos os resultados podemos saber se algum falhou
	result = sqlite3_bind_int(ppstmt, 1, user->id);
	result += sqlite3_bind_text(ppstmt, 2, user->name, -1, SQLITE_STATIC);
	result += sqlite3_bind_int(ppstmt, 3, user->is_active);
	result += sqlite3_bind_int(ppstmt, 4, user->is_admin);
	result += sqlite3_bind_int64(ppstmt, 5, time(NULL));
	
	if(result) {
		LOG_ERROR("Failed to bind values to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	result = sqlite3_step(ppstmt);
	
	sqlite3_finalize(ppstmt);
	
	changes = sqlite3_changes(db_conn);
	
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to update user: %s", sqlite3_errstr(result));
		
		return -1;
	}
	
	return changes;
}

int users_update_password(int user_id, const char *password) {
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_update_user_password[] = "UPDATE users SET (password_hash,password_salt,modification_date) = (?2, ?3, ?4) WHERE id = ?1;";
	char *salt, *hash;
	int changes = 0;
	
	if(password == NULL)
		return 0;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_update_user_password, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement for user password update: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	salt = auth_new_salt();
	
	if((hash = auth_hash_password(salt, password)) == NULL) {
		LOG_ERROR("Failed to calculate password hash.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -2;
	}
	
	// SQLITE_OK é zero, então somando todos os resultados podemos saber se algum falhou
	result = sqlite3_bind_int(ppstmt, 1, user_id);
	result += sqlite3_bind_text(ppstmt, 2, hash, -1, SQLITE_TRANSIENT);
	result += sqlite3_bind_text(ppstmt, 3, salt, -1, SQLITE_TRANSIENT);
	result += sqlite3_bind_int64(ppstmt, 4, time(NULL));
	
	free(hash);
	free(salt);
	
	if(result) {
		LOG_ERROR("Failed to bind values to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	result = sqlite3_step(ppstmt);
	
	sqlite3_finalize(ppstmt);
	
	changes = sqlite3_changes(db_conn);
	
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to update user password: %s", sqlite3_errstr(result));
		
		return -1;
	}
	
	return changes;
}
