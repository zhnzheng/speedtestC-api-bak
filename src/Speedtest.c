/*
	Main program.

	Michał Obrembski (byku@byku.com.pl)
*/
#include "http.h"
#include "SpeedtestConfig.h"
#include "SpeedtestServers.h"
#include "Speedtest.h"
#include "SpeedtestLatencyTest.h"
#include "SpeedtestDownloadTest.h"
#include "SpeedtestUploadTest.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <sys/time.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>

#define SPEEDTEST_MAX_RETRY_TIMES 10
#define SPEEDTEST_LONGEST_TEST_TIME 60 //second

// Global variables
#ifdef SPEEDTEST_FILE_DUMP_SUPPORT
#define SPEEDTEST_STATUS_FILE "/var/speedtest_status"
#define SPEEDTEST_DOWNLOAD_SPEED_FILE "/var/speedtest_download"
#define SPEEDTEST_UPLOAD_SPEED_FILE "/var/speedtest_uploadload"

typedef enum {
    SPEEDTEST_STATUS_DL_RUNNING = 0,
    SPEEDTEST_STATUS_UL_RUNNING,
    SPEEDTEST_STATUS_STOP,
    SPEEDTEST_STATUS_FILE_NO_FIND,
    SPEEDTEST_STATUS_UNKNOWN,
}SPEEDTEST_STATUS;

FILE *fpStatus;
FILE *fpDownloadSpeed;
FILE *fpUploadSpeed;
#else
static int speedTestProgress = 2; /* 0:download running 1:upload running 2:all finish */
static int runStatus = 0; /* 0:finish 1:running */
static int stopStatus = 0; /* 0:not stop 1:stop */
#endif

SPEEDTESTSERVER_T **serverList;
THREADMNG_T ThreadMng;
static char *downloadUrl;
static char *tmpUrl;
static char *uploadUrl;
static char *latencyUrl;
unsigned long totalTransfered;
unsigned long totalToBeTransfered;
unsigned totalDownloadTestCount;
static int serverCount;
static int randomizeBestServers;
static int lowestLatencyServers;
int downloadTestEnd; /* 0:not start or start 1:end */
int uploadTestEnd;  /* 0:not start or start 1:end */
static float downloadSpeed; /* global download speed */
static float uploadSpeed; /* global upload speed */

// strdup isnt a C99 function, so we need it to define itself
char *strdup(const char *str)
{
    int n = strlen(str) + 1;
    char *dup = malloc(n);
    if(dup)
    {
        strcpy(dup, str);
    }
    return dup;
}

int sortServersDistance(SPEEDTESTSERVER_T **srv1, SPEEDTESTSERVER_T **srv2)
{
    return((*srv1)->distance - (*srv2)->distance);
}

int sortServersLatency(SPEEDTESTSERVER_T **srv1, SPEEDTESTSERVER_T **srv2)
{
    return (*srv1)->latency - (*srv2)->latency;
}

float getElapsedTime(struct timeval tval_start) {
    struct timeval tval_end, tval_diff;
    gettimeofday(&tval_end, NULL);
    tval_diff.tv_sec = tval_end.tv_sec - tval_start.tv_sec;
    tval_diff.tv_usec = tval_end.tv_usec - tval_start.tv_usec;
    if(tval_diff.tv_usec < 0)
    {
        --tval_diff.tv_sec;
        tval_diff.tv_usec += 1000000;
    }
    return (float)tval_diff.tv_sec + (float)tval_diff.tv_usec / 1000000;
}

#if 0
void parseCmdLine(int argc, char **argv) {
    int i;
    for(i=1; i<argc; i++)
    {
        if(strcmp("--help", argv[i]) == 0 || strcmp("-h", argv[i]) == 0)
        {
            printf("Usage (options are case sensitive):\n\
            \t--help - Show this help.\n\
            \t--server URL - use server URL, don't read config.\n\
            \t--upsize SIZE - use upload size of SIZE bytes.\n\
            \t--downtimes TIMES - how many times repeat download test.\n\
            \t\tSingle download test is downloading 30MB file.\n\
            \t--lowestlatency NUMBER - pick server with lowest latency\n\
            \t\tamong NUMBER closest\n\
            \t--randomize NUMBER - select random server among NUMBER best\n\
            \t\tCan be combined with --lowestlatency\n\
            \nDefault action: Get server from Speedtest.NET infrastructure\n\
            and test download with 30MB download size and 1MB upload size.\n");
            exit(1);
        }
        if(strcmp("--server", argv[i]) == 0)
        {
            downloadUrl = malloc(sizeof(char) * strlen(argv[i+1]) + 1);
            strcpy(downloadUrl, argv[i + 1]);
        }
        if(strcmp("--upsize", argv[i]) == 0)
        {
            totalToBeTransfered = strtoul(argv[i + 1], NULL, 10);
        }
        if(strcmp("--downtimes", argv[i]) == 0)
        {
            totalDownloadTestCount = strtoul(argv[i + 1], NULL, 10);
        }
        if(strcmp("--randomize", argv[i]) == 0)
        {
            randomizeBestServers = strtoul(argv[i + 1], NULL, 10);
        }
        if(strcmp("--lowestlatency", argv[i]) == 0)
        {
            lowestLatencyServers = strtoul(argv[i + 1], NULL, 10);
        }
    }
}
#endif

