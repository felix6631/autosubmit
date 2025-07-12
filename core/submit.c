#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bson/bson.h>

// 응답 데이터를 저장하기 위한 구조체
struct MemoryStruct {
    char *memory;
    size_t size;
};

// 함수 프로토타입 선언
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);
int jungol_login(const char *username, const char *password);
int check_login_status();
char* extract_token(const char* html);  
int submit_solution(const char *problem_id, const char *code, const char *language);
char *prepare_code_json(const char *file_path);
unsigned char* create_encrypted_submission(const char* problem_id, const char* lang, const char* code, const char* code_filename, const char* xor_key, size_t* out_len);

unsigned char* decrypt_response(const unsigned char* data, size_t len, const char* xor_key);
long extract_submission_id(const char* decrypted_data, size_t len);
int check_submission_result(long submission_id, const char* xor_key);
int parse_submission_result(const unsigned char* decrypted_data, size_t len);
int wait_for_submission_result(long submission_id, const char* xor_key);
long extract_submission_id_fallback(const char* data, size_t len);

void print_bson_document(const uint8_t* bson_data, size_t length);
void print_sub_document(const bson_t* doc, int indent_level);

int detect_runtime_error_type(const char* message);
void print_result_description(int result_code);
char* extract_error_log(const char* html);

char* fetch_error_log(long submission_id);
int analyze_runtime_error(const char* error_log);
char* fetch_submission_details(long submission_id);
// 이제 함수 구현들...

char cached_token[256] = {0,};
// 상태 및 설정을 관리하기 위한 전역 변수
char cookie_file[256] = "jungol_cookies.txt";
int verbose_mode = 1; // 상세 로깅 활성화

/**
 * 채점 결과 상세 페이지에서 오류 로그를 가져오는 함수
 */
char* fetch_error_log(long submission_id) {
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;
    
    // 메모리 초기화
    chunk.memory = malloc(65536);  // 64KB 할당 (충분히 큰 크기)
    if (!chunk.memory) {
        printf("메모리 할당 실패\n");
        return NULL;
    }
    chunk.size = 0;
    
    curl = curl_easy_init();
    if (!curl) {
        free(chunk.memory);
        return NULL;
    }
    
    // URL 설정 (직접 채점 결과 페이지로 접근)
    char url[256];
    sprintf(url, "https://jungol.co.kr/submission/%ld", submission_id);
    
    if (verbose_mode) {
        printf("상세 결과 URL: %s\n", url);
    }
    
    // CURL 옵션 설정
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, 
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/136.0.0.0 Safari/537.36");
    
    // 쿠키 사용
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookie_file);
    
    // HTTP/2 활성화
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
    
    if (verbose_mode) {
        printf("HTML 페이지 요청 중...\n");
    }
    
    // 요청 수행
    res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        fprintf(stderr, "상세 결과 페이지 가져오기 실패: %s\n", curl_easy_strerror(res));
        free(chunk.memory);
        curl_easy_cleanup(curl);
        return NULL;
    }
    
    // HTTP 응답 코드 확인
    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    
    if (response_code != 200) {
        printf("서버 응답 오류: HTTP %ld\n", response_code);
        free(chunk.memory);
        curl_easy_cleanup(curl);
        return NULL;
    }
    
    if (verbose_mode) {
        printf("HTML 페이지 크기: %zu 바이트\n", chunk.size);
        // 페이지 처음 부분 출력 (디버깅용)
        printf("HTML 처음 200자:\n%.*s\n...\n", 
               chunk.size > 200 ? 200 : (int)chunk.size, chunk.memory);
    }
    
    // HTML에서 오류 로그 추출
    char* error_log = NULL;
    
    // 다양한 가능한 패턴 시도
    const char* patterns[] = {
        "<pre class=\"svelte-1p9fcbm\">",
        "<pre class=\"svelte-",
        "<pre class=",
        "<div class=\"error\">",
        "<div class=\"result\">",
        "<div id=\"result\">"
    };
    
    int num_patterns = sizeof(patterns) / sizeof(patterns[0]);
    
    for (int i = 0; i < num_patterns; i++) {
        const char* pattern = patterns[i];
        const char* pre_tag = strstr(chunk.memory, pattern);
        
        if (pre_tag) {
            if (verbose_mode) {
                printf("패턴 발견: %s\n", pattern);
            }
            
            // 태그 내용 시작 위치로 이동
            pre_tag += strlen(pattern);
            
            // 닫는 태그 찾기 (여러 가능성 시도)
            const char* end_tag = NULL;
            
            if (strstr(pattern, "pre")) {
                end_tag = strstr(pre_tag, "</pre>");
            } else if (strstr(pattern, "div")) {
                end_tag = strstr(pre_tag, "</div>");
            }
            
            if (end_tag) {
                // 내용 복사
                size_t log_len = end_tag - pre_tag;
                error_log = (char*)malloc(log_len + 1);
                
                if (error_log) {
                    strncpy(error_log, pre_tag, log_len);
                    error_log[log_len] = '\0';
                    
                    if (verbose_mode) {
                        printf("추출된 오류 로그:\n%s\n", error_log);
                    }
                    
                    // 첫 번째 일치하는 패턴에서 로그를 추출했으면 루프 종료
                    break;
                }
            } else if (verbose_mode) {
                printf("닫는 태그를 찾을 수 없음\n");
            }
        }
    }
    
    // 결과 상태 키워드 기반 검색 (이전 방법이 실패한 경우)
    if (!error_log) {
        const char* result_keywords[] = {
            "시간 초과", "Time Limit Exceeded",
            "런타임 에러", "Runtime Error",
            "메모리 초과", "Memory Limit Exceeded",
            "컴파일 에러", "Compile Error",
            "틀렸습니다", "Wrong Answer",
            "정답입니다", "Accepted"
        };
        
        int num_keywords = sizeof(result_keywords) / sizeof(result_keywords[0]);
        
        for (int i = 0; i < num_keywords; i++) {
            const char* keyword = result_keywords[i];
            if (strstr(chunk.memory, keyword)) {
                error_log = strdup(keyword);
                if (verbose_mode) {
                    printf("키워드 발견: %s\n", keyword);
                }
                break;
            }
        }
    }
    
    // 모든 시도가 실패한 경우, 페이지 HTML 저장 (디버깅용)
    if (!error_log && verbose_mode) {
        printf("HTML 구조 분석: 오류 로그를 추출할 수 없음\n");
        
        // HTML 파일로 저장 (디버깅용)
        FILE* debug_file = fopen("debug_html.txt", "w");
        if (debug_file) {
            fwrite(chunk.memory, 1, chunk.size, debug_file);
            fclose(debug_file);
            printf("디버깅을 위해 HTML을 'debug_html.txt'에 저장했습니다.\n");
        }
    }
    
    // 메모리 정리
    free(chunk.memory);
    curl_easy_cleanup(curl);
    
    return error_log;
}

/**
 * 오류 로그를 분석하는 함수 (개선된 버전)
 */
