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
* @defgroup iarmmgrs
* @{
* @defgroup vrexmgr
* @{
**/


#ifndef __VREX_LOG_INTERNAL_H__
#define __VREX_LOG_INTERNAL_H__

#include <stdio.h>

#ifdef RDK_LOGGER_ENABLED
#include "rdk_debug.h"
#include "iarmUtil.h"

#ifdef __cplusplus
extern "C"
{
#endif

extern int b_rdk_logger_enabled;

#ifdef __cplusplus
extern "C"
}
#endif

#define INT_LOG(...)     if(b_rdk_logger_enabled) {\
RDK_LOG(RDK_LOG_DEBUG, "LOG.RDK.VREXMGR", __VA_ARGS__);\
}\
else\
{\
printf(__VA_ARGS__);\
}

#else /* RDK_LOGGER_ENABLED */

#define INT_LOG(...)      printf(__VA_ARGS__)

#endif /* RDK_LOGGER_ENABLED */

#define LOG(...)              INT_LOG("VREXMGR: " __VA_ARGS__)
#define STATUS_LOG(...)       LOG("IARMSTATUS: " __VA_ARGS__)


#endif /* __VREX_LOG_INTERNAL_H__ */


/** @} */
/** @} */
