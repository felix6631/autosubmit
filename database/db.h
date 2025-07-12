#ifndef AUTOSUBMIT_DB_H
#define AUTOSUBMIT_DB_H

#include <sqlite3.h>
#include <time.h>
#include "../core/core.h"

// Database connection structure
typedef struct {
    sqlite3* db;
    char* db_path;
    int is_open;
} database_t;

// Problem structure
typedef struct {
    int problem_id;
    char* title;
    char* description;
    int difficulty;
    int time_limit;
    int memory_limit;
    char* tags;
    time_t created_at;
    time_t updated_at;
} problem_t;

// Submission record structure
typedef struct {
    long submission_id;
    int problem_id;
    char* username;
    language_t language;
    char* code;
    submission_result_t result;
    int score;
    int memory_usage;
    int execution_time;
    char* error_message;
    time_t submitted_at;
    time_t completed_at;
} submission_record_t;

// User structure
typedef struct {
    int user_id;
    char* username;
    char* email;
    int total_submissions;
    int accepted_submissions;
    time_t last_login;
    time_t created_at;
} user_t;

// Database functions
int db_init(database_t* db, const char* db_path);
void db_close(database_t* db);
int db_create_tables(database_t* db);

// Problem functions
int db_add_problem(database_t* db, const problem_t* problem);
int db_get_problem(database_t* db, int problem_id, problem_t* problem);
int db_list_problems(database_t* db, problem_t** problems, int* count);
int db_search_problems(database_t* db, const char* query, problem_t** problems, int* count);
void db_free_problem(problem_t* problem);
void db_free_problems(problem_t* problems, int count);

// Submission functions
long db_add_submission(database_t* db, const submission_record_t* submission);
int db_get_submission(database_t* db, long submission_id, submission_record_t* submission);
int db_update_submission_result(database_t* db, long submission_id, 
                               submission_result_t result, int score, 
                               int memory_usage, int execution_time, 
                               const char* error_message);
int db_list_user_submissions(database_t* db, const char* username, 
                            submission_record_t** submissions, int* count);
void db_free_submission(submission_record_t* submission);
void db_free_submissions(submission_record_t* submissions, int count);

// User functions
int db_add_user(database_t* db, const user_t* user);
int db_get_user(database_t* db, const char* username, user_t* user);
int db_update_user_stats(database_t* db, const char* username);
void db_free_user(user_t* user);

// Statistics functions
int db_get_problem_stats(database_t* db, int problem_id, int* total_submissions, 
                        int* accepted_submissions, double* acceptance_rate);
int db_get_user_stats(database_t* db, const char* username, int* total_problems_solved,
                     int* total_submissions, double* success_rate);

#endif // AUTOSUBMIT_DB_H