int analyze_runtime_error(const char* error_log) {
    if (!error_log) return 0;  // 로그 없음
    
    // 오류 유형 판단
    if (strstr(error_log, "시간 초과") || 
        strstr(error_log, "Time Limit Exceeded") || 
        strstr(error_log, "TLE")) {
        return 4;  // 시간 초과
    } else if (strstr(error_log, "틀렸") || 
              strstr(error_log, "Wrong Answer") || 
              strstr(error_log, "WA")) {
        return 5;  // 오답
    } else if (strstr(error_log, "컴파일") || 
              strstr(error_log, "Compile Error") || 
              strstr(error_log, "CE")) {
        return 2;  // 컴파일 에러
    } else if (strstr(error_log, "정답") || 
              strstr(error_log, "Accepted") || 
              strstr(error_log, "AC")) {
        return 1;  // 정답
    }
    
    // 런타임 에러의 세부 유형 판단
    if (strstr(error_log, "세그멘테이션") || 
        strstr(error_log, "Segmentation fault") || 
        strstr(error_log, "signal 11") || 
        strstr(error_log, "SIGSEGV")) {
        return 31;  // 세그멘테이션 오류
    } else if (strstr(error_log, "Floating point") || 
              strstr(error_log, "signal 8") || 
              strstr(error_log, "SIGFPE")) {
        return 32;  // 부동소수점 예외
    } else if (strstr(error_log, "Aborted") || 
              strstr(error_log, "signal 6") || 
              strstr(error_log, "SIGABRT")) {
        return 33;  // 비정상 종료
    } else if (strstr(error_log, "main") && 
              strstr(error_log, "return")) {
        return 34;  // main 함수 리턴 값 오류
    } else if (strstr(error_log, "메모리 제한") || 
              strstr(error_log, "Memory Limit Exceeded") || 
              strstr(error_log, "MLE")) {
        return 35;  // 메모리 제한 초과
    } else if (strstr(error_log, "런타임") || 
              strstr(error_log, "Runtime Error") || 
              strstr(error_log, "RE") || 
              strstr(error_log, "signal")) {
        return 3;   // 기본 런타임 에러
    }
    
    // 로그 내용이 충분하지 않을 경우 0 반환 (계속 대기)
    return 0;
}


/**
 * 채점 결과 페이지에서 상세 오류 정보를 가져오는 함수
 */
char* fetch_submission_details(long submission_id) {
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;
    
    chunk.memory = malloc(16384);  // 더 큰 버퍼 할당
    if (!chunk.memory) {
        printf("메모리 할당 실패\n");
        return NULL;
    }
    chunk.size = 0;
    
    curl = curl_easy_init();
    if (!curl) {
        free(chunk.memory);
        return NULL;
    }
    
    // URL 설정
    char url[256];
    sprintf(url, "https://jungol.co.kr/submission/%ld", submission_id);
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookie_file);
    curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookie_file);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, 
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/136.0.0.0 Safari/537.36");
    
    res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        fprintf(stderr, "상세 결과 가져오기 실패: %s\n", curl_easy_strerror(res));
        free(chunk.memory);
        curl_easy_cleanup(curl);
        return NULL;
    }
    
    curl_easy_cleanup(curl);
    return chunk.memory;
}

/**
 * HTML에서 오류 로그를 추출하는 함수
 */
char* extract_error_log(const char* html) {
    if (!html) return NULL;
    
    // <pre class="svelte-1p9fcbm"> 태그 찾기
    const char* pre_tag = strstr(html, "<pre class=\"svelte-1p9fcbm\">");
    if (!pre_tag) return NULL;
    
    // 태그 내용의 시작 위치로 이동
    pre_tag += strlen("<pre class=\"svelte-1p9fcbm\">");
    
    // 닫는 태그 찾기
    const char* end_tag = strstr(pre_tag, "</pre>");
    if (!end_tag) return NULL;
    
    // 내용 복사
    size_t len = end_tag - pre_tag;
    char* error_log = (char*)malloc(len + 1);
    if (!error_log) return NULL;
    
    strncpy(error_log, pre_tag, len);
    error_log[len] = '\0';
    
    return error_log;
}

/**
 * 런타임 에러 유형을 판단하는 함수
 */
int detect_runtime_error_type(const char* message) {
    if (!message) return 30;  // 기본 런타임 에러
    
    if (strstr(message, "signal 11") || strstr(message, "segmentation fault"))
        return 31;  // 세그멘테이션 오류
    else if (strstr(message, "signal 8") || strstr(message, "floating point"))
        return 32;  // 부동소수점 예외
    else if (strstr(message, "return") && strstr(message, "main"))
        return 33;  // main의 return 값 오류
    else if (strstr(message, "stack") || strstr(message, "heap"))
        return 34;  // 메모리 문제 (스택 오버플로우 등)
    
    return 30;  // 알 수 없는 런타임 에러
}

/**
 * 결과 코드에 따른 설명 출력 함수 (상세 런타임 에러 유형 포함)
 */
void print_result_description(int result_code) {
    printf("-------------------------------------------\n");
    printf("채점 결과 코드: %d\n", result_code);
    
    switch(result_code) {
        case 1:
            printf("정답입니다!\n");
            break;
        case 2:
            printf("컴파일 오류입니다.\n");
            break;
        case 3:
            printf("런타임 오류입니다.\n");
            break;
        case 4:
            printf("시간 초과입니다.\n");
            break;
        case 5:
            printf("틀렸습니다.\n");
            break;
        case 31:
            printf("런타임 오류: 세그멘테이션 오류 (Segmentation Fault)\n");
            printf("- 잘못된 메모리 접근 (배열 범위 초과, 널 포인터 참조 등)\n");
            break;
        case 32:
            printf("런타임 오류: 부동소수점 예외 (Floating Point Exception)\n");
            printf("- 0으로 나누기, 오버플로우 등의 수학 연산 오류\n");
            break;
        case 33:
            printf("런타임 오류: 비정상 종료 (Aborted)\n");
            printf("- assert 실패, abort() 호출 등\n");
            break;
        case 34:
            printf("런타임 오류: main 함수의 return 값이 0이 아님\n");
            break;
        case 35:
            printf("런타임 오류: 메모리 제한 초과\n");
            printf("- 메모리 사용량이 제한을 초과했습니다.\n");
            break;
        case 0:
            printf("채점 중입니다.\n");
            break;
        default:
            if (result_code < 0)
                printf("채점 시스템 연결 오류 또는 요청 실패\n");
            else
                printf("알 수 없는 결과 코드\n");
            break;
    }
    printf("-------------------------------------------\n");
}

/**
 * BSON 문서를 읽기 쉬운 형태로 출력하는 함수
 */
void print_bson_document(const uint8_t* bson_data, size_t length) {
    bson_t b;
    if (!bson_init_static(&b, bson_data, length)) {
        printf("BSON 초기화 실패\n");
        return;
    }
    
    printf("---- BSON 문서 시작 ----\n");
    
    bson_iter_t iter;
    if (bson_iter_init(&iter, &b)) {
        while (bson_iter_next(&iter)) {
            const char* key = bson_iter_key(&iter);
            printf("키: %s, 타입: %d", key, bson_iter_type(&iter));
            
            // 값 출력 (타입에 따라 다르게 처리)
            switch (bson_iter_type(&iter)) {
                case BSON_TYPE_UTF8: {
                    uint32_t len;
                    const char* str = bson_iter_utf8(&iter, &len);
                    printf(", 값: \"%s\" (길이: %u)\n", str, len);
                    break;
                }
                case BSON_TYPE_INT32: {
                    int32_t val = bson_iter_int32(&iter);
                    printf(", 값: %d\n", val);
                    break;
                }
                case BSON_TYPE_INT64: {
                    int64_t val = bson_iter_int64(&iter);
                    printf(", 값: %lld\n", val);
                    break;
                }
                case BSON_TYPE_DOUBLE: {
                    double val = bson_iter_double(&iter);
                    printf(", 값: %f\n", val);
                    break;
                }
                case BSON_TYPE_BOOL: {
                    bool val = bson_iter_bool(&iter);
                    printf(", 값: %s\n", val ? "true" : "false");
                    break;
                }
                case BSON_TYPE_NULL: {
                    printf(", 값: null\n");
                    break;
                }
                case BSON_TYPE_DOCUMENT: {
                    printf(" (하위 문서)\n");
                    const uint8_t* document_data;
                    uint32_t document_len;
                    bson_iter_document(&iter, &document_len, &document_data);
                    
                    // 문서 들여쓰기하여 재귀적으로 출력
                    bson_t sub_doc;
                    if (bson_init_static(&sub_doc, document_data, document_len)) {
                        print_sub_document(&sub_doc, 1);  // 1 단계 들여쓰기로 시작
                    }
                    break;
                }
                case BSON_TYPE_ARRAY: {
                    printf(" (배열)\n");
                    const uint8_t* array_data;
                    uint32_t array_len;
                    bson_iter_array(&iter, &array_len, &array_data);
                    
                    // 배열 들여쓰기하여 재귀적으로 출력
                    bson_t array_doc;
                    if (bson_init_static(&array_doc, array_data, array_len)) {
                        print_sub_document(&array_doc, 1);  // 1 단계 들여쓰기로 시작
                    }
                    break;
                }
                default:
                    printf(", 값: <복잡한 타입 또는 바이너리>\n");
                    break;
            }
        }
    }
    
    printf("---- BSON 문서 끝 ----\n");
}

