#include "../queue/queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

void test_basic_queue_operations() {
    printf("Testing basic queue operations...\n");
    
    submission_queue_t queue;
    assert(queue_init(&queue) == 0);
    
    // Add some jobs
    long job1 = queue_add_job(&queue, 1000, "#include <stdio.h>\nint main() { return 0; }", LANG_C, "testuser");
    long job2 = queue_add_job(&queue, 1001, "print('hello')", LANG_PYTHON3, "testuser");
    
    assert(job1 > 0);
    assert(job2 > 0);
    assert(job2 > job1);
    
    // Get jobs
    queue_job_t* retrieved_job1 = queue_get_job(&queue);
    assert(retrieved_job1 != NULL);
    assert(retrieved_job1->job_id == job1);
    assert(retrieved_job1->problem_id == 1000);
    
    queue_job_t* retrieved_job2 = queue_get_job(&queue);
    assert(retrieved_job2 != NULL);
    assert(retrieved_job2->job_id == job2);
    assert(retrieved_job2->problem_id == 1001);
    
    // Complete jobs
    queue_complete_job(retrieved_job1, RESULT_ACCEPTED);
    queue_complete_job(retrieved_job2, RESULT_WRONG_ANSWER);
    
    queue_destroy(&queue);
    printf("Basic queue operations test passed!\n");
}

void test_queue_manager() {
    printf("Testing queue manager...\n");
    
    queue_manager_t manager;
    assert(queue_manager_init(&manager, 2) == 0);
    assert(queue_manager_start(&manager) == 0);
    
    // Submit some jobs
    long job1 = queue_manager_submit(&manager, 1000, "int main() { return 0; }", LANG_C, "user1");
    long job2 = queue_manager_submit(&manager, 1001, "print('test')", LANG_PYTHON3, "user2");
    long job3 = queue_manager_submit(&manager, 1002, "System.out.println(\"test\");", LANG_JAVA, "user3");
    
    printf("Submitted jobs: %ld, %ld, %ld\n", job1, job2, job3);
    
    // Let workers process for a while
    sleep(3);
    
    queue_manager_stop(&manager);
    queue_manager_destroy(&manager);
    
    printf("Queue manager test passed!\n");
}

int main() {
    printf("Starting queue system tests...\n\n");
    
    test_basic_queue_operations();
    test_queue_manager();
    
    printf("\nAll queue tests passed!\n");
    return 0;
}