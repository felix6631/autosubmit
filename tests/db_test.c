#include "../database/db.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

void test_database_init() {
    printf("Testing database initialization...\n");
    
    database_t db;
    assert(db_init(&db, ":memory:") == 0);
    assert(db_create_tables(&db) == 0);
    
    db_close(&db);
    printf("Database initialization test passed!\n");
}

void test_problem_operations() {
    printf("Testing problem operations...\n");
    
    database_t db;
    assert(db_init(&db, ":memory:") == 0);
    assert(db_create_tables(&db) == 0);
    
    // Add a problem
    problem_t problem = {
        .problem_id = 1000,
        .title = "A+B",
        .description = "Add two numbers",
        .difficulty = 1,
        .time_limit = 2000,
        .memory_limit = 128,
        .tags = "math,implementation"
    };
    
    assert(db_add_problem(&db, &problem) == 0);
    
    // Retrieve the problem
    problem_t retrieved;
    memset(&retrieved, 0, sizeof(retrieved));
    assert(db_get_problem(&db, 1000, &retrieved) == 0);
    
    assert(retrieved.problem_id == 1000);
    assert(strcmp(retrieved.title, "A+B") == 0);
    assert(strcmp(retrieved.description, "Add two numbers") == 0);
    assert(retrieved.difficulty == 1);
    
    db_free_problem(&retrieved);
    db_close(&db);
    printf("Problem operations test passed!\n");
}

void test_submission_operations() {
    printf("Testing submission operations...\n");
    
    database_t db;
    assert(db_init(&db, ":memory:") == 0);
    assert(db_create_tables(&db) == 0);
    
    // Add a problem first
    problem_t problem = {
        .problem_id = 1000,
        .title = "A+B",
        .description = "Add two numbers",
        .difficulty = 1,
        .time_limit = 2000,
        .memory_limit = 128,
        .tags = "math"
    };
    assert(db_add_problem(&db, &problem) == 0);
    
    // Add a submission
    submission_record_t submission = {
        .problem_id = 1000,
        .username = "testuser",
        .language = LANG_CPP,
        .code = "#include <iostream>\nint main() { int a, b; std::cin >> a >> b; std::cout << a + b; return 0; }",
        .result = RESULT_PENDING
    };
    
    long submission_id = db_add_submission(&db, &submission);
    assert(submission_id > 0);
    
    // Update submission result
    assert(db_update_submission_result(&db, submission_id, RESULT_ACCEPTED, 100, 1024, 150, NULL) == 0);
    
    db_close(&db);
    printf("Submission operations test passed!\n");
}

void test_sample_data() {
    printf("Testing sample data initialization...\n");
    
    database_t db;
    assert(db_init(&db, ":memory:") == 0);
    assert(db_create_tables(&db) == 0);
    assert(db_init_sample_data(&db) == 0);
    
    // Try to get a sample problem
    problem_t problem;
    memset(&problem, 0, sizeof(problem));
    assert(db_get_problem(&db, 1000, &problem) == 0);
    assert(strcmp(problem.title, "A+B") == 0);
    
    db_free_problem(&problem);
    db_close(&db);
    printf("Sample data test passed!\n");
}

int main() {
    printf("Starting database tests...\n\n");
    
    test_database_init();
    test_problem_operations();
    test_submission_operations();
    test_sample_data();
    
    printf("\nAll database tests passed!\n");
    return 0;
}