/**
 * 하위 BSON 문서를 출력하는 함수 (재귀적으로 사용)
 */
void print_sub_document(const bson_t* doc, int indent_level) {
    bson_iter_t iter;
    if (bson_iter_init(&iter, doc)) {
        while (bson_iter_next(&iter)) {
            const char* key = bson_iter_key(&iter);
            
            // 들여쓰기 출력
            for (int i = 0; i < indent_level; i++) {
                printf("  ");
            }
            
            printf("키: %s, 타입: %d", key, bson_iter_type(&iter));
            
            // 값 출력 (타입에 따라 다르게 처리)
            switch (bson_iter_type(&iter)) {
                case BSON_TYPE_UTF8: {
                    uint32_t len;
                    const char* str = bson_iter_utf8(&iter, &len);
                    printf(", 값: \"%s\" (길이: %u)\n", str, len);
                    break;
                }
                case BSON_TYPE_INT32: {
                    int32_t val = bson_iter_int32(&iter);
                    printf(", 값: %d\n", val);
                    break;
                }
                case BSON_TYPE_INT64: {
                    int64_t val = bson_iter_int64(&iter);
                    printf(", 값: %lld\n", val);
                    break;
                }
                case BSON_TYPE_DOUBLE: {
                    double val = bson_iter_double(&iter);
                    printf(", 값: %f\n", val);
                    break;
                }
                case BSON_TYPE_BOOL: {
                    bool val = bson_iter_bool(&iter);
                    printf(", 값: %s\n", val ? "true" : "false");
                    break;
                }
                case BSON_TYPE_NULL: {
                    printf(", 값: null\n");
                    break;
                }
                case BSON_TYPE_DOCUMENT: {
                    printf(" (하위 문서)\n");
                    const uint8_t* document_data;
                    uint32_t document_len;
                    bson_iter_document(&iter, &document_len, &document_data);
                    
                    // 문서 들여쓰기하여 재귀적으로 출력
                    bson_t sub_doc;
                    if (bson_init_static(&sub_doc, document_data, document_len)) {
                        print_sub_document(&sub_doc, indent_level + 1);
                    }
                    break;
                }
                case BSON_TYPE_ARRAY: {
                    printf(" (배열)\n");
                    const uint8_t* array_data;
                    uint32_t array_len;
                    bson_iter_array(&iter, &array_len, &array_data);
                    
                    // 배열 들여쓰기하여 재귀적으로 출력
                    bson_t array_doc;
                    if (bson_init_static(&array_doc, array_data, array_len)) {
                        print_sub_document(&array_doc, indent_level + 1);
                    }
                    break;
                }
                default:
                    printf(", 값: <복잡한 타입 또는 바이너리>\n");
                    break;
            }
        }
    }
}

/**
 * 채점 결과를 기다리는 함수 (개선된 버전)
 */
int wait_for_submission_result(long submission_id, const char* xor_key) {
    int result = 0;
    int retry_count = 0;
    const int max_retries = 20;  // 최대 20번 시도 (약 40초)
    int pd_count = 0;
    
    printf("채점 결과를 기다리는 중");
    fflush(stdout);
    
    while (retry_count < max_retries) {
        // API를 통한 채점 결과 확인
        result = check_submission_result(submission_id, xor_key);
        
        // 명확한 결과가 나왔으면 바로 반환
        if (result == 1 || result == 2 || result == 3 || result == 4 || result == 5) {
            printf("\n");
            return result;
        }
        
        // API 요청 실패 또는 채점 중(PD) 상태
        if (result <= 0) {
            pd_count++;
            printf(".");
            fflush(stdout);
            
            // 3회 이상 채점 중 상태가 지속되면 HTML 페이지 확인
            if (pd_count >= 3) {
                printf("\n채점 상태 확인을 위해 HTML 페이지를 확인합니다.\n");
                
                // HTML 페이지에서 오류 로그 확인
                char* error_log = fetch_error_log(submission_id);
                
                if (error_log) {
                    int detailed_result = analyze_runtime_error(error_log);
                    free(error_log);
                    
                    if (detailed_result != 0) {
                        return detailed_result;  // 상세 결과 반환
                    }
                    
                    // 로그가 있지만 판단할 수 없는 경우
                    // PD 상태가 오래 지속되면 시간 초과로 간주
                    if (pd_count >= 7) {
                        printf("PD 상태가 장시간 지속되어 런타임 에러로 판단합니다.\n");
                        return 3;  // 런타임 에러로 간주
                    }
                } else {
                    // 로그 추출 실패 시, 단순히 PD 상태가 오래 지속되면 런타임 에러로 간주
                    if (pd_count >= 7) {
                        printf("채점 프로세스 지연: 런타임 에러로 추정합니다.\n");
                        return 3;  // 런타임 에러로 간주
                    }
                }
            }
        }
        
        // 다음 확인까지 대기
        sleep(2);  // 2초 대기
        retry_count++;
    }
    
    printf("\n최대 재시도 횟수 초과: 채점 시간이 너무 오래 걸립니다.\n");
    
    // 모든 시도 실패: 마지막으로 HTML 페이지 확인
    char* error_log = fetch_error_log(submission_id);
    if (error_log) {
        int detailed_result = analyze_runtime_error(error_log);
        free(error_log);
        
        if (detailed_result != 0) {
            return detailed_result;
        }
    }
    
    // 최종적으로 런타임 에러로 판단
    return 3;
}
/**
 * BSON 응답에서 채점 상태를 분석하는 함수 (개선된 버전)
 */
