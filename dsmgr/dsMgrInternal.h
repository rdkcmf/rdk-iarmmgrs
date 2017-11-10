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
 * @file dsMgrInternal.h
 *
 * @defgroup IARMBUS_DS_MGR DS Manager
 * @ingroup IARM_MGR_RPC
 *
 * DS (Device Settings) Manager is responsible for managing the following operations,
 * - Audio Output Ports (Volume, Mute, etc.)
 * - Video Ouptut Ports (Resolutions, Aspect Ratio, etc.)
 * - Front Panel Indicators
 * - Zoom Settings
 * - Display (Aspect Ratio, EDID data etc.)
 * - General Host configuration (Power managements, event management etc.)
 */

/**
* @defgroup iarmmgrs
* @{
* @defgroup dsmgr
* @{
**/


#ifndef _IARM_DSMGR_INTERNAL_
#define _IARM_DSMGR_INTERNAL_
#include "libIARM.h"

#define IARM_BUS_DSMGR_NAME		"DSMgr"
#include <string.h>
#ifdef RDK_LOGGER_ENABLED
#include "rdk_debug.h"
#include "iarmUtil.h"


extern int b_rdk_logger_enabled;


#define INT_DEBUG(FORMAT, ...)       if(b_rdk_logger_enabled) {\
RDK_LOG(RDK_LOG_DEBUG, "LOG.RDK.DSMGR", FORMAT , __VA_ARGS__);\
}\
else\
{\
printf(FORMAT, __VA_ARGS__);\
}

#define INT_ERROR(FORMAT, ...)       if(b_rdk_logger_enabled) {\
RDK_LOG(RDK_LOG_ERROR, "LOG.RDK.DSMGR", FORMAT , __VA_ARGS__);\
}\
else\
{\
printf(FORMAT, __VA_ARGS__);\
}

#define INT_INFO(FORMAT, ...)        if(b_rdk_logger_enabled) {\
RDK_LOG(RDK_LOG_INFO, "LOG.RDK.DSMGR", FORMAT , __VA_ARGS__);\
}\
else\
{\
printf(FORMAT, __VA_ARGS__);\
}

#define INT_WARNING(FORMAT, ...)     if(b_rdk_logger_enabled) {\
RDK_LOG(RDK_LOG_WARN, "LOG.RDK.DSMGR", FORMAT , __VA_ARGS__);\
}\
else\
{\
printf(FORMAT, __VA_ARGS__);\
}

#else

#define INT_DEBUG(FORMAT, ...)        printf(FORMAT, __VA_ARGS__)
#define INT_ERROR(FORMAT, ...)        printf(FORMAT, __VA_ARGS__)
#define INT_INFO(FORMAT, ...)         printf(FORMAT, __VA_ARGS__)
#define INT_WARNING(FORMAT, ...)      printf(FORMAT, __VA_ARGS__)

#endif

#define DEBUG_LOG(...)              INT_DEBUG(__VA_ARGS__, "")
#define INFO_LOG(...)               INT_INFO(__VA_ARGS__, "")
#define ERROR_LOG(...)              INT_ERROR(__VA_ARGS__, "")
#define WARN_LOG(...)               INT_WARNING(__VA_ARGS__, "")

IARM_Result_t DSMgr_Start();
IARM_Result_t DSMgr_Loop();
IARM_Result_t DSMgr_Stop(void);


#endif


/** @} */
/** @} */
