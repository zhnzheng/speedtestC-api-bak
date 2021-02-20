/*
    Server list parsing functions.

    Michał Obrembski (byku@byku.com.pl)
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SpeedtestServers.h"
#include "http.h"

/* "http://mirror.nonstop.co.il/speedtest/" */
#define RANDOM_JPG1 "random350x350.jpg"     /* 239.6K */
#define RANDOM_JPG2 "random1500x1500.jpg"   /* 4.2M   */
#define RANDOM_JPG3 "random2000x2000.jpg"   /* 7.5M   */
#define RANDOM_JPG4 "random4000x4000.jpg"   /* 30.1M  */

void parseServer(SPEEDTESTSERVER_T *result, const char *configline)
{
    /* TODO: Remove Switch, replace it with something space-friendly
    result = malloc(sizeof(SPEEDTESTSERVER_T));*/
    int tokensize,size;
    char *first, *second, *substr;
    const char *tokens[8] = {"url=\"","lat=\"", "lon=\"", "name=\"",
                                "country=\"", "cc=\"", "sponsor=\"", "id=\""};
    int i;
    for(i=1; i < 8; i++)
    {
        first = strstr(configline, tokens[i-1]);
        second = strstr(configline, tokens[i]);
        if(first == NULL || second==NULL )
            return;
        tokensize = strlen(tokens[i-1]);
        size = second - first - 1;
        substr = calloc(sizeof(char), size);
        strncpy(substr, first+tokensize, size - tokensize - 1);
        substr[size - tokensize] = '\0';
        switch(i)
        {
            case 1:
                result->url = substr;
                break;
            case 2:
                result->lat = strtof(substr, NULL);
                free(substr);
                break;
            case 3:
                result->lon = strtof(substr, NULL);
                free(substr);
                break;
            case 4:
                result->name = substr;
                break;
            case 5:
                result->country = substr;
                break;
            case 7:
                result->sponsor = substr;
                break;
            default:
                free(substr);
                break;
        }
    }
}

SPEEDTESTSERVER_T **getServers(int *serverCount, const char *infraUrl)
{
    char buffer[1500] = {0};
    int i;
    SPEEDTESTSERVER_T **list = NULL;
    sock_t sockId = httpGetRequestSocket(infraUrl, 1);
    if(sockId != BAD_SOCKID) {
        long size;
        while((size = recvLine(sockId, buffer, sizeof(buffer))) > 0)
        {
            buffer[size + 1] = '\0';
            if(strlen(buffer) > 25)
            {
                /*Ommiting XML invocation...*/
                if (strstr(buffer, "<?xml"))
                    continue;
                /*TODO: Fix case when server entry doesn't fit in TCP packet*/
                if(buffer[0] == '<' && buffer[size - 1] == '>')
                {
                    *serverCount = *serverCount + 1;
                    list = (SPEEDTESTSERVER_T**)realloc(list,
                        sizeof(SPEEDTESTSERVER_T**) * (*serverCount));
                    if(list == NULL) {
                        fprintf(stderr, "Unable to realloc memory for server list!\n");
                        goto ERROR;
                    }
                    list[*serverCount - 1] = malloc(sizeof(SPEEDTESTSERVER_T));
                    if(list[*serverCount - 1]){
                        parseServer(list[*serverCount - 1],buffer);
                    }else{
                        fprintf(stderr, "Unable to malloc memory for server list!\n");
                        goto ERROR;
                    }
                }
            }
        }
        httpClose(sockId);
        return list;

ERROR:
        for(i = 0; i < *serverCount - 1; i++)
        {
            if(list[i])
                free(list[i]);
        }

        if(list)
            free(list);

        *serverCount = 0;
        httpClose(sockId);
        return NULL;
    }
    return NULL;
}

static char *modifyServerUrl(char *serverUrl, const char *urlFile)
{
    size_t urlSize = strlen(serverUrl);
    char *upload = strstr(serverUrl, "upload.php");
    if(upload == NULL)
    {
        printf("Download URL parsing error - cannot find upload.php in %s\n",
            serverUrl);
        return NULL;
    }
    size_t uploadSize = strlen(upload);
    size_t totalSize = (urlSize - uploadSize) +
        strlen(urlFile) + 1;
    char *result = (char*)malloc(sizeof(char) * totalSize);
    result[(urlSize - uploadSize)] = '\0';
    memcpy(result, serverUrl, urlSize - uploadSize);
    strcat(result, urlFile);
    return result;
}

char *getServerDownloadUrl(char *serverUrl)
{
    return modifyServerUrl(serverUrl, RANDOM_JPG3);
}

char *getLatencyUrl(char *serverUrl)
{
    return modifyServerUrl(serverUrl, "latency.txt");
}