int parse_submission_result(const unsigned char* decrypted_data, size_t len) {
    bson_t b;
    if (!bson_init_static(&b, decrypted_data, len)) {
        printf("BSON 파싱 실패\n");
        return -1;
    }
    
    bson_iter_t iter;
    int result = -1;
    const char* reason_code = NULL;
    int32_t score = 0;
    
    // 'data' 문서 체크
    if (bson_iter_init_find(&iter, &b, "data")) {
        if (bson_iter_type(&iter) == BSON_TYPE_DOCUMENT) {
            bson_t data_doc;
            const uint8_t* data_buf;
            uint32_t data_len;
            
            bson_iter_document(&iter, &data_len, &data_buf);
            if (bson_init_static(&data_doc, data_buf, data_len)) {
                bson_iter_t data_iter;
                
                // m_reason 필드 확인
                if (bson_iter_init_find(&data_iter, &data_doc, "m_reason")) {
                    reason_code = bson_iter_utf8(&data_iter, NULL);
                    printf("응답 코드: %s\n", reason_code);
                }
                
                // 점수 확인
                if (bson_iter_init_find(&data_iter, &data_doc, "score")) {
                    score = bson_iter_int32(&data_iter);
                    printf("점수: %d\n", score);
                }
                
                // 메모리 사용량
                if (bson_iter_init_find(&data_iter, &data_doc, "m_memory")) {
                    int32_t memory = bson_iter_int32(&data_iter);
                    printf("메모리 사용량: %d KB\n", memory);
                }
                
                // 실행 시간
                if (bson_iter_init_find(&data_iter, &data_doc, "m_time")) {
                    int32_t time = bson_iter_int32(&data_iter);
                    printf("실행 시간: %d ms\n", time);
                }
                
                // 메시지 필드 확인 (컴파일 에러 등)
                if (bson_iter_init_find(&data_iter, &data_doc, "message")) {
                    const char* message = bson_iter_utf8(&data_iter, NULL);
                    if (message && strlen(message) > 0) {
                        // 컴파일 에러 메시지가 있으면 출력
                        printf("메시지:\n%s\n", message);
                        
                        // 명확한 컴파일 에러인 경우
                        if (strstr(message, "error:")) {
                            return 2;  // 컴파일 에러
                        }
                    }
                }
                
                // additional 문서 체크 (추가 정보)
                if (bson_iter_init_find(&data_iter, &data_doc, "additional")) {
                    if (bson_iter_type(&data_iter) == BSON_TYPE_DOCUMENT) {
                        bson_t add_doc;
                        const uint8_t* add_buf;
                        uint32_t add_len;
                        
                        bson_iter_document(&data_iter, &add_len, &add_buf);
                        if (bson_init_static(&add_doc, add_buf, add_len)) {
                            // 여기에 필요한 추가 정보 확인 로직 추가
                        }
                    }
                }
            }
        }
    }
    
    // 결과 코드 결정
    if (reason_code != NULL) {
        if (strcmp(reason_code, "AC") == 0) {
            return 1;  // 정답
        } else if (strcmp(reason_code, "CE") == 0) {
            return 2;  // 컴파일 에러
        } else if (strcmp(reason_code, "PD") == 0) {
            // PD는 채점 중(Pending) 상태지만, 실제로는 여러 오류 상태를 포함할 수 있음
            // 따라서 0을 반환하여 HTML 확인 프로세스를 트리거함
            return 0;
        } else if (strcmp(reason_code, "WA") == 0) {
            return 5;  // 오답
        } else if (strcmp(reason_code, "TLE") == 0) {
            return 4;  // 시간 초과
        } else if (strcmp(reason_code, "RTE") == 0 || 
                  strcmp(reason_code, "RE") == 0) {
            return 3;  // 런타임 에러
        }
    }
    
    // 기본값: HTML 확인 필요
    return 0;
}


// WebSocket을 사용한 채점 결과 확인은 복잡하므로, 
// 간단히 HTTP를 통해 결과를 폴링하는 방식 제안
/**
 * Jungol 제출 결과 확인 함수
 * @param submission_id 제출 ID
 * @param xor_key XOR 암호화 키 (x-fp 헤더 값과 동일)
 * @return 채점 결과 코드 (1: 성공, 음수: 오류, 기타: 각종 오류 코드)
 */
int check_submission_result(long submission_id, const char* xor_key) {
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;
    int result_code = -1; // 기본값: 오류/알 수 없음
    
    // 메모리 초기화
    chunk.memory = malloc(4096);  // 충분한 공간 할당
    if (!chunk.memory) {
        printf("메모리 할당 실패\n");
        return result_code;
    }
    chunk.size = 0;
    
    // CURL 초기화
    curl = curl_easy_init();
    if (!curl) {
        printf("CURL 초기화 실패\n");
        free(chunk.memory);
        return result_code;
    }

    // URL 구성 
    char url[256];
    sprintf(url, "https://api-v7.jungol.co.kr/submission/%ld", submission_id);
    
    if (verbose_mode) {
        printf("제출 결과 확인 URL: %s\n", url);
    }
    
    // 요청 설정
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookie_file);
    curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookie_file);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    // 일관된 User-Agent 사용
    curl_easy_setopt(curl, CURLOPT_USERAGENT, 
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/136.0.0.0 Safari/537.36");
    
    // 디버깅을 위한 VERBOSE 모드 (submit_solution과 동일)
    if (verbose_mode) {
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    }
    
    // 헤더 초기화 및 설정 (submit_solution과 동일한 패턴)
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
    headers = curl_slist_append(headers, "Accept: */*");
    headers = curl_slist_append(headers, "Origin: https://jungol.co.kr");
    headers = curl_slist_append(headers, "Sec-Fetch-Site: same-site");
    headers = curl_slist_append(headers, "Sec-Fetch-Mode: cors");
    
    // x-api 헤더 (cached_token 사용)
    if (cached_token[0] != '\0') {
        headers = curl_slist_append(headers, cached_token);
    } else {
        if (verbose_mode) {
            printf("경고: 토큰이 없습니다. 응답이 실패할 수 있습니다.\n");
        }
    }
    
    // x-fp 헤더 (XOR 키)
    char xfp_header[100];
    sprintf(xfp_header, "x-fp: %s", xor_key);
    headers = curl_slist_append(headers, xfp_header);
    
    // auth 쿠키 직접 설정 (필요한 경우)
    char* auth_value = NULL;
    FILE* cookie_fp = fopen(cookie_file, "r");
    if (cookie_fp) {
        char line[1024];
        while (fgets(line, sizeof(line), cookie_fp)) {
            if (strstr(line, "auth")) {
                char* value_start = strstr(line, "auth\t");
                if (value_start) {
                    value_start += 5;  // "auth\t" 길이
                    // 값 끝 찾기
                    char* value_end = strchr(value_start, '\n');
                    if (value_end) {
                        *value_end = '\0';
                        auth_value = strdup(value_start);
                        break;
                    }
                }
            }
        }
        fclose(cookie_fp);
    }
    
    if (auth_value) {
        char auth_cookie[1024];
        sprintf(auth_cookie, "Cookie: auth=%s", auth_value);
        headers = curl_slist_append(headers, auth_cookie);
        free(auth_value);
    }
    
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // 요청 수행
    res = curl_easy_perform(curl);
    
    // HTTP 응답 코드 확인
    long response_code = 0;
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        if (verbose_mode) {
            printf("HTTP 응답 코드: %ld\n", response_code);
        }
    } else {
        fprintf(stderr, "요청 실패: %s\n", curl_easy_strerror(res));
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        free(chunk.memory);
        return result_code;
    }
    
    if (response_code == 200) {
        // 응답 해독
        unsigned char* decrypted = decrypt_response(
            (unsigned char*)chunk.memory, chunk.size, xor_key);
    
        if (decrypted) {
            if (verbose_mode) {
                printf("\n해독된 BSON 문서 구조:\n");
                print_bson_document(decrypted, chunk.size);
            }
            
            // BSON에서 결과 상태 추출
            result_code = parse_submission_result(decrypted, chunk.size);
            
            free(decrypted);
        }
    } else {
        printf("서버 응답 오류: HTTP %ld\n", response_code);
        if (response_code == 401 || response_code == 403) {
            printf("로그인 세션이 만료되었을 수 있습니다. 재로그인이 필요합니다.\n");
        }
    }
    
    // 정리
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(chunk.memory);
    
    return result_code;
}

