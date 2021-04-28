#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <openssl/evp.h>
#include <uuid/uuid.h>

#include "database.h"
#include "logger.h"

#define SESSION_KEY_DURATION_HOURS 8
#define HASH_STR_SIZE 65

static int calc_password_hash(const char *salt, const char *password, char result[HASH_STR_SIZE]) {
	EVP_MD_CTX *mdctx = NULL;
	unsigned char md_result[EVP_MAX_MD_SIZE];
	unsigned int md_result_size;
	
	if(salt == NULL || password == NULL)
		return -1;
	
	mdctx = EVP_MD_CTX_new();
	EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
	EVP_DigestUpdate(mdctx, salt, strlen(salt));
	EVP_DigestUpdate(mdctx, password, strlen(password));
	EVP_DigestFinal_ex(mdctx, md_result, &md_result_size);
	EVP_MD_CTX_free(mdctx);
	
	if(md_result_size != 32)
		return -2;
	
	for(int i = 0; i < 32; i++)
		sprintf(result + i * 2, "%02x", md_result[i]);
	
	return 0;
}

int auth_verify_key(const char *key) {
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_verify_key[] = "SELECT user_id FROM sessions WHERE key=?1 AND valid_thru>=?2;";
	int user_id = 0;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_verify_key, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	// SQLITE_OK é zero, então somando todos os resultados podemos saber se algum falhou
	result = sqlite3_bind_text(ppstmt, 1, key, -1, SQLITE_STATIC);
	result += sqlite3_bind_int64(ppstmt, 2, time(NULL));
	
	if(result) {
		LOG_ERROR("Failed to bind value to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	do {
		if((result = sqlite3_step(ppstmt)) == SQLITE_ROW)
			user_id = sqlite3_column_int(ppstmt, 0);
	} while (result == SQLITE_ROW);
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to check authentication key: %s", sqlite3_errstr(result));
		
		return -2;
	}
	
	return user_id;
}

int auth_user_login(const char *username, const char *password, int *user_id_ptr) {
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_find_user[] = "SELECT id,is_active,password_salt,password_hash FROM users WHERE name=?1;";
	const char *str_ptr = NULL;
	int user_id = 0;
	int user_is_active = 0;
	char salt[UUID_STR_LEN];
	char correct_hash[HASH_STR_SIZE], hash[HASH_STR_SIZE];
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_find_user, -1, &ppstmt, NULL)) != SQLITE_OK) {
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
		user_is_active = sqlite3_column_int(ppstmt, 1);
		
		if((str_ptr = (const char*) sqlite3_column_text(ppstmt, 2)))
			strncpy(salt, str_ptr, sizeof(salt));
		else if(sqlite3_errcode(db_conn) == SQLITE_NOMEM)
			LOG_ERROR("Out of memory.");
		
		if((str_ptr = (const char*) sqlite3_column_text(ppstmt, 3)))
			strncpy(correct_hash, str_ptr, sizeof(correct_hash));
		else if(sqlite3_errcode(db_conn) == SQLITE_NOMEM)
			LOG_ERROR("Out of memory.");
		
		result = sqlite3_step(ppstmt);
	}
	
	sqlite3_finalize(ppstmt);
	sqlite3_close(db_conn);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to check user login: %s", sqlite3_errstr(result));
		return -1;
	}
	
	if(user_id == 0) // Usuário não existe
		return 1;
	
	if(user_is_active == 0) // Usuário não está ativo
		return 2;
	
	salt[sizeof(salt) - 1] = '\0';
	
	if(calc_password_hash(salt, password, hash))
		return -2;
	
	if(strncmp(correct_hash, hash, HASH_STR_SIZE)) // Senha incorreta
		return 3;
	
	if(user_id_ptr)
		*user_id_ptr = user_id;
	
	return 0;
}

