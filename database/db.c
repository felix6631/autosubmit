#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// SQL statements for table creation
static const char* CREATE_PROBLEMS_TABLE = 
    "CREATE TABLE IF NOT EXISTS problems ("
    "problem_id INTEGER PRIMARY KEY,"
    "title TEXT NOT NULL,"
    "description TEXT,"
    "difficulty INTEGER DEFAULT 1,"
    "time_limit INTEGER DEFAULT 1000,"
    "memory_limit INTEGER DEFAULT 128,"
    "tags TEXT,"
    "created_at INTEGER DEFAULT (strftime('%s', 'now')),"
    "updated_at INTEGER DEFAULT (strftime('%s', 'now'))"
    ");";

static const char* CREATE_SUBMISSIONS_TABLE = 
    "CREATE TABLE IF NOT EXISTS submissions ("
    "submission_id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "problem_id INTEGER NOT NULL,"
    "username TEXT NOT NULL,"
    "language INTEGER NOT NULL,"
    "code TEXT NOT NULL,"
    "result INTEGER DEFAULT 0,"
    "score INTEGER DEFAULT 0,"
    "memory_usage INTEGER DEFAULT 0,"
    "execution_time INTEGER DEFAULT 0,"
    "error_message TEXT,"
    "submitted_at INTEGER DEFAULT (strftime('%s', 'now')),"
    "completed_at INTEGER,"
    "FOREIGN KEY (problem_id) REFERENCES problems(problem_id)"
    ");";

static const char* CREATE_USERS_TABLE = 
    "CREATE TABLE IF NOT EXISTS users ("
    "user_id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "username TEXT UNIQUE NOT NULL,"
    "email TEXT,"
    "total_submissions INTEGER DEFAULT 0,"
    "accepted_submissions INTEGER DEFAULT 0,"
    "last_login INTEGER,"
    "created_at INTEGER DEFAULT (strftime('%s', 'now'))"
    ");";

int db_init(database_t* db, const char* db_path) {
    db->db = NULL;
    db->db_path = strdup(db_path);
    db->is_open = 0;
    
    int rc = sqlite3_open(db_path, &db->db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db->db));
        sqlite3_close(db->db);
        free(db->db_path);
        return -1;
    }
    
    db->is_open = 1;
    
    // Enable foreign keys
    sqlite3_exec(db->db, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);
    
    return 0;
}

void db_close(database_t* db) {
    if (db->is_open) {
        sqlite3_close(db->db);
        db->is_open = 0;
    }
    free(db->db_path);
}

int db_create_tables(database_t* db) {
    char* err_msg = NULL;
    
    // Create problems table
    int rc = sqlite3_exec(db->db, CREATE_PROBLEMS_TABLE, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to create problems table: %s\n", err_msg);
        sqlite3_free(err_msg);
        return -1;
    }
    
    // Create submissions table
    rc = sqlite3_exec(db->db, CREATE_SUBMISSIONS_TABLE, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to create submissions table: %s\n", err_msg);
        sqlite3_free(err_msg);
        return -1;
    }
    
    // Create users table
    rc = sqlite3_exec(db->db, CREATE_USERS_TABLE, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to create users table: %s\n", err_msg);
        sqlite3_free(err_msg);
        return -1;
    }
    
    return 0;
}

