/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2016 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
 
/**
* @file
*
* @brief IARM-Bus TR69 Bus Internal API.
*
* This API defines the operations used to starting and stopping IARM-Bus 
* TR69 Bus Manager.
*/



/**
* @defgroup iarmmgrs
* @{
* @defgroup tr69Bus
* @{
**/


#ifndef _TR69BUSINTERNAL_H_
#define _TR69BUSINTERNAL_H_


#include "libIARM.h"
#include <string.h>
#ifdef RDK_LOGGER_ENABLED
#include "rdk_debug.h"
#include "iarmUtil.h"

extern int b_rdk_logger_enabled;

#define LOG(...)              INT_LOG(__VA_ARGS__, "")
#define INT_LOG(FORMAT, ...)     if(b_rdk_logger_enabled) {\
RDK_LOG(RDK_LOG_DEBUG, "LOG.RDK.TR69MGR", FORMAT , __VA_ARGS__);\
}\
else\
{\
printf(FORMAT, __VA_ARGS__);\
}

#else

#define LOG(...)              printf(__VA_ARGS__)

#endif




 /**
 * @brief Starts the TR69 Bus.
 *
 * This function registers and connects TR69 Bus Manager to the iarm bus.
 * Register Events that this module publishes and register APIs that 
 * can be RPCs by other entities on the bus.
 *
 * @return IARM_Result_t Error Code.
 * @retval IARM_RESULT_SUCCESS on success
 */
IARM_Result_t TR69Bus_Start(void);

/**
 * @brief Terminates the TR69 Bus 
 *
 * This function disconnects TR69 Bus Manager from the iarm bus and terminates 
 * it.
 *
 * @return IARM_Result_t Error Code.
 * @retval IARM_RESULT_SUCCESS on success
 */
IARM_Result_t TR69Bus_Stop(void);

/**
 * @brief Listens for component specific events from drivers.
 *
 * @return IARM_Result_t Error Code.
 * @retval IARM_RESULT_SUCCESS on success
 */
IARM_Result_t TR69Bus_Loop(void);


typedef void (*TR69Mgr_LogCb)(const char *);

void TR69Mgr_RegisterForLog(TR69Mgr_LogCb cb);



#endif



/** @} */
/** @} */