char *auth_new_session(int user_id) {
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_insert_session[] = "INSERT INTO sessions (key,user_id,valid_thru) VALUES(?1,?2,?3);";
	const char sql_delete_sessions[] = "DELETE FROM sessions WHERE valid_thru < ?1 OR (user_id=?2 AND key NOT IN (SELECT key FROM sessions WHERE user_id=?2 ORDER BY valid_thru DESC LIMIT 3));";
	uuid_t key_uuid;
	char session_key[UUID_STR_LEN];
	time_t session_valid_thru;
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return NULL;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_insert_session, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return NULL;
	}
	
	// Gera uma chave aleatória e única usando a biblioteca uuid
	uuid_generate_random(key_uuid);
	uuid_unparse_lower(key_uuid, session_key);
	
	session_valid_thru = time(NULL) + 3600 * SESSION_KEY_DURATION_HOURS;
	
	// SQLITE_OK é zero, então somando todos os resultados podemos saber se algum falhou
	result = sqlite3_bind_text(ppstmt, 1, session_key, -1, SQLITE_STATIC);
	result += sqlite3_bind_int(ppstmt, 2, user_id);
	result += sqlite3_bind_int64(ppstmt, 3, session_valid_thru);
	
	if(result) {
		LOG_ERROR("Failed to bind value to prepared statement.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return NULL;
	}
	
	result = sqlite3_step(ppstmt);
	
	sqlite3_finalize(ppstmt);
	
	if(result != SQLITE_DONE) {
		LOG_ERROR("Failed to create new user session: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return NULL;
	}
	
	// Deleta as sessões expiradas e as sessões excedentes
	if((result = sqlite3_prepare_v2(db_conn, sql_delete_sessions, -1, &ppstmt, NULL)) == SQLITE_OK) {
		
		result = sqlite3_bind_int64(ppstmt, 1, time(NULL));
		result += sqlite3_bind_int(ppstmt, 2, user_id);
		
		if(result == SQLITE_OK) {
			if((result = sqlite3_step(ppstmt)) != SQLITE_DONE)
				LOG_ERROR("Failed to clean sessions: %s", sqlite3_errstr(result));
		} else {
			LOG_ERROR("Failed to bind values to user session cleaning query.");
		}
		
		sqlite3_finalize(ppstmt);
	} else {
		LOG_ERROR("Failed to prepare SQL statement for cleaning user sessions: %s", sqlite3_errstr(result));
	}
	
	sqlite3_close(db_conn);
	
	return strdup(session_key);
}

int auth_change_user_password(int user_id, const char *new_password) {
	int result;
	sqlite3 *db_conn = NULL;
	sqlite3_stmt *ppstmt = NULL;
	const char sql_insert_session[] = "UPDATE users SET (password_salt,password_hash,modification_date) = (?2,?3,?4) WHERE user_id = ?1;";
	uuid_t new_salt_uuid;
	char new_salt[UUID_STR_LEN];
	char password_hash[HASH_STR_SIZE];
	
	if((result = sqlite3_open(DB_FILENAME, &db_conn)) != SQLITE_OK) {
		LOG_ERROR("Failed to open database connection: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	sqlite3_busy_timeout(db_conn, 1000);
	
	if((result = sqlite3_prepare_v2(db_conn, sql_insert_session, -1, &ppstmt, NULL)) != SQLITE_OK) {
		LOG_ERROR("Failed to prepare SQL statement: %s", sqlite3_errstr(result));
		sqlite3_close(db_conn);
		
		return -1;
	}
	
	// Gera um novo salt para a senha
	uuid_generate_random(new_salt_uuid);
	uuid_unparse_lower(new_salt_uuid, new_salt);
	
	if(calc_password_hash(new_salt, new_password, password_hash)) {
		LOG_ERROR("Failed to calculate the new password hash.");
		sqlite3_finalize(ppstmt);
		sqlite3_close(db_conn);
		
		return -2;
	}
	
	// SQLITE_OK é zero, então somando todos os resultados podemos saber se algum falhou
	result = sqlite3_bind_int(ppstmt, 1, user_id);
	result += sqlite3_bind_text(ppstmt, 2, new_salt, -1, SQLITE_STATIC);
	result += sqlite3_bind_text(ppstmt, 3, password_hash, -1, SQLITE_STATIC);
	result += sqlite3_bind_int64(ppstmt, 4, time(NULL));
	
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
		LOG_ERROR("Failed to update user password: %s", sqlite3_errstr(result));
		
		return -1;
	}
	
	return 0;
}
