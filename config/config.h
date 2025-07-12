#ifndef AUTOSUBMIT_CONFIG_H
#define AUTOSUBMIT_CONFIG_H

// Configuration constants for the autosubmit system

// Jungol API Configuration
#define JUNGOL_BASE_URL "https://jungol.co.kr"
#define JUNGOL_API_BASE_URL "https://api-v7.jungol.co.kr"
#define JUNGOL_LOGIN_URL JUNGOL_BASE_URL "/auth/signin"
#define JUNGOL_API_LOGIN_URL JUNGOL_API_BASE_URL "/auth/signin"
#define JUNGOL_SUBMISSION_URL JUNGOL_API_BASE_URL "/submission"

// Default encryption key (should be externalized in production)
#define DEFAULT_XOR_KEY "770cf65b5965d85c4b3c09fce562f3f8"

// Default file paths
#define DEFAULT_COOKIE_FILE "jungol_cookies.txt"
#define DEFAULT_DB_FILE "autosubmit.db"
#define DEFAULT_CONFIG_FILE "autosubmit.conf"

// Queue configuration
#define DEFAULT_WORKER_COUNT 3
#define MAX_WORKER_COUNT 10
#define SUBMISSION_POLL_INTERVAL 2  // seconds
#define MAX_POLL_RETRIES 20

// Database configuration
#define MAX_PROBLEM_TITLE_LENGTH 256
#define MAX_PROBLEM_DESCRIPTION_LENGTH 4096
#define MAX_CODE_LENGTH 65536
#define MAX_USERNAME_LENGTH 64
#define MAX_ERROR_MESSAGE_LENGTH 1024

// TUI configuration
#define TUI_MIN_WIDTH 80
#define TUI_MIN_HEIGHT 24
#define TUI_STATUS_HEIGHT 3

// Supported languages
typedef struct {
    const char* name;
    const char* extension;
    const char* jungol_id;
} language_info_t;

static const language_info_t SUPPORTED_LANGUAGES[] = {
    {"C", ".c", "C"},
    {"C++", ".cpp", "CPP"},
    {"C++", ".cc", "CPP"},
    {"Java", ".java", "JAVA"},
    {"Python3", ".py", "PYTHON3"},
    {NULL, NULL, NULL}  // Sentinel
};

// HTTP configuration
#define HTTP_USER_AGENT "Mozilla/5.0 (Linux; AutoSubmit) AppleWebKit/537.36"
#define HTTP_TIMEOUT 30  // seconds
#define HTTP_MAX_REDIRECTS 5

// Retry configuration
#define MAX_LOGIN_RETRIES 3
#define MAX_SUBMISSION_RETRIES 3
#define RETRY_DELAY 1  // seconds

#endif // AUTOSUBMIT_CONFIG_H