/**
 * 개선된 응답 해독 함수
 * @return 성공 시 해독된 데이터, 실패 시 NULL
 */
unsigned char* decrypt_response(const unsigned char* data, size_t len, const char* xor_key) {
    if (!data || len == 0 || !xor_key) {
        printf("decrypt_response: 잘못된 입력 파라미터\n");
        return NULL;
    }
    
    printf("응답 원본 (처음 16바이트): ");
    for (size_t i = 0; i < (len < 16 ? len : 16); i++) {
        printf("%02x ", (unsigned char)data[i]);
    }
    printf("\n");
    
    // XOR 키 변환
    size_t xor_key_len = strlen(xor_key) / 2;
    if (xor_key_len == 0) {
        printf("decrypt_response: XOR 키 길이가 0입니다\n");
        return NULL;
    }
    
    unsigned char* key_bytes = (unsigned char*)malloc(xor_key_len);
    if (!key_bytes) {
        printf("decrypt_response: 키 변환을 위한 메모리 할당 실패\n");
        return NULL;
    }
    
    // 16진수 문자열을 바이트 배열로 변환
    for (size_t i = 0; i < xor_key_len; i++) {
        int value;
        if (sscanf(&xor_key[i*2], "%2x", &value) != 1) {
            printf("decrypt_response: 키의 %zu번째 바이트 변환 실패\n", i);
            free(key_bytes);
            return NULL;
        }
        key_bytes[i] = (unsigned char)value;
    }
    
    printf("XOR 키 (처음 8바이트): ");
    for (size_t i = 0; i < (xor_key_len < 8 ? xor_key_len : 8); i++) {
        printf("%02x ", key_bytes[i]);
    }
    printf("\n");
    
    // 암호 해독
    unsigned char* decrypted = (unsigned char*)malloc(len);
    if (!decrypted) {
        printf("decrypt_response: 해독 버퍼 메모리 할당 실패\n");
        free(key_bytes);
        return NULL;
    }
    
    // XOR 복호화 수행
    for (size_t i = 0; i < len; i++) {
        decrypted[i] = data[i] ^ key_bytes[i % xor_key_len];
    }
    
    // BSON 유효성 검사 개선
    if (len >= 4) {
        uint32_t doc_size = 
            (uint32_t)decrypted[0] | 
            ((uint32_t)decrypted[1] << 8) | 
            ((uint32_t)decrypted[2] << 16) | 
            ((uint32_t)decrypted[3] << 24);
            
        printf("해독된 BSON 크기: %u (응답 크기: %zu)\n", doc_size, len);
        
        // 유효한 BSON인지 확인 (크기가 맞는지)
        if (doc_size != len) {
            if (doc_size < len) {
                printf("주의: BSON 문서 크기(%u)가 응답 크기(%zu)보다 작습니다. 유효한 BSON 부분만 처리합니다.\n", 
                       doc_size, len);
            } else {
                printf("경고: BSON 문서 크기(%u)가 응답 크기(%zu)보다 큽니다. 불완전한 BSON일 수 있습니다.\n", 
                       doc_size, len);
            }
        }
    }
    
    // 바이너리 전체 출력 (HEX)
    printf("해독된 데이터 전체 (HEX):\n");
    for (size_t i = 0; i < len; i++) {
        printf("%02x ", decrypted[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");
    
    free(key_bytes);
    return decrypted;
}

// 해독된 BSON 응답에서 제출 ID 추출
long extract_submission_id(const char* decrypted_data, size_t len) {
    // BSON 응답 파싱 (libbson 사용 또는 필요한 부분 수동 추출)
    bson_t b;
    if (!bson_init_static(&b, (const uint8_t*)decrypted_data, len)) {
        return -1;
    }
    
    bson_iter_t iter;
    if (bson_iter_init_find(&iter, &b, "id")) {
        return bson_iter_as_int64(&iter);
    }
    
    return -1;
}

// BSON 객체 생성 및 암호화 함수
unsigned char* create_encrypted_submission(const char* problem_id, const char* lang, const char* code, const char* code_filename, const char* xor_key, size_t* out_len) {
    bson_t *b;
    bson_t *source_doc;
    bson_t sources_array;
    
    // 메인 BSON 문서 생성
    b = bson_new();
    
    // problemId 추가
    int problem_id_num = atoi(problem_id);
    BSON_APPEND_INT32(b, "problemId", problem_id_num);
    
    // language 추가
    BSON_APPEND_UTF8(b, "language", lang);
    
    // source 배열 시작
    bson_append_array_begin(b, "source", -1, &sources_array);
    
    // 소스 코드 문서 생성
    source_doc = bson_new();
    BSON_APPEND_UTF8(source_doc, "name", code_filename);
    BSON_APPEND_UTF8(source_doc, "source", code);
    
    // 배열에 소스 코드 문서 추가
    bson_append_document(&sources_array, "0", -1, source_doc);
    bson_append_array_end(b, &sources_array);
    
    // contestId 추가 (null)
    BSON_APPEND_NULL(b, "contestId");
    
    // BSON을 바이너리로 변환
    const uint8_t *bson_data;
    size_t bson_len;
    bson_data = bson_get_data(b);
    bson_len = b->len;
    
    // XOR 암호화를 위한 메모리 할당
    unsigned char *encrypted = (unsigned char*) malloc(bson_len);
    if (!encrypted) {
        bson_destroy(source_doc);
        bson_destroy(b);
        return NULL;
    }
    
    // XOR 암호화 수행
    size_t xor_key_len = strlen(xor_key) / 2;
    unsigned char *key_bytes = (unsigned char*) malloc(xor_key_len);
    
    // 16진수 문자열 키를 바이트 배열로 변환
    for (size_t i = 0; i < xor_key_len; i++) {
        sscanf(&xor_key[i*2], "%2hhx", &key_bytes[i]);
    }
    
    // XOR 암호화 적용
    for (size_t i = 0; i < bson_len; i++) {
        encrypted[i] = bson_data[i] ^ key_bytes[i % xor_key_len];
    }
    
    free(key_bytes);
    bson_destroy(source_doc);
    bson_destroy(b);
    
    *out_len = bson_len;
    return encrypted;
}

// 메모리에 응답 데이터를 저장하는 콜백 함수
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb,
                                  void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (ptr == NULL) {
        printf("메모리 할당 실패\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

// Jungol에 로그인하는 함수
int jungol_login(const char *username, const char *password) {
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;

    chunk.memory = malloc(1);
    if (!chunk.memory) {
        printf("메모리 할당 실패\n");
        return 0;
    }
    chunk.size = 0;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    if (curl) {
        // 쿠키 파일이 존재한다면 삭제
        remove(cookie_file);

        // 먼저 메인 페이지 방문
        curl_easy_setopt(curl, CURLOPT_URL, "https://jungol.co.kr/");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookie_file);
        curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookie_file);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(
            curl, CURLOPT_USERAGENT,
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
            "(KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36");

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "메인 페이지 요청 실패: %s\n",
                    curl_easy_strerror(res));
            free(chunk.memory);
            curl_easy_cleanup(curl);
            return 0;
        }

        // 로그인 페이지 방문
        free(chunk.memory);
        chunk.memory = malloc(1);
        if (!chunk.memory) {
            printf("메모리 할당 실패\n");
            curl_easy_cleanup(curl);
            return 0;
        }
        chunk.size = 0;

        curl_easy_setopt(curl, CURLOPT_URL, "https://jungol.co.kr/auth/signin");
        
        

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "로그인 페이지 요청 실패: %s\n",
                    curl_easy_strerror(res));
            free(chunk.memory);
            curl_easy_cleanup(curl);
            return 0;
        }

        // 토큰 추출
        char *token_start = strstr(chunk.memory, "token:\"");
        if (!token_start) {
            printf("토큰을 찾을 수 없습니다.\n");
            free(chunk.memory);
            curl_easy_cleanup(curl);
            return 0;
        }

        token_start += 7; // "token:"" 다음 위치로 이동
        char *token_end = strchr(token_start, '"');
        if (!token_end) {
            printf("토큰 끝을 찾을 수 없습니다.\n");
            free(chunk.memory);
            curl_easy_cleanup(curl);
            return 0;
        }

        size_t token_len = token_end - token_start;
        char csrf_token[256] = {0};
        strncpy(csrf_token, token_start, token_len > 255 ? 255 : token_len);
        printf("CSRF 토큰: %s\n", csrf_token);

        // 로그인 POST 요청 (form 형식)
        free(chunk.memory);
        chunk.memory = malloc(1);
        if (!chunk.memory) {
            printf("메모리 할당 실패\n");
            curl_easy_cleanup(curl);
            return 0;
        }
        chunk.size = 0;

        // 폼 데이터 형식으로 로그인
        char post_data[512];
        snprintf(post_data, sizeof(post_data),
                 "username=%s&password=%s&remember=true&_csrf=%s", username,
                 password, csrf_token);

        curl_easy_setopt(curl, CURLOPT_URL,
                         "https://jungol.co.kr/api/v7/auth/signin");
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(
            headers, "Content-Type: application/x-www-form-urlencoded");
        headers = curl_slist_append(headers, "Accept: application/json");
        headers =
            curl_slist_append(headers, "X-Requested-With: XMLHttpRequest");
        headers = curl_slist_append(headers, "Origin: https://jungol.co.kr");
        headers = curl_slist_append(
            headers, "Referer: https://jungol.co.kr/auth/signin");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "로그인 요청 실패: %s\n", curl_easy_strerror(res));
            free(chunk.memory);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            return 0;
        }

        printf("로그인 응답: %s\n", chunk.memory);

        // 응답에서 성공 여부 확인
        if (strstr(chunk.memory, "\"success\"") != NULL ||
            strstr(chunk.memory, "token") != NULL) {
            printf("로그인 성공!\n");

            // 기존 메모리 해제
            free(chunk.memory);
            chunk.memory = malloc(4096);  // 더 큰 버퍼 할당
            if (!chunk.memory) {
                printf("메모리 할당 실패\n");
                curl_slist_free_all(headers);
                curl_easy_cleanup(curl);
                return 0;
            }
            chunk.size = 0;

            curl_slist_free_all(headers);
            headers = NULL;

            // 중요: 기존 핸들 완전히 정리하고 새로 생성
            curl_easy_cleanup(curl);
            curl = curl_easy_init();
            if (!curl) {
                printf("새 curl 핸들 생성 실패\n");
                free(chunk.memory);
                return 0;
            }

            // 필수 옵션 모두 다시 설정
            curl_easy_setopt(curl, CURLOPT_URL, "https://jungol.co.kr/");
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookie_file);
            curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookie_file);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
            curl_easy_setopt(curl, CURLOPT_USERAGENT,
                "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                "(KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36");
            printf("DEBUG: curl easy setopt done\n");
            
            res = curl_easy_perform(curl);                  printf("DEBUG: curl easy perform done\n");
            if (res != CURLE_OK) {
                fprintf(stderr, "홈페이지 확인 실패: %s\n",
                        curl_easy_strerror(res));
                free(chunk.memory);
                curl_easy_cleanup(curl);                    printf("DEBUG: curl easy cleanup done\n");
                return 0;
            }
            printf("DEBUG: homepage check done\n");
            // 세션 확인: 로그인된 페이지에는 로그아웃 버튼이 있음
            if (strstr(chunk.memory, "account") ||
                !strstr(chunk.memory, "person_off")) {
                printf("세션 확인 성공!\n");
                free(chunk.memory);
                curl_easy_cleanup(curl);
                return 1;
            } else {
                printf("세션 확인 실패: 로그인은 성공했지만 세션이 유지되지 "
                       "않습니다.\n");
                free(chunk.memory);
                curl_easy_cleanup(curl);
                return 0;
            }
            printf("DEBUG: session check done\n");
        } else {
            printf("로그인 실패: 성공 응답을 찾을 수 없음\n");
            free(chunk.memory);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            return 0;
        }
    }

    return 0;
}

