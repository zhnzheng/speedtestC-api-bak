#ifndef _SPEEDTEST_
#define _SPEEDTEST_

#include <sys/time.h>
#include <pthread.h>

#define COLOR_NONE          "\033[0m"
#define COLOR_BLACK         "\033[0;30m"
#define COLOR_BLUE          "\033[0;34m"
#define COLOR_GREEN         "\033[0;32m"
#define COLOR_CYAN          "\033[0;36m"
#define COLOR_RED           "\033[0;31m"
#define COLOR_YELLOW        "\033[1;33m"
#define COLOR_WHITE         "\033[1;37m"

//#define SPEEDTEST_FILE_DUMP_SUPPORT 1
#define SPEEDTEST_DEBUG 1

#define SPEED_TEST_FILE_SIZE 31625365
#define BUFFER_SIZE 1500

#ifdef SPEEDTEST_DEBUG
#define ST_DBG(fmt, args...) \
    do { \
        printf(COLOR_GREEN "[DBG]:%s[%d]: " COLOR_NONE, __FUNCTION__,__LINE__); \
        printf(fmt, ##args); \
    }while(0)
#else
#define ST_DBG(fmt, args...) 
#endif

#define ST_WARN(fmt, args...) \
    do { \
        printf(COLOR_YELLOW "[WARN]:%s[%d]: " COLOR_NONE, __FUNCTION__,__LINE__); \
        printf(fmt, ##args); \
    }while(0)

#define ST_INFO(fmt, args...) \
    do { \
        printf("[INFO]:%s[%d]: ", __FUNCTION__,__LINE__); \
        printf(fmt, ##args); \
    }while(0)

#define ST_ERR(fmt, args...) \
    do { \
        printf(COLOR_RED "[ERR]:%s[%d]: " COLOR_NONE, __FUNCTION__,__LINE__); \
        printf(fmt, ##args); \
    }while(0)

/** Memory Safe Free */
#define ST_SAFE_FREE(p) do { if (NULL != (p)){ free(p); (p) = NULL; } }while(0)

extern SPEEDTESTCONFIG_T *speedTestConfig;
extern unsigned totalDownloadTestCount;
extern unsigned long totalTransfered;
extern unsigned long totalToBeTransfered;

typedef struct thread_args {
    pthread_t tid;
    char *url;
    unsigned int testCount;
    unsigned long transferedBytes;
    float elapsedSecs;
    int threadEnd; /* 0:start 1:end */
} THREADARGS_T;

typedef struct thread_mng
{
    pthread_t tidCalcResult;
    THREADARGS_T *downloadArgs;
    THREADARGS_T *uploadArgs;
} THREADMNG_T;

float getElapsedTime(struct timeval tval_start);
char *strdup(const char *str);
int getRunStatus(void);
int getStopStatus(void);

#endif