int db_add_problem(database_t* db, const problem_t* problem) {
    const char* sql = "INSERT INTO problems (problem_id, title, description, difficulty, "
                     "time_limit, memory_limit, tags) VALUES (?, ?, ?, ?, ?, ?, ?)";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;
    
    sqlite3_bind_int(stmt, 1, problem->problem_id);
    sqlite3_bind_text(stmt, 2, problem->title, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, problem->description, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, problem->difficulty);
    sqlite3_bind_int(stmt, 5, problem->time_limit);
    sqlite3_bind_int(stmt, 6, problem->memory_limit);
    sqlite3_bind_text(stmt, 7, problem->tags, -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_get_problem(database_t* db, int problem_id, problem_t* problem) {
    const char* sql = "SELECT problem_id, title, description, difficulty, "
                     "time_limit, memory_limit, tags, created_at, updated_at "
                     "FROM problems WHERE problem_id = ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;
    
    sqlite3_bind_int(stmt, 1, problem_id);
    
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        problem->problem_id = sqlite3_column_int(stmt, 0);
        problem->title = strdup((char*)sqlite3_column_text(stmt, 1));
        problem->description = strdup((char*)sqlite3_column_text(stmt, 2));
        problem->difficulty = sqlite3_column_int(stmt, 3);
        problem->time_limit = sqlite3_column_int(stmt, 4);
        problem->memory_limit = sqlite3_column_int(stmt, 5);
        problem->tags = strdup((char*)sqlite3_column_text(stmt, 6));
        problem->created_at = sqlite3_column_int64(stmt, 7);
        problem->updated_at = sqlite3_column_int64(stmt, 8);
        
        sqlite3_finalize(stmt);
        return 0;
    }
    
    sqlite3_finalize(stmt);
    return -1;
}

long db_add_submission(database_t* db, const submission_record_t* submission) {
    const char* sql = "INSERT INTO submissions (problem_id, username, language, code) "
                     "VALUES (?, ?, ?, ?)";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;
    
    sqlite3_bind_int(stmt, 1, submission->problem_id);
    sqlite3_bind_text(stmt, 2, submission->username, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, (int)submission->language);
    sqlite3_bind_text(stmt, 4, submission->code, -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    long submission_id = sqlite3_last_insert_rowid(db->db);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE) ? submission_id : -1;
}

int db_update_submission_result(database_t* db, long submission_id, 
                               submission_result_t result, int score, 
                               int memory_usage, int execution_time, 
                               const char* error_message) {
    const char* sql = "UPDATE submissions SET result = ?, score = ?, memory_usage = ?, "
                     "execution_time = ?, error_message = ?, "
                     "completed_at = strftime('%s', 'now') WHERE submission_id = ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;
    
    sqlite3_bind_int(stmt, 1, (int)result);
    sqlite3_bind_int(stmt, 2, score);
    sqlite3_bind_int(stmt, 3, memory_usage);
    sqlite3_bind_int(stmt, 4, execution_time);
    sqlite3_bind_text(stmt, 5, error_message, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 6, submission_id);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE) ? 0 : -1;
}

void db_free_problem(problem_t* problem) {
    if (problem) {
        free(problem->title);
        free(problem->description);
        free(problem->tags);
        memset(problem, 0, sizeof(problem_t));
    }
}

void db_free_submission(submission_record_t* submission) {
    if (submission) {
        free(submission->username);
        free(submission->code);
        free(submission->error_message);
        memset(submission, 0, sizeof(submission_record_t));
    }
}

// Initialize database with some sample problems
int db_init_sample_data(database_t* db) {
    problem_t problems[] = {
        {1000, "A+B", "두 정수 A와 B를 입력받은 다음, A+B를 출력하는 프로그램을 작성하시오.", 1, 2000, 128, "수학,구현", 0, 0},
        {1001, "A-B", "두 정수 A와 B를 입력받은 다음, A-B를 출력하는 프로그램을 작성하시오.", 1, 2000, 128, "수학,구현", 0, 0},
        {1008, "A/B", "두 정수 A와 B를 입력받은 다음, A/B를 출력하는 프로그램을 작성하시오.", 1, 2000, 128, "수학,구현", 0, 0},
        {2741, "정수의 개수", "자연수 N이 주어졌을 때, N의 각 자릿수의 합을 구하는 프로그램을 작성하시오.", 2, 2000, 128, "수학,구현", 0, 0},
        {1003, "피보나치 함수", "피보나치 수열에 대한 문제", 3, 1000, 256, "동적계획법,수학", 0, 0}
    };
    
    int num_problems = sizeof(problems) / sizeof(problems[0]);
    for (int i = 0; i < num_problems; i++) {
        if (db_add_problem(db, &problems[i]) != 0) {
            // Problem might already exist, continue
        }
    }
    
    return 0;
}