void freeMem()
{
    ST_SAFE_FREE(latencyUrl);
    ST_SAFE_FREE(downloadUrl);
    ST_SAFE_FREE(uploadUrl);
    ST_SAFE_FREE(serverList);
    ST_SAFE_FREE(speedTestConfig);
}

int getBestServer()
{
    int i;
    size_t selectedServer = 0;
    char bufferTmp[64] = {0};
    char serverUrl[4][64] =
    {
        {"://www.speedtest.net/speedtest-servers-static.php"},
        {"://c.speedtest.net/speedtest-servers-static.php"},
        {"://www.speedtest.net/speedtest-servers.php"},
        {"://c.speedtest.net/speedtest-servers.php"}
    };

    for(i = 0; i < SPEEDTEST_MAX_RETRY_TIMES; i++)
    {
        speedTestConfig = getConfig();
        if(speedTestConfig != NULL)
        {
            break;
        }
    }

    if (speedTestConfig == NULL)
    {
        ST_ERR("Get config fail\n");
        freeMem();
        return -1;
    }
    
    ST_DBG("Retry %d times to get config.\n", i);
    ST_DBG("Your IP: %s And ISP: %s\n",
        speedTestConfig->ip, speedTestConfig->isp);
    ST_DBG("Lat: %f Lon: %f\n", speedTestConfig->lat, speedTestConfig->lon);

    for(i = 0; i < SPEEDTEST_MAX_RETRY_TIMES - 5; i++)
    {
        for(i = 0; i < 4; i++)
        {
            sprintf(bufferTmp, "%s%s", URL_PROTOCOL, serverUrl[i]);
            serverList = getServers(&serverCount, bufferTmp);
            if(serverCount != 0 && serverList != NULL)
            {
                break;
            }
        }
        
        if(serverCount != 0 && serverList != NULL)
        {
            break;
        }
    }
    
    if(serverList == NULL || serverCount == 0)
    {
        ST_ERR("Get servers fail\n");
        freeMem();
        return -1;
    }

    ST_DBG("Retry %d times to get server.\n", i);
    ST_DBG("Grabbed %d servers\n", serverCount);
  
    for(i = 0; i < serverCount; i++)
    {
        serverList[i]->distance = haversineDistance(speedTestConfig->lat,
            speedTestConfig->lon,
            serverList[i]->lat,
            serverList[i]->lon);
    }

    qsort(serverList, serverCount, sizeof(SPEEDTESTSERVER_T *),
        (int (*)(const void *,const void *)) sortServersDistance);

    if (lowestLatencyServers != 0)
    {
        int debug = 1;
        if (lowestLatencyServers < 0)
        {
            lowestLatencyServers = -lowestLatencyServers;
        }
        
        ST_DBG("Testing closest %d servers for latency\n", lowestLatencyServers);
        // for LARGE numbers of servers could do this in parallel
        // (but best not to pester them, maybe limit max number??)
        for(i = 0; i < lowestLatencyServers; i++)
        {
            latencyUrl = getLatencyUrl(serverList[i]->url);
            if(latencyUrl == NULL)
            {
                serverList[i]->latency = LATENCY_URL_ERROR;
                continue;
            }
            
            serverList[i]->latency = getLatency(latencyUrl);
            ST_SAFE_FREE(latencyUrl);
        }

        /* perform secondary sort on latency */
        qsort(serverList, lowestLatencyServers, sizeof(SPEEDTESTSERVER_T *),
              (int (*)(const void *,const void *)) sortServersLatency);

        if (debug)
        {
            ST_DBG("--------------------------------------------------------------------------------\n");
            for(i = 0; i < lowestLatencyServers; i++)
            {
                ST_DBG("%-30.30s %-20.20s Dist: %3ld km Latency: %ld %s\n",
                       serverList[i]->sponsor, serverList[i]->name,
                       serverList[i]->distance, serverList[i]->latency, LATENCY_UNITS);
            }
            ST_DBG("--------------------------------------------------------------------------------\n");
        }

        if (randomizeBestServers >= lowestLatencyServers)
            randomizeBestServers = lowestLatencyServers / 2;
    }
    
    if (randomizeBestServers > 1)
    {
        ST_DBG("Randomizing selection of %d best servers...\n", randomizeBestServers);
        srand(time(NULL));
        selectedServer = rand() % randomizeBestServers;
    }

    ST_DBG("Best Server URL: %s\n\t\t\t  Name: %s Country: %s Sponsor: %s Dist: %ld km\n",
        serverList[selectedServer]->url, serverList[selectedServer]->name, serverList[selectedServer]->country,
        serverList[selectedServer]->sponsor, serverList[selectedServer]->distance);
    downloadUrl = getServerDownloadUrl(serverList[selectedServer]->url);
    if(downloadUrl == NULL)
    {
        ST_ERR("getServerDownloadUrl fail, downloadUrl is null\n");
        for(i = 0; i < serverCount; i++)
        {
            ST_SAFE_FREE(serverList[i]->url);
            ST_SAFE_FREE(serverList[i]->name);
            ST_SAFE_FREE(serverList[i]->sponsor);
            ST_SAFE_FREE(serverList[i]->country);
            ST_SAFE_FREE(serverList[i]);
        }
        freeMem();
        return -1;
    }
    
    uploadUrl = malloc(sizeof(char) * strlen(serverList[selectedServer]->url) + 1);
    if(uploadUrl == NULL)
    {
        ST_ERR("malloc fail\n");
        for(i = 0; i < serverCount; i++)
        {
            ST_SAFE_FREE(serverList[i]->url);
            ST_SAFE_FREE(serverList[i]->name);
            ST_SAFE_FREE(serverList[i]->sponsor);
            ST_SAFE_FREE(serverList[i]->country);
            ST_SAFE_FREE(serverList[i]);
        }
        freeMem();
        return -1;
    }

    strcpy(uploadUrl, serverList[selectedServer]->url);

    if (lowestLatencyServers)        /* avoid getting latency twice! */
        ST_DBG("Latency: %ld %s\n",
               serverList[selectedServer]->latency, LATENCY_UNITS);

    for(i = 0; i < serverCount; i++)
    {
        ST_SAFE_FREE(serverList[i]->url);
        ST_SAFE_FREE(serverList[i]->name);
        ST_SAFE_FREE(serverList[i]->sponsor);
        ST_SAFE_FREE(serverList[i]->country);
        ST_SAFE_FREE(serverList[i]);
    }

    return 0;
}

