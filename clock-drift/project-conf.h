/*
* Copyright (c) 2024, Clock Drift Monitor
* All rights reserved.
*
* Project configuration for TSCH Clock Drift Monitor
*/
#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_


/*******************************************************/
/************* Other system configuration *************/
/*******************************************************/

/* Disable all system logs */
#define LOG_CONF_LEVEL_MAIN LOG_LEVEL_NONE
#define LOG_CONF_LEVEL_RPL LOG_LEVEL_NONE
#define LOG_CONF_LEVEL_TCPIP LOG_LEVEL_NONE
#define LOG_CONF_LEVEL_IPV6 LOG_LEVEL_NONE
#define LOG_CONF_LEVEL_6LOWPAN LOG_LEVEL_NONE
#define LOG_CONF_LEVEL_MAC LOG_LEVEL_NONE
#define LOG_CONF_LEVEL_FRAMER LOG_LEVEL_NONE
#define LOG_CONF_LEVEL_ROUTING LOG_LEVEL_NONE

/* Enable TSCH per-slot logging for drift monitoring */
#define TSCH_LOG_CONF_PER_SLOT 1
//#define TSCH_LOG_CONF_QUEUE_LEN 16

#define SEND_DRIFT_CALLBACK 1

/* Make sure we have enough space for our drift packets */
#define UIP_CONF_BUFFER_SIZE 200

/* Clock drift monitoring specific */
#define CLOCK_DRIFT_MONITOR 1

#endif /* PROJECT_CONF_H_ */