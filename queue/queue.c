#include "queue.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static long next_job_id = 1;

int queue_init(submission_queue_t* queue) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    queue->shutdown = 0;
    
    if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
        return -1;
    }
    
    if (pthread_cond_init(&queue->cond, NULL) != 0) {
        pthread_mutex_destroy(&queue->mutex);
        return -1;
    }
    
    return 0;
}

void queue_destroy(submission_queue_t* queue) {
    pthread_mutex_lock(&queue->mutex);
    
    // Free all remaining jobs
    queue_job_t* current = queue->head;
    while (current) {
        queue_job_t* next = current->next;
        free(current->code);
        free(current->username);
        free(current);
        current = next;
    }
    
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    
    pthread_mutex_unlock(&queue->mutex);
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond);
}

long queue_add_job(submission_queue_t* queue, int problem_id, const char* code, 
                   language_t language, const char* username) {
    queue_job_t* job = malloc(sizeof(queue_job_t));
    if (!job) return -1;
    
    job->job_id = __sync_fetch_and_add(&next_job_id, 1);
    job->problem_id = problem_id;
    job->code = strdup(code);
    job->language = language;
    job->username = strdup(username);
    job->submitted_at = time(NULL);
    job->status = RESULT_PENDING;
    job->next = NULL;
    
    pthread_mutex_lock(&queue->mutex);
    
    if (queue->tail) {
        queue->tail->next = job;
    } else {
        queue->head = job;
    }
    queue->tail = job;
    queue->count++;
    
    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
    
    return job->job_id;
}

queue_job_t* queue_get_job(submission_queue_t* queue) {
    pthread_mutex_lock(&queue->mutex);
    
    while (queue->head == NULL && !queue->shutdown) {
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }
    
    if (queue->shutdown) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }
    
    queue_job_t* job = queue->head;
    queue->head = job->next;
    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    queue->count--;
    
    pthread_mutex_unlock(&queue->mutex);
    return job;
}

void queue_complete_job(queue_job_t* job, submission_result_t result) {
    if (job) {
        job->status = result;
        // In a real implementation, you'd store the result in a database
        // or notify waiting clients
        free(job->code);
        free(job->username);
        free(job);
    }
}

void* queue_worker_thread(void* arg) {
    queue_worker_t* worker = (queue_worker_t*)arg;
    submission_queue_t* queue = worker->queue;
    
    printf("Worker %d started\n", worker->worker_id);
    
    while (worker->active) {
        queue_job_t* job = queue_get_job(queue);
        if (!job) break;
        
        printf("Worker %d processing job %ld (problem %d)\n", 
               worker->worker_id, job->job_id, job->problem_id);
        
        // Convert language enum to string
        const char* lang_str;
        switch (job->language) {
            case LANG_C: lang_str = "C"; break;
            case LANG_CPP: lang_str = "CPP"; break;
            case LANG_JAVA: lang_str = "JAVA"; break;
            case LANG_PYTHON3: lang_str = "PYTHON3"; break;
            default: lang_str = "CPP"; break;
        }
        
        // Submit the solution (this would call the core submit function)
        char problem_id_str[16];
        snprintf(problem_id_str, sizeof(problem_id_str), "%d", job->problem_id);
        
        // For now, just simulate processing
        sleep(2); // Simulate submission time
        submission_result_t result = RESULT_ACCEPTED; // Simulate success
        
        printf("Worker %d completed job %ld with result %d\n", 
               worker->worker_id, job->job_id, result);
        
        queue_complete_job(job, result);
    }
    
    printf("Worker %d stopped\n", worker->worker_id);
    return NULL;
}

int queue_manager_init(queue_manager_t* manager, int num_workers) {
    if (queue_init(&manager->queue) != 0) {
        return -1;
    }
    
    manager->num_workers = num_workers;
    manager->workers = calloc(num_workers, sizeof(queue_worker_t));
    if (!manager->workers) {
        queue_destroy(&manager->queue);
        return -1;
    }
    
    manager->running = 0;
    return 0;
}

void queue_manager_destroy(queue_manager_t* manager) {
    queue_manager_stop(manager);
    queue_destroy(&manager->queue);
    free(manager->workers);
}

int queue_manager_start(queue_manager_t* manager) {
    if (manager->running) return 0;
    
    for (int i = 0; i < manager->num_workers; i++) {
        manager->workers[i].worker_id = i;
        manager->workers[i].queue = &manager->queue;
        manager->workers[i].active = 1;
        
        if (pthread_create(&manager->workers[i].thread, NULL, 
                          queue_worker_thread, &manager->workers[i]) != 0) {
            // Cleanup on failure
            for (int j = 0; j < i; j++) {
                manager->workers[j].active = 0;
                pthread_join(manager->workers[j].thread, NULL);
            }
            return -1;
        }
    }
    
    manager->running = 1;
    printf("Queue manager started with %d workers\n", manager->num_workers);
    return 0;
}

void queue_manager_stop(queue_manager_t* manager) {
    if (!manager->running) return;
    
    // Signal shutdown
    pthread_mutex_lock(&manager->queue.mutex);
    manager->queue.shutdown = 1;
    pthread_cond_broadcast(&manager->queue.cond);
    pthread_mutex_unlock(&manager->queue.mutex);
    
    // Stop all workers
    for (int i = 0; i < manager->num_workers; i++) {
        manager->workers[i].active = 0;
        pthread_join(manager->workers[i].thread, NULL);
    }
    
    manager->running = 0;
    printf("Queue manager stopped\n");
}

long queue_manager_submit(queue_manager_t* manager, int problem_id, 
                         const char* code, language_t language, const char* username) {
    return queue_add_job(&manager->queue, problem_id, code, language, username);
}