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
* @file rdmMgr.h
*
* @brief IARM-Bus RDM Manager Public API.
*/

/**
* @defgroup iarmmgrs
* @{
* @defgroup rdmmgr
* @{
**/


#ifndef _IARM_BUS_RDMMGR_H
#define _IARM_BUS_RDMMGR_H


#ifdef __cplusplus
extern "C" 
{
#endif

#define IARM_BUS_RDMMGR_NAME 					    "RDMMgr" /*!< IARM BUS  name for rdm manager */


/*! Published Events from rdm manager  */
typedef enum _RDMMgr_EventId_t {
	IARM_BUS_RDMMGR_EVENT_APPDOWNLOADS_CHANGED,		/*!< RDM application download status chnaged*/ 
	IARM_BUS_RDMMGR_EVENT_MAX				/*!< Max Event Id */
} IARM_Bus_RDMMgr_EventId_t;


#ifdef __cplusplus
}
#endif

#endif

/* End of IARM_BUS_RDMMGR_API doxygen group */
/**
 * @}
 */



/** @} */
/** @} */