// 코드를 제출하는 함수 (리팩터링됨)
int submit_solution(const char *problem_id, const char *code, const char *language) {
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;

    // XOR 키 (x-fp 헤더와 동일한 값 사용)
    const char* xor_key = "770cf65b5965d85c4b3c09fce562f3f8";

    chunk.memory = malloc(4096);  // 충분한 공간 할당
    if (!chunk.memory) {
        printf("메모리 할당 실패\n");
        return 0;
    }
    chunk.size = 0;

    curl = curl_easy_init();
    if (!curl) {
        free(chunk.memory);
        return 0;
    }

    // 1단계: 토큰 가져오기 위해 문제 페이지 방문
    char problem_url[256];
    snprintf(problem_url, sizeof(problem_url), "https://jungol.co.kr/problem/%s", problem_id);

    curl_easy_setopt(curl, CURLOPT_URL, problem_url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookie_file);
    curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookie_file);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/136.0.0.0 Safari/537.36");

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "문제 페이지 요청 실패: %s\n", curl_easy_strerror(res));
        free(chunk.memory);
        curl_easy_cleanup(curl);
        return 0;
    }

    // 토큰 디버깅 및 추출
    printf("DEBUG: 페이지 내용 일부 (처음 500자):\n%.*s\n", 
        chunk.size > 500 ? 500 : (int)chunk.size, chunk.memory);
    printf("DEBUG: 'token:' 문자열 존재: %s\n", 
        strstr(chunk.memory, "token:") ? "예" : "아니오");
    printf("DEBUG: '\"token\":' 문자열 존재: %s\n", 
        strstr(chunk.memory, "\"token\":") ? "예" : "아니오");

    // 전체 응답 출력 (디버깅용)
    printf("전체 응답 (16진수): ");
    for (size_t i = 0; i < chunk.size; i++) {
        printf("%02x ", (unsigned char)chunk.memory[i]);
    }
    printf("\n");

    // 토큰 추출
    char *token = extract_token(chunk.memory);
    if (!token) {
        printf("토큰을 찾을 수 없습니다.\n");
        free(chunk.memory);
        curl_easy_cleanup(curl);
        return 0;
    }
    printf("API 토큰: %s\n", token);
    
    // 2단계: 제출 데이터 암호화 준비
    free(chunk.memory);
    chunk.memory = malloc(4096);
    if (!chunk.memory) {
        printf("메모리 할당 실패\n");
        curl_easy_cleanup(curl);
        return 0;
    }
    chunk.size = 0;
    
    // 암호화된 데이터 생성
    size_t encrypted_len = 0;
    unsigned char* encrypted_data = create_encrypted_submission(
        problem_id, language, code, "Main.cpp", xor_key, &encrypted_len);
        
    if (!encrypted_data) {
        printf("암호화 데이터 생성 실패\n");
        free(chunk.memory);
        curl_easy_cleanup(curl);
        return 0;
    }

    // 3단계: 제출 요청 준비
    curl_easy_setopt(curl, CURLOPT_URL, "https://api-v7.jungol.co.kr/submission");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, encrypted_data);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, encrypted_len);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookie_file);
    curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookie_file);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, 
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/136.0.0.0 Safari/537.36");

    // 디버깅을 위한 VERBOSE 모드 활성화
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    // 헤더 초기화 및 설정
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
    headers = curl_slist_append(headers, "Accept: */*");
    headers = curl_slist_append(headers, "Origin: https://jungol.co.kr");
    headers = curl_slist_append(headers, "Sec-Fetch-Site: same-site");
    headers = curl_slist_append(headers, "Sec-Fetch-Mode: cors");

    // x-api와 x-fp 헤더 추가
    char xapi_header[4096];
    sprintf(xapi_header, "x-api: %s", token);
    sprintf(cached_token, "x-api: %s", token);
    headers = curl_slist_append(headers, xapi_header);
    
    char xfp_header[100];
    sprintf(xfp_header, "x-fp: %s", xor_key);
    headers = curl_slist_append(headers, xfp_header);

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // 디버깅 출력
    printf("제출 URL: https://api-v7.jungol.co.kr/submission\n");
    printf("x-api 헤더: %s\n", xapi_header);
    printf("x-fp 헤더: %s\n", xfp_header);
    
    // auth 쿠키 직접 설정 (필요한 경우)
    char auth_cookie[1024];
    char* auth_value = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpZCI6NDQ2NjMsInYiOjUsImlhdCI6MTc2MjM5Mjk3NzQ4OH0.RhcgxhKfzIumsu3NNqDIYH-45tv8IEEqMPdUCut0EOQ";
    sprintf(auth_cookie, "Cookie: auth=%s", auth_value);
    headers = curl_slist_append(headers, auth_cookie);
    
    // 쿠키 파일 내용 확인
    printf("DEBUG: 쿠키 파일 내용:\n");
    FILE* fp = fopen(cookie_file, "r");
    if (fp) {
        char line[256];
        int auth_found = 0;
        while (fgets(line, sizeof(line), fp)) {
            printf("%s", line);
            if (strstr(line, "auth")) {
                auth_found = 1;
            }
        }
        fclose(fp);
        
        if (!auth_found) {
            printf("경고: auth 쿠키를 찾을 수 없습니다!\n");
        }
    }

    // 4단계: 요청 수행
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "제출 요청 실패: %s\n", curl_easy_strerror(res));
        free(encrypted_data);
        free(chunk.memory);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return 0;
    }

    // 메모리 해제
    free(encrypted_data);

    // 로그인 상태 확인
    if (!check_login_status()) {
        printf("로그인 세션이 만료되었습니다. 다시 로그인합니다.\n");
        if (!jungol_login("felix6631", "crafting6630")) {
            printf("재로그인 실패\n");
            free(chunk.memory);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            return 0;
        }
    }

    // 응답 디버깅
    printf("제출 응답: ");
    for (size_t i = 0; i < chunk.size && i < 100; i++) {
        printf("%c", chunk.memory[i]);
        // 문자가 표시되지 않는 경우 코드값도 출력
        if (chunk.memory[i] < 32 || chunk.memory[i] > 126) {
            printf("(\\x%02x)", (unsigned char)chunk.memory[i]);
        }
    }
    printf("\n");

    // 성공 여부 확인
    int response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (res == CURLE_OK && response_code == 200) {
        printf("HTTP 응답코드: %d\n", (int)response_code);
        
        // 응답 해독
        unsigned char* decrypted = decrypt_response(
            (unsigned char*)chunk.memory, chunk.size, xor_key);
        
        printf("decrypt 성공 여부: %d\n", decrypted != NULL);
        
        if (decrypted) {
            // 제출 ID 추출 시도
            long submission_id = -1;
            
            // BSON 파싱
            bson_t b;
            if (bson_init_static(&b, decrypted, chunk.size)) {
                bson_iter_t iter;
                
                // 먼저 'id' 필드 찾기 시도
                if (bson_iter_init_find(&iter, &b, "id")) {
                    submission_id = bson_iter_as_int64(&iter);
                    printf("'id' 필드에서 제출 ID 찾음: %ld\n", submission_id);
                } 
                // 'id' 필드가 없으면 'data' 필드 시도
                else if (bson_iter_init_find(&iter, &b, "data")) {
                    // 필드 타입 확인
                    bson_type_t type = bson_iter_type(&iter);
                    if (type == BSON_TYPE_INT32) {
                        submission_id = bson_iter_int32(&iter);
                        printf("'data' 필드에서 제출 ID 찾음: %ld\n", submission_id);
                    } else if (type == BSON_TYPE_INT64) {
                        submission_id = bson_iter_int64(&iter);
                        printf("'data' 필드에서 제출 ID 찾음: %ld\n", submission_id);
                    } else {
                        printf("'data' 필드가 정수 타입이 아닙니다 (타입: %d)\n", type);
                    }
                } else {
                    printf("제출 ID를 찾을 수 없습니다\n");
                    
                    // 디버깅: 문서 내 모든 필드 출력
                    printf("BSON 문서 내 필드들:\n");
                    bson_iter_t all_iter;
                    bson_iter_init(&all_iter, &b);
                    while (bson_iter_next(&all_iter)) {
                        printf("  필드: %s (타입: %d)\n", 
                            bson_iter_key(&all_iter), 
                            bson_iter_type(&all_iter));
                    }
                }
            } else {
                printf("BSON 초기화 실패\n");
            }
            
            free(decrypted);
            
            // 결과 반환
            if (submission_id > 0) {
                printf("제출 성공! 제출 ID: %ld\n", submission_id);
                
                // 채점 결과 기다리기
                int result = wait_for_submission_result(submission_id, xor_key);
                
                // 결과 설명 출력
                print_result_description(result);
                
                return 1;
            }
        }
    }
    return 0;
}
// x-api 토큰을 HTML에서 추출하는 함수
char* extract_token(const char* html) {
    static char token[4096]; // 정적 버퍼
    
    // 여러 가능한 패턴 시도
    const char* patterns[] = {
        "token:\"", 
        "\"token\":\"",
        "token=\"",
        "token='",
        "var token = \"",
        "__INITIAL_DATA__ = {token: \""
    };
    int num_patterns = sizeof(patterns) / sizeof(patterns[0]);
    
    for (int i = 0; i < num_patterns; i++) {
        const char* token_marker = patterns[i];
        char* token_start = strstr(html, token_marker);
        
        if (token_start) {
            // 토큰 시작 위치 이동
            token_start += strlen(token_marker);
            
            // 토큰 끝 위치 찾기 (다음 큰따옴표)
            char* token_end = strchr(token_start, '"');
            if (!token_end) token_end = strchr(token_start, '\'');
            if (!token_end) continue; // 토큰 끝을 찾지 못함, 다음 패턴 시도
            
            // 토큰 복사
            size_t token_len = token_end - token_start;
            if (token_len >= sizeof(token) - 1) token_len = sizeof(token) - 1;
            
            strncpy(token, token_start, token_len);
            token[token_len] = '\0';
            
            printf("DEBUG: 패턴 '%s'로 토큰 찾음: %s\n", token_marker, token);
            return token;
        }
    }
    
    // 추가적인 검색: Jungol의 SvelteKit 데이터 패턴 시도
    const char* sveltekit_data = "data:[{\"type\":\"data\",\"data\":{";
    char* data_start = strstr(html, sveltekit_data);
    if (data_start) {
        data_start += strlen(sveltekit_data);
        char* token_field = strstr(data_start, "\"token\":\"");
        if (token_field) {
            token_field += 9; // "token":" 길이
            char* token_end = strchr(token_field, '"');
            if (token_end) {
                size_t token_len = token_end - token_field;
                if (token_len >= sizeof(token) - 1) token_len = sizeof(token) - 1;
                
                strncpy(token, token_field, token_len);
                token[token_len] = '\0';
                
                printf("DEBUG: SvelteKit 데이터에서 토큰 찾음: %s\n", token);
                return token;
            }
        }
    }
    
    printf("DEBUG: 토큰을 찾을 수 없음, 페이지 내용 검사 필요\n");
    return NULL;
}