static int getUserDefinedServer()
{
    /* When user specify server URL, then we're not downloading config,
    so we need to specify thread count */
    speedTestConfig = malloc(sizeof(struct speedtestConfig));
    speedTestConfig->downloadThreadConfig.threadsCount = 4;
    speedTestConfig->uploadThreadConfig.threadsCount = 2;
    speedTestConfig->uploadThreadConfig.length = 3;

    uploadUrl = downloadUrl;
    tmpUrl = malloc(sizeof(char) * strlen(downloadUrl) + 1);
    if(tmpUrl == NULL)
    {
        ST_ERR("malloc fail\n");
        freeMem();
        return -1;
    }

    strcpy(tmpUrl, downloadUrl);
    downloadUrl = getServerDownloadUrl(tmpUrl);
    if(downloadUrl == NULL)
    {
        ST_ERR("getServerDownloadUrl fail, downloadUrl is null\n");
        ST_SAFE_FREE(tmpUrl);
        freeMem();
        return -1;
    }

    ST_SAFE_FREE(tmpUrl);
    return 0;
}

#ifdef SPEEDTEST_FILE_DUMP_SUPPORT
static int isFileExist(char *fileName)
{
	struct stat status;

	if ( stat(fileName, &status) < 0)
		return 0;

	return 1;
}

static pid_t findPidByName(char* pidName)
{
	DIR *dir;
	struct dirent *next;
	pid_t pid = -1;
    pid_t pidSelf = -1;
	
	if ( strcmp(pidName, "init")==0)
		return 1;
	
	dir = opendir("/proc");
	if (!dir)
    {
		ST_DBG("Cannot open /proc");
		return 0;
	}

    pidSelf = getpid();

	while ((next = readdir(dir)) != NULL)
    {
		FILE *status;
		char filename[64];
		char buffer[64];
		char name[64];

		/* Must skip ".." since that is outside /proc */
		if (strcmp(next->d_name, "..") == 0)
			continue;

		/* If it isn't a number, we don't want it */
		if (!isdigit(*next->d_name))
			continue;

		sprintf(filename, "/proc/%s/status", next->d_name);
		if (! (status = fopen(filename, "r")) )
        {
			continue;
		}
        
		if (fgets(buffer, 63, status) == NULL)
        {
			fclose(status);
			continue;
		}
		fclose(status);

		/* Buffer should contain a string like "Name:   binary_name" */
		sscanf(buffer, "%*s %s", name);
		if (strcmp(name, pidName) == 0)
        {
            if(pidSelf)
		//	pidList=xrealloc( pidList, sizeof(pid_t) * (i+2));
			pid=(pid_t)strtol(next->d_name, NULL, 0);

            if(pid != pidSelf)
            {
		        closedir(dir);
                return pid;
            }
		}
	}	
	closedir(dir);
	return 0;
}

