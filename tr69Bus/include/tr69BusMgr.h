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
 * @file tr69BusMgr.h
 *
 * @brief IARM-Bus TR69 Bus Manager Public API.
 *
 * @defgroup IARMBUS_TR69BUS_MGR TR-069 Manager
 * @ingroup IARM_MGR
 *
 * TR-069 is the CPE WAN Management protocol (CWMP) which defines an application layer protocol
 * for remote management of end user devices from Auto Configuration Server (ACS).
 * TR-069 is a bidirectional SOAP/HTTP based protocol.
 * The tr69Bus Manager is used by the applications for registering an Agent and data handling with
 * the TR69 Bus.

 * @par TR-069 Bus Manager: RPC Methods
 *
 * - @b IARM_BUS_TR69_BUS_MGR_API_RegisterAgent
 * @n This Method is used to register an tr69 Agent through the IARM.
 * @n Example:
 * @code
 * IARM_Bus_RegisterCall(IARM_BUS_TR69_BUS_MGR_API_RegisterAgent,registerAgent);
 * @endcode
 * @n
 * - @b IARM_BUS_TR69_BUS_MGR_API_UnRegisterAgent
 * @n This Method is used to un-register the tr69 agent from Bus.
 * @n Example:
 * @code
 * IARM_Bus_RegisterCall(IARM_BUS_TR69_BUS_MGR_API_UnRegisterAgent,unRegisterAgent);
 * @endcode
 * @n
 * - @b IARM_BUS_TR69_BUS_MGR_API_ParameterHandler
 * @n This Method is used to Get or set Parameters to the tr69 agent through Manager application.
 * @n Example:
 * @code
 * IARM_Bus_RegisterCall(IARM_BUS_TR69_BUS_MGR_API_ParameterHandler, parameterHandler);
 * @endcode
*/

/**
* @defgroup iarmmgrs
* @{
* @defgroup tr69Bus
* @{
**/

#ifndef _TR69_BUS_MGR_H_
#define _TR69_BUS_MGR_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "libIBus.h"
#include "libIARM.h"

#define IARM_BUS_TR69_BUS_MGR_NAME 		"TR69_BUS"     /*!< Well-known Name for TR69_BUS library */
#define IARM_BUS_TR69_BUS_XRE_AGENT_NAME    	"XRE_native_receiver"
#define BUF_MIN    	64
#define BUF_MAX    	1024

/*! Types of errors that could be reported by Agent (please extend this)*/
typedef enum _IARM_Bus_TR69Bus_Error_Type_t {
    IARM_Bus_TR69_BUS_SUCCESS = 0,
    IARM_Bus_TR69_BUS_ERROR_NO_SUCH_AGENT,
    IARM_Bus_TR69_BUS_ERROR_NO_SUCH_METHOD,
    IARM_Bus_TR69_BUS_ERROR_NO_SUCH_PARAM,
    IARM_Bus_TR69_BUS_ERROR_NO_MEM,
    IARM_Bus_TR69_BUS_ERROR_UNSUPPORTED_MODE,
    IARM_Bus_TR69_BUS_ERROR_GENERAL_ERROR
}IARM_Bus_TR69Bus_Error_Type_t;

/* Type of operation requested on the parameter
 */
typedef enum _IARM_Bus_TR69Bus_Request_Type_t {
    IARM_Bus_TR69_BUS_MODE_Get,
    IARM_Bus_TR69_BUS_MODE_Set
} IARM_Bus_TR69Bus_Request_Type_t;

/* Parameter type
 */
typedef enum _IARM_Bus_TR69Bus_Param_Type_t
{
    IARM_Bus_TR69_BUS_TYPE_String,
    IARM_Bus_TR69_BUS_TYPE_Int,
    IARM_Bus_TR69_BUS_TYPE_UnsignedInt,
    IARM_Bus_TR69_BUS_TYPE_Boolean,
    IARM_Bus_TR69_BUS_TYPE_DateTime
} IARM_Bus_TR69Bus_Param_Type_t;

#define IARM_BUS_TR69_BUS_MGR_API_RegisterAgent "registerAgent" /*!< Registers TR69 agent to the bus*/

typedef struct _IARM_Bus_TR69_BUS_RegisterAgent_Param_t{
    char agentName[BUF_MIN];                                   /*!< [in] Pointer to shared memory location having null terminated name of the agent (This will be deleted by the caller)*/
    int status; 											/*!< [in] Pointer to shared memory location having null terminated name of the agent (This will be deleted by the caller)*/
                                                        /* This shall be same string that is used by agent to register to IARMBus */
}IARM_Bus_TR69_BUS_RegisterAgent_Param_t;

#define IARM_BUS_TR69_BUS_MGR_API_UnRegisterAgent "unRegisterAgent" /*!< Unegister TR69 agent from the bus*/

typedef struct _IARM_Bus_TR69_BUS_UnRegisterAgent_Param_t{
    char  agentName[BUF_MIN];                             /*!< [in] Pointer to shared memory location having null terminated name of the agent (This will be deleted by the caller)*/
}IARM_Bus_TR69_BUS_UnRegisterAgent_Param_t;

#define IARM_BUS_TR69_BUS_MGR_API_ParameterHandler  "parameterHandler" /*!< Gets/Sets the given parameter from given TR69 agent*/

/*Common APIs to be implemented by TR69 sub-agents*/

#define IARM_BUS_TR69_COMMON_API_AgentParameterHandler	"agentParameterHandler" /*!< Common API to be implemented by TR69 agent to return data*/

typedef struct _IARM_Bus_TR69_BUS_AgentRequestInfo_Param_t{
    char agentName[BUF_MIN];                                   /*!< [in] Pointer to shared memory location having null terminated name of the agent (This will be deleted by the caller)*/

    char paramName[BUF_MIN];                                   /*!< [in] Pointer to shared memory location having null terminated name of the param (This will be deleted by the caller)*/
    IARM_Bus_TR69Bus_Param_Type_t paramType;            /*!< [in] er to shared memory location having data (This data will be allocated by callee and will be deleted by the caller)*/
    char paramValue[BUF_MAX];                                  /*!< [out] Pointer to shared memory location having data (This data will be allocated by callee and will be deleted by the caller)*/
    int  paramLen;                                      /*!< [out] size of the data pointed by pData*/

    IARM_Bus_TR69Bus_Request_Type_t mode;               /*!< [in] Request type (set/get)*/
    short instanceNumber;                               /*!< [in] Instance Number [Scalar =0, Tabular = 1..n] */
    IARM_Bus_TR69Bus_Error_Type_t  err_no;              /*!< [out] error number will be set by the callee on the failure of the call */	
}IARM_Bus_TR69_BUS_AgentRequestInfo_Param_t;

#ifdef __cplusplus
}
#endif

#endif //_TR69_BUS_MGR_H_

/** @} */
/** @} */
