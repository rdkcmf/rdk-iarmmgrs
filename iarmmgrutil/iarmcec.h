/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
* @file ircec.h
*
* @brief Handles the IR CEC Local operations.
*
* This API defines the operations for key management.
*
* @par Document
* Document reference.
*
* @par Open Issues (in no particular order)
* -# None
*
* @par Assumptions
* -# None
*
* @par Implementation Notes
* -# None
*
*
*/



/**
* @defgroup iarmmgrs
* @{
* @defgroup iarmutil
* @{
**/


#ifndef _IARM_IRCEC_
#define _IARM_IRCEC_

#ifdef RDK_LOGGER_ENABLED
#include "rdk_debug.h"
#include "iarmUtil.h"
extern int b_rdk_logger_enabled;

#define LOG(...)              INT_LOG(__VA_ARGS__, "")
#define INT_LOG(FORMAT, ...)     if(b_rdk_logger_enabled) {\
RDK_LOG(RDK_LOG_DEBUG, "LOG.RDK.IRMGR", FORMAT , __VA_ARGS__);\
}\
else\
{\
printf(FORMAT, __VA_ARGS__);\
}

#else
#define LOG(...)              printf(__VA_ARGS__)
#endif

/** @addtogroup IARM_IR_CEC_API IR CEC API.
*  @ingroup IARM_BUS
*
* Described here in are functions to IR CEC local logic.
*
*  @{
*/

/** 
 * @brief Utility function to send ActiveSource CEC event to TV.
 * @return if return 1 caller should not broadcast IARM key event.
 */
bool IARMCEC_SendCECActiveSource(bool bCecLocalLogic,int keyType, int keyCode);

/**
 * @brief Utility function to send ImageViewOn CEC event to TV.
 * @return if return 1 caller should not broadcast IARM key event.
 */
bool IARMCEC_SendCECImageViewOn(bool bCecLocalLogic);

#endif
/* End of IARM_IR_CEC_API doxygen group */
/**
 * @}
 */

/** @} */
/** @} */
