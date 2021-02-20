#include "SpeedtestDownloadTest.h"
#include "SpeedtestConfig.h"
#include "SpeedtestServers.h"
#include "Speedtest.h"
#include "http.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>

static void *__downloadThread(void *arg)
{
    THREADARGS_T *threadConfig = (THREADARGS_T*)arg;
    int testNum, recvTimes;
    int i;
    sock_t sockId = BAD_SOCKID;
    char buffer[BUFFER_SIZE] = {0};
    struct timeval tval_start;

    pthread_detach(pthread_self());

    gettimeofday(&tval_start, NULL);
    
    recvTimes = 0;
    for (testNum = 0; testNum < threadConfig->testCount; testNum++)
    {
        int size = -1;
        
        for(i = 0; i < 3; i++)
        {
            sockId = httpGetRequestSocket(threadConfig->url, 1);
            if(sockId != BAD_SOCKID)
            {
                break;
            }
        }
        
        if(sockId == BAD_SOCKID)
        {
            fprintf(stderr, "Unable to open socket for Download!");
            goto EXIT;
        }

#ifdef SPEEDTEST_FILE_DUMP_SUPPORT
        while(size != 0)
#else
        while(size != 0 && getStopStatus() == 0)
#endif
        {
            recvTimes++;
            size = httpRecv(sockId, buffer, BUFFER_SIZE);
            if (size != -1)
            {
                threadConfig->transferedBytes += size;
            }

            if(recvTimes % 68 == 0)
                threadConfig->elapsedSecs = getElapsedTime(tval_start);
        }
        
        httpClose(sockId);
    }

EXIT:
    threadConfig->threadEnd = 1;
    return NULL;
}

void testDownload(THREADMNG_T *threadMng, const char *url)
{
    size_t numOfThreads = speedTestConfig->downloadThreadConfig.threadsCount;
    THREADARGS_T *param = threadMng->downloadArgs;
    int i;

    /* Initialize and start threads */
    for (i = 0; i < numOfThreads; i++)
    {
        param[i].threadEnd = 0;
        param[i].testCount = totalDownloadTestCount / numOfThreads;
        if (param[i].testCount == 0)
        {
            /* At least one test should be run */
            param[i].testCount = 1;
        }
        
        param[i].url = strdup(url);
        if (param[i].url)
        {
            pthread_create(&param[i].tid, NULL, &__downloadThread, &param[i]);
        }
    }
    
    return;
}
