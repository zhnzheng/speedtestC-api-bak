#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <sys/time.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include "../include/SpeedtestApi.h"

int main(int argc, char **argv)
{
	int status1,status2;
	int i;

	for(i = 0; i < 500; i++)
	{
		SPEEDTEST_Start();
		printf("[zhn]time: %d\n", i);
		while(1)
		{
			status1 = SPEEDTEST_GetStatus(0);
			status2 = SPEEDTEST_GetStatus(1);
			printf("[zhn]status1:%d\n", status1);
			printf("[zhn]status2:%d\n", status2);
			if(status1 == 2 && status2 == 0)
				break;
			usleep(1000 * 1000);
		}
		usleep(1000 * 1000);
	}
	return 0;
}