static int checkStatusFile(char *fileName, SPEEDTEST_STATUS *status)
{
    FILE *fp = NULL;
    char buf[32] = {0};

    if(fileName == NULL)
    {
        return -1;
    }
    
    if(!isFileExist(fileName))
    {
        *status = SPEEDTEST_STATUS_FILE_NO_FIND;
    }
    else
    {
        fp = fopen(fileName, "r");
        if (fp == NULL)
        {
            fprintf(stderr, "Create %s error!\n", fileName);
            return -1;
        }

        fgets(buf, 32, fp);
        
        if(strstr(buf, "dlRunning"))
        {
            *status = SPEEDTEST_STATUS_DL_RUNNING;
        }
        else if(strstr(buf, "ulRunning"))
        {
            *status = SPEEDTEST_STATUS_UL_RUNNING;
        }
        else if(strstr(buf, "stop"))
        {
            *status = SPEEDTEST_STATUS_STOP;
        }
        else
        {
            *status = SPEEDTEST_STATUS_UNKNOWN;
        }
    }

    if(fp != NULL)
        fclose(fp);
    
    return 0;
}
#endif

static void *__calculateThread(void *arg)
{
    THREADMNG_T *threadMng = (THREADMNG_T*)arg;
    size_t downloadThreadNums = speedTestConfig->downloadThreadConfig.threadsCount;
    size_t uploadThreadNums = speedTestConfig->uploadThreadConfig.threadsCount;
    int i, flag;
    float speed = 0;
    struct timeval tval_start;

    gettimeofday(&tval_start, NULL);

#ifdef SPEEDTEST_FILE_DUMP_SUPPORT 
    char buf[32] = {0};
#endif
    /* do with download test */
    while(1)
    {
        flag = 1;
        
        for(i = 0; i < downloadThreadNums; i++)
        {
            if(threadMng->downloadArgs[i].threadEnd == 0)
            {
                flag = 0;
                break;
            }
        }

        speed = 0;
        totalTransfered = 1024 * 1024;

        for (i = 0; i < downloadThreadNums; i++)
        {
            if (threadMng->downloadArgs[i].transferedBytes)
            {
                /* There's no reason that we transfered nothing except error occured */
                totalTransfered += threadMng->downloadArgs[i].transferedBytes;
                speed += (threadMng->downloadArgs[i].transferedBytes / threadMng->downloadArgs[i].elapsedSecs) / 1024;
            } 
        }

        downloadSpeed = speed;
        
        /* Report */
        /*ST_DBG("Bytes %lu downloaded with a speed %.2f kB/s (%.2f Mbit/s)\n",
            totalTransfered, speed, speed * 8 / 1024);*/

#ifdef SPEEDTEST_FILE_DUMP_SUPPORT   
        if(fpDownloadSpeed != NULL)
        {
            memset(buf, 0, 32);
            sprintf(buf, "%.2f", speed);
            fseek(fpDownloadSpeed, 0, SEEK_SET);
            fwrite(buf, 1, 32, fpDownloadSpeed);
            fflush(fpDownloadSpeed);
        }
#endif

        if(flag == 1)
        {
            for (i = 0; i < downloadThreadNums; i++)
            {
                /* Cleanup */
                ST_SAFE_FREE(threadMng->downloadArgs[i].url);
            }
            
            downloadTestEnd = 1;
            break;
        }

        if(getElapsedTime(tval_start) > SPEEDTEST_LONGEST_TEST_TIME)
        {
#ifdef SPEEDTEST_FILE_DUMP_SUPPORT
            int pid = -1;
            char cmd[32];
            char buf[32];
            FILE *pf = NULL;
            
            pid = findPidByName("Speedtest");//Max name length is 15
            if(pid > 0)
            {
                /* kill proccess */
                memset(cmd, 0x0, 32);
                sprintf(cmd, "kill -9 %d", pid);
                system(cmd);
            }
            
            /* restore file */
            if(isFileExist(SPEEDTEST_STATUS_FILE))
            {
                pf = fopen(SPEEDTEST_STATUS_FILE, "r+");
                if (pf == NULL)
                {
                    fprintf(stderr, "Create %s error!\n", SPEEDTEST_STATUS_FILE);
                }
                else
                {
                    memset(buf, 0, 32);
                    sprintf(buf, "%s", "stop");
                    fseek(pf, 0, SEEK_SET);
                    fwrite(buf, 1, 32, pf);
                    fflush(pf); 
                    
                    if(pf != NULL)
                    {
                        fclose(pf);
                        pf = NULL;
                    }
                }
            }         
            break;
#else
            stopStatus = 1;
#endif 
        }

        usleep(1000 * 50);
        
    }

    /* Report */
    ST_DBG("Bytes %lu downloaded with a speed %.2f kB/s (%.2f Mbit/s)\n",
        totalTransfered, speed, speed * 8 / 1024);

#ifdef SPEEDTEST_FILE_DUMP_SUPPORT
    if(fpStatus != NULL)
    {
        memset(buf, 0, 32);
        sprintf(buf, "%s", "ulRunning");
        fseek(fpStatus, 0, SEEK_SET);
        fwrite(buf, 1, 32, fpStatus);
        fflush(fpStatus);
    }
#else
    speedTestProgress = 1;
#endif

    /* do with upload test */
    while(1)
    {
        flag = 1;
        
        for(i = 0; i < uploadThreadNums; i++)
        {
            if(threadMng->uploadArgs[i].threadEnd == 0)
            {
                flag = 0;
                break;
            }
        }
       
        speed = 0;
        totalTransfered = 0;

        for (i = 0; i < downloadThreadNums; i++)
        {
            if (threadMng->uploadArgs[i].transferedBytes)
            {
                /* There's no reason that we transfered nothing except error occured */
                totalTransfered += threadMng->uploadArgs[i].transferedBytes;
                speed += (threadMng->uploadArgs[i].transferedBytes / threadMng->uploadArgs[i].elapsedSecs) / 1024;
            } 
        }

        uploadSpeed = speed;

        /* Report */
        /*ST_DBG("Bytes %lu uploaded with a speed %.2f kB/s (%.2f Mbit/s)\n",
            totalTransfered, speed, speed * 8 / 1024);*/

#ifdef SPEEDTEST_FILE_DUMP_SUPPORT
    
        if(fpUploadSpeed != NULL)
        {
            memset(buf, 0, 32);
            sprintf(buf, "%.2f", speed);
            fseek(fpUploadSpeed, 0, SEEK_SET);
            fwrite(buf, 1, 32, fpUploadSpeed);
            fflush(fpUploadSpeed);
        }
#endif

        if(flag == 1)
        {
            for (i = 0; i < uploadThreadNums; i++)
            {
                /* Cleanup */
                ST_SAFE_FREE(threadMng->uploadArgs[i].url);
            }
            
            uploadTestEnd = 1;
            break;
        }
        
        if(getElapsedTime(tval_start) > SPEEDTEST_LONGEST_TEST_TIME)
        {
#ifdef SPEEDTEST_FILE_DUMP_SUPPORT
            int pid = -1;
            char cmd[32];
            char buf[32];
            FILE *pf = NULL;
            
            pid = findPidByName("Speedtest");//Max name length is 15
            if(pid > 0)
            {
                /* kill proccess */
                memset(cmd, 0x0, 32);
                sprintf(cmd, "kill -9 %d", pid);
                system(cmd);
            }
            
            /* restore file */
            if(isFileExist(SPEEDTEST_STATUS_FILE))
            {
                pf = fopen(SPEEDTEST_STATUS_FILE, "r+");
                if (pf == NULL)
                {
                    fprintf(stderr, "Create %s error!\n", SPEEDTEST_STATUS_FILE);
                }
                else
                {
                    memset(buf, 0, 32);
                    sprintf(buf, "%s", "stop");
                    fseek(pf, 0, SEEK_SET);
                    fwrite(buf, 1, 32, pf);
                    fflush(pf); 
                    
                    if(pf != NULL)
                    {
                        fclose(pf);
                        pf = NULL;
                    }
                }
            }   
            break;
#else
            stopStatus = 1;
#endif 
        }
        usleep(1000 * 50);

    }

    /* Report */
    ST_DBG("Bytes %lu uploaded with a speed %.2f kB/s (%.2f Mbit/s)\n",
        totalTransfered, speed, speed * 8 / 1024);

    ST_SAFE_FREE(ThreadMng.downloadArgs);
    ST_SAFE_FREE(ThreadMng.uploadArgs);
    freeMem();

#ifdef SPEEDTEST_FILE_DUMP_SUPPORT

    if(fpStatus != NULL)
    {
        memset(buf, 0, 32);
        sprintf(buf, "%s", "stop");
        fseek(fpStatus, 0, SEEK_SET);
        fwrite(buf, 1, 32, fpStatus);
        fflush(fpStatus);

        fclose(fpStatus);
    }

#if 0    
    if(fpDownloadSpeed != NULL)
    {   
        memset(buf, 0, 32);
        sprintf(buf, "%s", "0");
        fseek(fpDownloadSpeed, 0, SEEK_SET);
        fwrite(buf, 1, 32, fpDownloadSpeed); 
        fflush(fpDownloadSpeed);

        fclose(fpDownloadSpeed);
    }

    if(fpUploadSpeed != NULL)
    { 
        memset(buf, 0, 32);
        sprintf(buf, "%s", "0");
        fseek(fpUploadSpeed, 0, SEEK_SET);
        fwrite(buf, 1, 32, fpUploadSpeed);
        fflush(fpUploadSpeed);

        fclose(fpUploadSpeed);
    }
#endif 

#else
    speedTestProgress = 2;
    runStatus = 0;
#endif

    ST_DBG("Speedtest finish!\n");

    return NULL;
}

