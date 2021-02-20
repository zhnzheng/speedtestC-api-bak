#include "SpeedtestUploadTest.h"
#include "SpeedtestConfig.h"
#include "SpeedtestServers.h"
#include "Speedtest.h"
#include "http.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h> 

extern int downloadTestEnd;

static void __appendTimestamp(const char *url, char *buff, int buff_len)
{
    char delim = '?';
    char *p = strchr(url, '?');

    if (p)
        delim = '&';
    snprintf(buff, buff_len, "%s%cx=%llu", url, delim, (unsigned long long)time(NULL));
}

static void *__uploadThread(void *arg)
{
    /* Testing upload... */
    THREADARGS_T *threadConfig = (THREADARGS_T *)arg;
    char alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char buffer[BUFFER_SIZE] = {0};
    int i, size, uploadTimes, retry;
    struct timeval tval_start;
    unsigned long totalTransfered = 0;
    char uploadUrl[1024];
    sock_t sockId = BAD_SOCKID;

    pthread_detach(pthread_self());

    while(downloadTestEnd == 0)
    {
        usleep(1000 * 20);
    }

    /* Build the random buffer */
    srand(time(NULL));
    for(i = 0; i < BUFFER_SIZE; i++)
    {
        buffer[i] = alphabet[rand() % ARRAY_SIZE(alphabet)];
    }

    gettimeofday(&tval_start, NULL);
    uploadTimes = 0;
    for (i = 0; i < threadConfig->testCount; i++)
    {
        __appendTimestamp(threadConfig->url, uploadUrl, sizeof(uploadUrl));
        /* FIXME: totalToBeTransfered should be readonly while the upload thread is running */
        totalTransfered = totalToBeTransfered;
        for(retry = 0; retry < 3; retry++)
        {
            sockId = httpPutRequestSocket(uploadUrl, totalToBeTransfered, 1);
            if(sockId != BAD_SOCKID)
            {
                break;
            }
        }
        
        if(sockId == BAD_SOCKID)
        {
            ST_ERR("Unable to open socket for Upload!");
            goto EXIT;
        }

#ifdef SPEEDTEST_FILE_DUMP_SUPPORT
        while(totalTransfered != 0)
#else
        while(totalTransfered != 0 && getStopStatus() == 0)
#endif
        {
            uploadTimes++;
            if (totalTransfered > BUFFER_SIZE)
            {
                size = httpSend(sockId, buffer, BUFFER_SIZE);
            }
            else
            {
                buffer[totalTransfered - 1] = '\n'; /* Indicate terminated */
                size = httpSend(sockId, buffer, totalTransfered);
            }

            threadConfig->transferedBytes += size;

            if(uploadTimes % 68 == 0)
                threadConfig->elapsedSecs = getElapsedTime(tval_start);
                
            totalTransfered -= size;
        }

        /* Cleanup */
        httpClose(sockId);
    }

EXIT:
    threadConfig->threadEnd = 1;
    return NULL;
}

void testUpload(THREADMNG_T *threadMng, const char *url)
{
    size_t numOfThreads = speedTestConfig->uploadThreadConfig.threadsCount;
    THREADARGS_T *param = threadMng->uploadArgs;
    int i;
    
    for (i = 0; i < numOfThreads; i++)
    {
        /* Initializing some parameters */
        param[i].threadEnd = 0;
        param[i].testCount =  speedTestConfig->uploadThreadConfig.length;
        if (param[i].testCount == 0)
        {
            /* At least three test should be run */
            param[i].testCount = 3;
        }
        param[i].url = strdup(url);
        if (param[i].url)
        {
            pthread_create(&param[i].tid, NULL, &__uploadThread, &param[i]);
        }
    }

    return;
}
