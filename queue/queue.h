#ifndef AUTOSUBMIT_QUEUE_H
#define AUTOSUBMIT_QUEUE_H

#include <pthread.h>
#include <time.h>
#include "../core/core.h"

// Queue job structure
typedef struct queue_job {
    long job_id;
    int problem_id;
    char* code;
    language_t language;
    char* username;
    time_t submitted_at;
    submission_result_t status;
    struct queue_job* next;
} queue_job_t;

// Queue structure
typedef struct {
    queue_job_t* head;
    queue_job_t* tail;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int shutdown;
} submission_queue_t;

// Queue worker thread structure
typedef struct {
    int worker_id;
    submission_queue_t* queue;
    pthread_t thread;
    int active;
} queue_worker_t;

// Queue manager structure
typedef struct {
    submission_queue_t queue;
    queue_worker_t* workers;
    int num_workers;
    int running;
} queue_manager_t;

// Queue functions
int queue_init(submission_queue_t* queue);
void queue_destroy(submission_queue_t* queue);
long queue_add_job(submission_queue_t* queue, int problem_id, const char* code, 
                   language_t language, const char* username);
queue_job_t* queue_get_job(submission_queue_t* queue);
void queue_complete_job(queue_job_t* job, submission_result_t result);

// Queue manager functions
int queue_manager_init(queue_manager_t* manager, int num_workers);
void queue_manager_destroy(queue_manager_t* manager);
int queue_manager_start(queue_manager_t* manager);
void queue_manager_stop(queue_manager_t* manager);
long queue_manager_submit(queue_manager_t* manager, int problem_id, 
                         const char* code, language_t language, const char* username);

// Worker functions
void* queue_worker_thread(void* arg);

#endif // AUTOSUBMIT_QUEUE_H