void calculateResult(THREADMNG_T *threadMng)
{
    pthread_create(&threadMng->tidCalcResult, NULL, &__calculateThread, threadMng);

#ifndef SPEEDTEST_FILE_DUMP_SUPPORT
    pthread_detach(threadMng->tidCalcResult);
#endif

    return;
}

void paramInit(void)
{
    totalTransfered = 1024 * 1024;
    totalToBeTransfered = 1024 * 1024;
    totalDownloadTestCount = 1;
    randomizeBestServers = 0;
    lowestLatencyServers = 3;
    serverCount = 0;
    downloadTestEnd = 0;
    downloadSpeed = 0;
    uploadTestEnd = 0; 
    uploadSpeed = 0;

    speedTestConfig = NULL;
    serverList = NULL;
    downloadUrl = NULL;
    tmpUrl = NULL;
    uploadUrl = NULL;
    latencyUrl = NULL;
    ThreadMng.tidCalcResult = -1;
    ThreadMng.downloadArgs = NULL;
    ThreadMng.uploadArgs = NULL;

#ifdef SPEEDTEST_FILE_DUMP_SUPPORT
    fpStatus = NULL;
    fpDownloadSpeed = NULL;
    fpUploadSpeed = NULL;
#else   
    speedTestProgress = 0;
#endif

    return ;
}

