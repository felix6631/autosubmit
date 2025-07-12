#ifndef AUTOSUBMIT_CORE_H
#define AUTOSUBMIT_CORE_H

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bson/bson.h>

// Core submission result codes
typedef enum {
    RESULT_PENDING = 0,
    RESULT_ACCEPTED = 1,
    RESULT_COMPILE_ERROR = 2,
    RESULT_RUNTIME_ERROR = 3,
    RESULT_TIME_LIMIT = 4,
    RESULT_WRONG_ANSWER = 5,
    RESULT_SEGFAULT = 31,
    RESULT_FPE = 32,
    RESULT_ABORT = 33,
    RESULT_MAIN_RETURN = 34,
    RESULT_MEMORY_LIMIT = 35
} submission_result_t;

// Language types
typedef enum {
    LANG_C,
    LANG_CPP,
    LANG_JAVA,
    LANG_PYTHON3
} language_t;

// Submission structure
typedef struct {
    long id;
    int problem_id;
    language_t language;
    char* code;
    submission_result_t result;
    int score;
    int memory_usage;
    int execution_time;
    char* error_message;
} submission_t;

// Core functions
int jungol_login(const char *username, const char *password);
int submit_solution(const char *problem_id, const char *code, const char *language);
int check_submission_result(long submission_id, const char* xor_key);
submission_result_t wait_for_submission_result(long submission_id, const char* xor_key);

// Utility functions
char* prepare_code_json(const char *file_path);
const char* result_to_string(submission_result_t result);
language_t detect_language_from_extension(const char* filename);

#endif // AUTOSUBMIT_CORE_H