// 서버 응답이 BSON이 아닐 경우를 대비한 대체 파싱 함수
long extract_submission_id_fallback(const char* data, size_t len) {
    // 1. 텍스트 응답에서 ID 찾기
    const char* id_marker = "\"id\":";
    char* id_pos = strstr(data, id_marker);
    
    if (id_pos) {
        // "id": 다음 위치로 이동
        id_pos += strlen(id_marker);
        
        // 숫자 부분 읽기
        long id = 0;
        if (sscanf(id_pos, "%ld", &id) == 1) {
            return id;
        }
    }
    
    // 2. 간단한 숫자 응답인 경우 (응답 자체가 ID일 수 있음)
    if (len < 20) {  // 작은 크기의 응답
        char temp[32] = {0};
        strncpy(temp, data, len < 31 ? len : 31);
        
        // 숫자인지 확인
        char* endptr;
        long id = strtol(temp, &endptr, 10);
        if (*endptr == '\0' || *endptr == '\n') {
            return id;  // 전체 문자열이 숫자임
        }
    }
    
    return -1;  // ID를 찾을 수 없음
}

// 세션 모니터 - 로그인 상태인지 확인
int check_login_status() {
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;

    chunk.memory = malloc(1);
    if (!chunk.memory) {
        printf("메모리 할당 실패\n");
        return 0;
    }
    chunk.size = 0;

    curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "https://jungol.co.kr/account/my");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookie_file);
        curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookie_file);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(
            curl, CURLOPT_USERAGENT,
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
            "(KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36");

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "계정 페이지 요청 실패: %s\n",
                    curl_easy_strerror(res));
            free(chunk.memory);
            curl_easy_cleanup(curl);
            return 0;
        }

        // 로그인 상태 확인
        if (strstr(chunk.memory, "account") ||
            !strstr(chunk.memory, "person_off")) {
            printf("로그인 상태입니다.\n");
            free(chunk.memory);
            curl_easy_cleanup(curl);
            return 1;
        } else {
            printf("로그인 상태가 아닙니다.\n");
            free(chunk.memory);
            curl_easy_cleanup(curl);
            return 0;
        }
    }
    printf("DEBUG: return null curl\n");
    return 0;
}

