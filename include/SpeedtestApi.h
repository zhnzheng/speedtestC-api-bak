#ifndef _SPEEDTESTAPI_
#define _SPEEDTESTAPI_

/**
 * @brief       start speedtest, then call SPEEDTEST_GetDownloadSpeed or SPEEDTEST_GetUploadSpeed to get speed
 * 
 * @param[in]   none
 * @return      0: success  -1: fail    1:running
 */
int SPEEDTEST_Start(void);

/**
 * @brief       force to stop speedtest
 * 
 * @param[in]   none
 * @return      0: success  -1: fail
 */
int SPEEDTEST_Stop(void);

/**
 * @brief       get speedtest status, call this api to ensure if speedtest has finished.
 * 
 * @param[in]   type: 0:downlaod status / 1:upload status
 * @return      speedtest status: 0:finish 1:running or not start
 */
int SPEEDTEST_GetStatus(int type);

/**
 * @brief       get download speed
 * 
 * @param[in]   none
 * @return      download speed
 */
float SPEEDTEST_GetDownloadSpeed(void);

/**
 * @brief       get upload speed
 * 
 * @param[in]   none
 * @return      upload speed
 */
float SPEEDTEST_GetUploadSpeed(void);

#endif
