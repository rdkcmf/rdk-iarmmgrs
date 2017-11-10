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
 * @file pwrMgr.h
 *
 * @brief IARM-Bus Power Manager Public API.
 *
 * This API defines the structures and functions for the IARM-Bus Power Manager interface.
 */

/**
* @defgroup iarmmgrs
* @{
* @defgroup hal
* @{
**/

#ifndef _IARM_BUS_PWRMGR_H
#define _IARM_BUS_PWRMGR_H

#include "libIARM.h"
#include "libIBusDaemon.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" 
{
#endif

/**
 * @addtogroup IARM_PLAT_POWER_API
 * @{
 */

#define IARM_BUS_PWRMGR_NAME 						"PWRMgr"  /*!< Power manager IARM bus name */

/**
 * @brief Represents the published Events from PWR  Manager
 */
typedef enum _PWRMgr_EventId_t {
    IARM_BUS_PWRMGR_EVENT_MODECHANGED = 0,         /*!< Event to notify power mode change */
    IARM_BUS_PWRMGR_EVENT_DEEPSLEEP_TIMEOUT, /*!< Event to notify deepsleep timeout */
    IARM_BUS_PWRMGR_EVENT_RESET_SEQUENCE,        /*!< Event to notify progress of reset key sequence*/
#ifdef ENABLE_THERMAL_PROTECTION
    IARM_BUS_PWRMGR_EVENT_THERMAL_MODECHANGED,        /*!< Event to notify temperature level change */
#endif // ENABLE_THERMAL_PROTECTION
    IARM_BUS_PWRMGR_EVENT_MAX,                     /*!< Max event id from this module */
} IARM_Bus_PWRMgr_EventId_t;


/**
 *  @brief Enumerator which represents the  possible temeperature states
 */
typedef enum _IARM_Bus_PWRMgr_ThermalState_t{
    IARM_BUS_PWRMGR_TEMPERATURE_NORMAL = 0,    /* Temp is within normal operating range */
    IARM_BUS_PWRMGR_TEMPERATURE_HIGH,          /* Temp is high, but just a warning as device can still operate */
    IARM_BUS_PWRMGR_TEMPERATURE_CRITICAL       /* Temp is critical, should trigger a thermal reset */
} IARM_Bus_PWRMgr_ThermalState_t;


/**
 *  @brief Structure which holds the event data.
 */
typedef struct _PWRMgr_EventData_t {
    union {
        struct _MODE_DATA{
        	/* Declare Event Data structure for PWRMGR_EVENT_DUMMY0 */
        	IARM_Bus_PWRMgr_PowerState_t curState;	/*!< Power manager current power state */
        	IARM_Bus_PWRMgr_PowerState_t newState;	/*!< Power manager new power state */
            #ifdef ENABLE_DEEP_SLEEP
                uint32_t deep_sleep_timeout;    
            #endif 
	} state; 
	#ifdef ENABLE_THERMAL_PROTECTION
	struct _THERM_DATA{
	    	IARM_Bus_PWRMgr_ThermalState_t curLevel;
		IARM_Bus_PWRMgr_ThermalState_t newLevel;
		float curTemperature;
        } therm;
	#endif
        int32_t reset_sequence_progress;
     } data;
}IARM_Bus_PWRMgr_EventData_t;


/**
 *  @brief Structure which holds the Deep sleep timeout value.
 */
typedef struct _IARM_BUS_PWRMgr_DeepSleepTimeout_EventData_t {
	unsigned int timeout;        /*!< Timeout for deep sleep in seconds*/ 
} IARM_BUS_PWRMgr_DeepSleepTimeout_EventData_t;


/**
 * Declare RPC API names and their arguments
 */
#define IARM_BUS_PWRMGR_API_SetPowerState 		"SetPowerState" /*!< Sets the powerstate of the device*/

/**
 *  @brief Structure which holds the new powerstate to be set to the device.
 */
typedef struct _IARM_Bus_PWRMgr_SetPowerState_Param_t {
	IARM_Bus_PWRMgr_PowerState_t newState;        /*!< [in] New powerstate to be set */
} IARM_Bus_PWRMgr_SetPowerState_Param_t;

#define IARM_BUS_PWRMGR_API_GetPowerState 		"GetPowerState" /*!< Retrives current  power state of the box*/

/**
 *  @brief Structure which holds the current power state of the CPE.
 */
typedef struct _IARM_Bus_PWRMgr_GetPowerState_Param_t {
	IARM_Bus_PWRMgr_PowerState_t curState;        /*!< Current powerstate of the box*/ 
} IARM_Bus_PWRMgr_GetPowerState_Param_t;

#define IARM_BUS_PWRMGR_API_WareHouseReset		"WareHouseReset" /*!< Reset the box to warehouse state*/

/**
 *  @brief Structure which holds the ware house reset time.
 */
typedef struct _IARM_Bus_PWRMgr_WareHouseReset_Param_t {
	bool suppressReboot; /*!< STB should not be rebooted */
} IARM_Bus_PWRMgr_WareHouseReset_Param_t;

#define IARM_BUS_PWRMGR_API_SetDeepSleepTimeOut		"SetDeepSleepTimeOut" /*!< Sets the timeout for deep sleep*/

/**
 *  @brief Structure which holds the timeout value to set for Deep sleep.
 */
typedef struct _IARM_Bus_PWRMgr_SetDeepSleepTimeOut_Param_t {
	unsigned int timeout;        /*!< Timeout for deep sleep in seconds*/ 
} IARM_Bus_PWRMgr_SetDeepSleepTimeOut_Param_t;

#define IARM_BUS_PWRMGR_API_SetSleepTimer      		"SetSleepTimer" /*!< Sets sleep timer state and timeout*/
#define IARM_BUS_PWRMGR_API_GetSleepTimer      		"GetSleepTimer" /*!< Gets sleep timer state and remaining */

/**
 *  @brief Structure which holds the sleep timer information.
 */
typedef struct _IARM_Bus_PWRMgr_SleepTimer_Param_t {
	double time;        /*!< timer duration*/ 
	int    start;       /*!< timer state, started=1 or stopped=0*/ 
} IARM_Bus_PWRMgr_SleepTimer_Param_t;

#ifdef ENABLE_THERMAL_PROTECTION

/**
 *  @brief Structure which holds the data associated with thermal level.
 */
typedef struct _IARM_Bus_PWRMgr_GetThermalState_Param_t{
     IARM_Bus_PWRMgr_ThermalState_t curLevel;                     /*!<  Current Thermal level */
     float curTemperature;                                                /* !< Current temperature value */
} IARM_Bus_PWRMgr_GetThermalState_Param_t;

#define IARM_BUS_PWRMGR_API_GetThermalState             "GetThermalState" /*!< Retrieves current thermal level of the box*/

/**
 *  @brief Structure which holds the  thermal threshold value to be set to the device.
 */
typedef struct _IARM_Bus_PWRMgr_SetTempThresholds_Param_t{
     float tempHigh;                     /*!< New threshold at which TEMPERATURE_HIGH will be reported  */
     float tempCritical;                  /*!< New threshold at which TEMPERATURE_CRITICAL will be reported  */
} IARM_Bus_PWRMgr_SetTempThresholds_Param_t;

#define IARM_BUS_PWRMGR_API_SetTemperatureThresholds   "SetTemperatureThresholds" /*!< Sets the thermal threshold  for the device*/


/**
 *  @brief Structure which holds the data associated with  current temperature threshold.
 */
typedef struct _IARM_Bus_PWRMgr_GetTempThresholds_Param_t{
     float tempHigh;                     /*!< New threshold at which TEMPERATURE_HIGH will be reported  */
     float tempCritical;                  /*!< New threshold at which TEMPERATURE_CRITICAL will be reported  */
} IARM_Bus_PWRMgr_GetTempThresholds_Param_t;

#define IARM_BUS_PWRMGR_API_GetTemperatureThresholds   "GetTemperatureThresholds" /*!< Gets the thermal threshold  for the device*/

/** @brief This function will be used to initialize thermal protection thread */
extern void initializeThermalProtection();
#endif //ENABLE_THERMAL_PROTECTION

/**
 *  @brief Structure which holds the setting for whether video port is enabled in standby.
 */

#define PWRMGR_MAX_VIDEO_PORT_NAME_LENGTH 16
typedef struct _IARM_Bus_PWRMgr_StandbyVideoState_Param_t{
     char port[PWRMGR_MAX_VIDEO_PORT_NAME_LENGTH]; 
     int isEnabled;
     int result;
} IARM_Bus_PWRMgr_StandbyVideoState_Param_t;
#define IARM_BUS_PWRMGR_API_SetStandbyVideoState "SetStandbyVideoState"
#define IARM_BUS_PWRMGR_API_GetStandbyVideoState "GetStandbyVideoState"

#ifdef __cplusplus
}
#endif
#endif

/** @} */ // End of Doxygen Tag

/** @} */
/** @} */