// 참고: 이 함수는 코드 파일을 읽어 JSON 코드 문자열로 변환합니다
char *prepare_code_json(const char *file_path) {
    FILE *fp = fopen(file_path, "r");
    if (!fp) {
        printf("파일을 열 수 없습니다: %s\n", file_path);
        return NULL;
    }

    // 파일 크기 확인
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // 파일 읽기
    char *code = (char *)malloc(file_size + 1);
    if (!code) {
        printf("메모리 할당 실패\n");
        fclose(fp);
        return NULL;
    }

    size_t read_size = fread(code, 1, file_size, fp);
    fclose(fp);

    if (read_size != file_size) {
        printf("파일 읽기 실패\n");
        free(code);
        return NULL;
    }

    code[file_size] = '\0';

    // JSON 문자열 형식으로 변환 (따옴표, 개행 문자 등 이스케이프)
    size_t json_size = file_size * 2 + 3; // 충분한 크기 할당
    char *json_code = (char *)malloc(json_size);
    if (!json_code) {
        printf("메모리 할당 실패\n");
        free(code);
        return NULL;
    }
    // 시작 지점
    size_t pos = 0;

    // 이스케이프 처리
    for (size_t i = 0; i < file_size; i++) {
        if (pos >= json_size - 3) {
            // 버퍼 크기가 부족한 경우 확장
            json_size *= 2;
            char *new_buf = (char *)realloc(json_code, json_size);
            if (!new_buf) {
                printf("메모리 재할당 실패\n");
                free(json_code);
                free(code);
                return NULL;
            }
            json_code = new_buf;
        }
        json_code[pos++] = code[i];
    }

    // NULL 문자 추가
    json_code[pos] = '\0';

    free(code);
    return json_code;
}



int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("사용법: %s <사용자이름> <비밀번호> <문제ID> [코드파일경로]\n",
               argv[0]);
        printf("언어는 코드 파일 확장자에 따라 자동 결정됩니다.\n");
        printf("지원하는 확장자: .c (C), .cpp (C++), .java (Java), .py "
               "(Python3)\n");
        return 1;
    }

    const char *username = argv[1];
    const char *password = argv[2];
    const char *problem_id = argv[3];
    const char *code_file = NULL;

    if (argc >= 5) {
        code_file = argv[4];
    }

    // 로그인
    if (!jungol_login(username, password)) {
        printf("로그인에 실패했습니다. 프로그램을 종료합니다.\n");
        return 1;
    }
    printf("DEBUG: jungol_login finished\n");
    // 로그인 상태 확인
    if (!check_login_status()) {
        printf("로그인 상태 확인에 실패했습니다. 프로그램을 종료합니다.\n");
        return 1;
    }

    printf("로그인 상태 확인 성공! 코드를 준비합니다...\n");

    // 코드 준비 및 언어 결정
    char *json_code = NULL;
    const char *language = NULL;

    if (code_file) {
        // 파일 확장자로 언어 결정
        const char *ext = strrchr(code_file, '.');
        if (!ext) {
            printf("파일 확장자를 확인할 수 없습니다. 기본값으로 C를 "
                   "사용합니다.\n");
            language = "C";
        } else {
            if (strcmp(ext, ".c") == 0) {
                language = "C";
            } else if (strcmp(ext, ".cpp") == 0 || strcmp(ext, ".cc") == 0) {
                language = "CPP";
            } else if (strcmp(ext, ".java") == 0) {
                language = "JAVA";
            } else if (strcmp(ext, ".py") == 0) {
                language = "PYTHON3";
            } else {
                printf("지원되지 않는 파일 확장자입니다: %s\n", ext);
                printf("기본값으로 C를 사용합니다.\n");
                language = "C";
            }
        }

        // 코드 파일 읽기 및 JSON 문자열로 변환
        json_code = prepare_code_json(code_file);
        if (!json_code) {
            printf("코드 준비 실패\n");
            return 1;
        }
    } else {
        // 간단한 예제 코드 사용
        language = "CPP"; // 기본 언어
        json_code = strdup(
            "#include <iostream>\n\n"
            "int main() {\n"
            "    int a, b;\n"
            "    std::cin >> a >> b;\n"
            "    std::cout << (a + b) << '\\n';\n"
            "    return 0;\n"
            "}"
        );
        if (!json_code) {
            printf("메모리 할당 실패 (예제 코드)\n");
            return 1;
        }
    }

    printf("선택된 언어: %s\n", language);

    // 코드 제출
    printf("문제 %s에 코드를 제출합니다...\n", problem_id);

    if (!submit_solution(problem_id, json_code, language)) {
        printf("코드 제출에 실패했습니다.\n");
        free(json_code);
        return 1;
    }

    printf("코드가 성공적으로 제출되었습니다!\n");

    // 메모리 해제
    free(json_code);

    return 0;
}