#ifndef SPEEDTEST_FILE_DUMP_SUPPORT
int getRunStatus(void)
{
    return runStatus;
}

int getStopStatus(void)
{
    return stopStatus;
}
#endif
extern void testDownload(THREADMNG_T *threadMng, const char *url);
extern void testUpload(THREADMNG_T *threadMng, const char *url);

#ifdef SPEEDTEST_FILE_DUMP_SUPPORT
int main(int argc, char **argv)
#else
/**
 * @brief       start speedtest, then call SPEEDTEST_GetDownloadSpeed or SPEEDTEST_GetUploadSpeed to get speed
 * 
 * @param[in]   none
 * @return      0: success  -1: fail  1:has run
 */
int SPEEDTEST_Start(void)
#endif
{
    int ret = -1;

    paramInit();

#ifdef SPEEDTEST_FILE_DUMP_SUPPORT
    char buf[32] = {0};
    int pid = -1;
    SPEEDTEST_STATUS status = SPEEDTEST_STATUS_UNKNOWN;

    pid = findPidByName("Speedtest");
    if(pid > 0)
    {
        ST_DBG("Speedtest has run.\n");
        return 1;
    }

    ret = checkStatusFile(SPEEDTEST_STATUS_FILE, &status);
    if(ret == -1)
    {
        ST_ERR("checkStatusFile fail.\n");
        return ret;
    }

    switch(status)
    { 
        case SPEEDTEST_STATUS_FILE_NO_FIND:
            fpStatus = fopen(SPEEDTEST_STATUS_FILE, "w+");
            if (fpStatus == NULL)
            {
                fprintf(stderr, "Create %s error!\n", SPEEDTEST_STATUS_FILE);
                return -1;
            }
            break;
        case SPEEDTEST_STATUS_DL_RUNNING:
        case SPEEDTEST_STATUS_UL_RUNNING:   
        case SPEEDTEST_STATUS_STOP:    
        case SPEEDTEST_STATUS_UNKNOWN:
        default: 
            fpStatus = fopen(SPEEDTEST_STATUS_FILE, "r+");
            if (fpStatus == NULL)
            {
                fprintf(stderr, "Create %s error!\n", SPEEDTEST_STATUS_FILE);
                return -1;
            }
            break;
    }

    memset(buf, 0, 32);
    sprintf(buf, "%s", "dlRunning");
    fseek(fpStatus, 0, SEEK_SET);
    fwrite(buf, 1, 32, fpStatus);
    fflush(fpStatus);

    if(isFileExist(SPEEDTEST_DOWNLOAD_SPEED_FILE))
    {
        fpDownloadSpeed = fopen(SPEEDTEST_DOWNLOAD_SPEED_FILE, "r+");
    }
    else
    {
        fpDownloadSpeed = fopen(SPEEDTEST_DOWNLOAD_SPEED_FILE, "w+");
    }
    
    if (fpDownloadSpeed == NULL)
    {
        fprintf(stderr, "Create %s error!\n", SPEEDTEST_DOWNLOAD_SPEED_FILE);

        memset(buf, 0, 32);
        sprintf(buf, "%s", "stop");
        fseek(fpStatus, 0, SEEK_SET);
        fwrite(buf, 1, 32, fpStatus);
        fflush(fpStatus);

        if(fpStatus != NULL)
            fclose(fpStatus);

        return -1;
    }

    if(isFileExist(SPEEDTEST_UPLOAD_SPEED_FILE))
    {
        fpUploadSpeed = fopen(SPEEDTEST_UPLOAD_SPEED_FILE, "r+");
    }
    else
    {
        fpUploadSpeed = fopen(SPEEDTEST_UPLOAD_SPEED_FILE, "w+");
    }
    
    if (fpUploadSpeed == NULL)
    {
        fprintf(stderr, "Create %s error!\n", SPEEDTEST_UPLOAD_SPEED_FILE);
        
        memset(buf, 0, 32);
        sprintf(buf, "%s", "stop");
        fseek(fpStatus, 0, SEEK_SET);
        fwrite(buf, 1, 32, fpStatus);
        fflush(fpStatus);

        if(fpStatus != NULL)
            fclose(fpStatus);
        
        if(fpDownloadSpeed != NULL)
            fclose(fpDownloadSpeed);

        return -1;
    }

    memset(buf, 0, 32);
    sprintf(buf, "%s", "0");
    fseek(fpDownloadSpeed, 0, SEEK_SET);
    fwrite(buf, 1, 32, fpDownloadSpeed); 
    fflush(fpDownloadSpeed);

    memset(buf, 0, 32);
    sprintf(buf, "%s", "0");
    fseek(fpUploadSpeed, 0, SEEK_SET);
    fwrite(buf, 1, 32, fpUploadSpeed);
    fflush(fpUploadSpeed);

#else
    if(runStatus == 1)
    {
        ST_DBG("Speedtest has run!\n");
        return 1;
    }

    runStatus = 1;
    stopStatus = 0;
#endif

    ST_DBG("Speedtest start!\n");
    
#ifdef OPENSSL
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
#endif

    if(downloadUrl == NULL)
    {
        ret = getBestServer();
    }
    else
    {
        ret = getUserDefinedServer();
    }

    if(ret != 0)
    {
#ifdef SPEEDTEST_FILE_DUMP_SUPPORT

        memset(buf, 0, 32);
        sprintf(buf, "%s", "stop");
        fseek(fpStatus, 0, SEEK_SET);
        fwrite(buf, 1, 32, fpStatus);
        fflush(fpStatus);

        if(fpStatus != NULL)
            fclose(fpStatus);

        if(fpDownloadSpeed != NULL)
            fclose(fpDownloadSpeed);
        
        if(fpUploadSpeed != NULL)
            fclose(fpUploadSpeed);

#else
        runStatus = 0;
        speedTestProgress = 2;
        return -1;
#endif
    }

    if (lowestLatencyServers == 0)
    {
        latencyUrl = getLatencyUrl(uploadUrl);
        if(latencyUrl != NULL)
        {
            ST_DBG("Latency: %ld %s\n", getLatency(latencyUrl), LATENCY_UNITS);
        }
    }

    size_t downloadThreadNums = speedTestConfig->downloadThreadConfig.threadsCount;
    size_t uploadThreadNums = speedTestConfig->uploadThreadConfig.threadsCount;
    if(ThreadMng.downloadArgs == NULL)
    {
        ThreadMng.downloadArgs = (THREADARGS_T *)calloc(downloadThreadNums, sizeof(THREADARGS_T));
    }
    
    if(ThreadMng.uploadArgs == NULL)
    {
        ThreadMng.uploadArgs = (THREADARGS_T *) calloc(uploadThreadNums, sizeof(THREADARGS_T));
    }

    calculateResult(&ThreadMng);
    testDownload(&ThreadMng, downloadUrl);
    testUpload(&ThreadMng, uploadUrl);

#ifdef SPEEDTEST_FILE_DUMP_SUPPORT
    if(ThreadMng.tidCalcResult != -1)
    {
        pthread_join(ThreadMng.tidCalcResult, NULL);
    }
#endif

    return 0;
}

#ifdef SPEEDTEST_FILE_DUMP_SUPPORT
int SPEEDTEST_Start(void)
{
    system("Speedtest &");
    return 0;
}
#endif

/**
 * @brief       force to stop speedtest
 * 
 * @param[in]   none
 * @return      0: success  -1: fail
 */
int SPEEDTEST_Stop(void)
{
#ifdef SPEEDTEST_FILE_DUMP_SUPPORT

    int pid = -1;
    char cmd[32];
    char buf[32];
    FILE *pf = NULL;
    
    pid = findPidByName("Speedtest");//Max name length is 15
    if(pid > 0)
    {
        /* kill proccess */
        memset(cmd, 0x0, 32);
        sprintf(cmd, "kill -9 %d", pid);
        system(cmd);
    }
    
    /* restore file */
    if(isFileExist(SPEEDTEST_STATUS_FILE))
    {
        pf = fopen(SPEEDTEST_STATUS_FILE, "r+");
        if (pf == NULL)
        {
            fprintf(stderr, "Create %s error!\n", SPEEDTEST_STATUS_FILE);
        }
        else
        {
            memset(buf, 0, 32);
            sprintf(buf, "%s", "stop");
            fseek(pf, 0, SEEK_SET);
            fwrite(buf, 1, 32, pf);
            fflush(pf); 
            
            if(pf != NULL)
            {
                fclose(pf);
                pf = NULL;
            }
        }
    }

#if 0
    if(isFileExist(SPEEDTEST_DOWNLOAD_SPEED_FILE))
    {
        pf = fopen(SPEEDTEST_DOWNLOAD_SPEED_FILE, "r+");
        if (pf == NULL)
        {
            fprintf(stderr, "Create %s error!\n", SPEEDTEST_DOWNLOAD_SPEED_FILE);
        }
        else
        {
            memset(buf, 0, 32);
            sprintf(buf, "%s", "0");
            fseek(pf, 0, SEEK_SET);
            fwrite(buf, 1, 32, pf);
            fflush(pf); 
            
            if(pf != NULL)
            {
                fclose(pf);
                pf = NULL;
            }
        }
    }
    
    if(isFileExist(SPEEDTEST_UPLOAD_SPEED_FILE))
    {
        pf = fopen(SPEEDTEST_UPLOAD_SPEED_FILE, "r+");
        if (pf == NULL)
        {
            fprintf(stderr, "Create %s error!\n", SPEEDTEST_UPLOAD_SPEED_FILE);
        }
        else
        {
            memset(buf, 0, 32);
            sprintf(buf, "%s", "0");
            fseek(pf, 0, SEEK_SET);
            fwrite(buf, 1, 32, pf);
            fflush(pf); 
            
            if(pf != NULL)
            {
                fclose(pf);
                pf = NULL;
            }
        }
    } 
#endif

#else
    stopStatus = 1;
#endif 

    return 0;
}

/**
 * @brief       get speedtest status, call this api to ensure if speedtest has finished.
 * 
 * @param[in]   type: 0:downlaod status / 1:upload status
 * @return      speedtest status: 0:finish 1:running 2:not start
 */
int SPEEDTEST_GetStatus(int type)
{
#ifdef SPEEDTEST_FILE_DUMP_SUPPORT
    int ret = -1;
    SPEEDTEST_STATUS status = SPEEDTEST_STATUS_UNKNOWN;

    ret = checkStatusFile(SPEEDTEST_STATUS_FILE, &status);
    if(ret == -1)
    {
        ST_ERR("checkStatusFile fail.\n");
        return 1;
    }

    if(type == 0)
    {
        if(status == SPEEDTEST_STATUS_UL_RUNNING)
        {
            return 0;
        }
        else if(status == SPEEDTEST_STATUS_DL_RUNNING)
        {
            return 1;
        }
        else
        {
            return 2;
        }
    }
    else if(type == 1)
    {
        if(status == SPEEDTEST_STATUS_STOP)
        {
            return 0;
        }
        else if(status == SPEEDTEST_STATUS_UL_RUNNING)
        {
            return 1;
        }
        else
        {
            return 2;     
        }
    }
#else
    if(type == 0)
    {
        if(speedTestProgress == 1)
        {
            return 0;
        }
        else if(speedTestProgress == 0)
        {
            return 1;
        }
        else
        {
            return 2;     
        }
    }
    else if(type == 1)
    {
        if(speedTestProgress == 2)
        {
            return 0;
        }
        else if(speedTestProgress == 1)
        {
            return 1;
        }
        else
        {
            return 2;     
        }
    }
    else
    {
        ST_ERR("Type %d error\n", type);
    }
#endif

    return 1;
}

/**
 * @brief       get download speed
 * 
 * @param[in]   none
 * @return      download speed
 */
float SPEEDTEST_GetDownloadSpeed(void)
{
#ifdef SPEEDTEST_FILE_DUMP_SUPPORT
    FILE *fp = NULL;
    char buf[32] = {0};
    float speed = 0;

    if(!isFileExist(SPEEDTEST_DOWNLOAD_SPEED_FILE))
    {
        ST_ERR("File \"%s\" is not exist.\n", SPEEDTEST_DOWNLOAD_SPEED_FILE);
        return 0;
    }
    else
    {
        fp = fopen(SPEEDTEST_DOWNLOAD_SPEED_FILE, "r");
        if (fp == NULL)
        {
            fprintf(stderr, "Create %s error!\n", SPEEDTEST_DOWNLOAD_SPEED_FILE);
            return 0;
        }

        fgets(buf, 32, fp);
        speed = (float)atof(buf);
        
        return speed;
    }
#else
    return downloadSpeed;
#endif
    
}

/**
 * @brief       get upload speed
 * 
 * @param[in]   none
 * @return      upload speed
 */
float SPEEDTEST_GetUploadSpeed(void)
{
#ifdef SPEEDTEST_FILE_DUMP_SUPPORT
        FILE *fp = NULL;
        char buf[32] = {0};
        float speed = 0;
    
        if(!isFileExist(SPEEDTEST_UPLOAD_SPEED_FILE))
        {
            ST_ERR("File \"%s\" is not exist.\n", SPEEDTEST_UPLOAD_SPEED_FILE);
            return 0;
        }
        else
        {
            fp = fopen(SPEEDTEST_UPLOAD_SPEED_FILE, "r");
            if (fp == NULL)
            {
                fprintf(stderr, "Create %s error!\n", SPEEDTEST_UPLOAD_SPEED_FILE);
                return 0;
            }
    
            fgets(buf, 32, fp);
            speed = (float)atof(buf);
            
            return speed;
        }
#else
        return uploadSpeed;
#endif
}


