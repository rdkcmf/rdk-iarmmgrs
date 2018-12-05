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
 * @brief IARM-Bus Power Manager HAL Public API.
 *
 * This API defines the HAL for the IARM-Bus Power Manager interface.
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
 * @par Abbreviations
 * - BE:      Big-Endian.
 * - cb:      Callback function (suffix).
 * - DS:      Device Settings.
 * - FPD:     Front-Panel Display.
 * - HAL:     Hardware Abstraction Layer.
 * - LE:      Little-Endian.
 * - LS:      Least Significant.
 * - MBZ:     Must be zero.
 * - MS:      Most Significant.
 * - RDK:     Reference Design Kit.
 * - _t:      Type (suffix).
 *
 * @par Implementation Notes
 * -# None
 *
 */
 


/**
* @defgroup iarmmgrs
* @{
* @defgroup hal
* @{
**/


#ifndef _POWERMGR_PLAT_H
#define _POWERMGR_PLAT_H
#include "pwrMgr.h"
#ifdef __cplusplus
extern "C"
{
#endif

 /** @addtogroup IARM_BUS_PWRMGR_HAL_API IARM-Bus HAL Power Manager API.
 *  @ingroup IARM_BUS
 *
 *  Described herein are the IARM-Bus HAL types and functions that are part of the Power
 *  Manager application. This manager monitors Power IR key events and reacts to power
 *  state changes based on Comcast RDK Power Management Specification. It dispatches
 *  Power Mode Change events to IARM-Bus. All listeners should releases resources when
 *  entering POWER OFF/STANDBY state and re-acquire them when entering POWER ON state.
 *
 *  @{
 */

/**
 * @todo Should a callback function mode and registration API be added here?
 */

 /** @addtogroup IARM_BUS_PWRMGR_HAL_API IARM-Bus HAL Power Manager API.
 *  @ingroup IARM_BUS
 *
 *  Described herein are the IARM-Bus HAL types and functions that are part of the Power
 *  Manager application. This manager monitors Power IR key events and reacts to power
 *  state changes based on Comcast RDK Power Management Specification. It dispatches
 *  Power Mode Change events to IARM-Bus. All listeners should releases resources when
 *  entering POWER OFF/STANDBY state and re-acquire them when entering POWER ON state.
 *
 *  @{
 */

/**
 * @todo Should a callback function mode and registration API be added here?
 */

/**
 * @brief Initialize the underlying Power Management module.
 *
 * This function must initialize all aspects of the CPE's Power Management module.
 *
 * @param None.
 * @return    Return Code.
 * @retval    0 if successful.
 */
int PLAT_INIT(void);

/**
 * @brief Set the CPE Power State.
 *
 * This function sets the CPE's current power state to the specified state.
 *
 * @param [in]  newState    The power state to which to set the CPE.
 * @return    Return Code.
 * @retval    0 if successful.
 */
int PLAT_API_SetPowerState(IARM_Bus_PWRMgr_PowerState_t newState);

/**
 * @brief Get the CPE Power State.
 *
 * This function returns the current power state of the CPE.
 *
 * @param [in]  curState    The address of a location to hold the current power state of
 *                          the CPE on return.
 * @return    Return Code.
 * @retval    0 if successful.
 */
int PLAT_API_GetPowerState(IARM_Bus_PWRMgr_PowerState_t *curState);

#ifdef ENABLE_THERMAL_PROTECTION
/**
* @brief get current temperature of the core
*
* @param [out] state:  the current state of the core temperature
** @param [out] curTemperature:  raw temperature value of the core
*              in degrees Celsius
 @param [out] wifiTemperature:  raw temperature value of the wifi chip
*              in degrees Celsius
*
* @return Error Code
*/

int PLAT_API_GetTemperature(IARM_Bus_PWRMgr_ThermalState_t *curState, float *curTemperature, float *wifiTemperature);

 /**
 * @brief Set temperature thresholds which will determine the state returned
 *        from a call to mfrGetTemperature
 *
 * @param [in] tempHigh:  Temperature threshold at which mfrTEMPERATURE_HIGH
 *                        state will be reported.
 * @param [in] tempCritical:  Temperature threshold at which mfrTEMPERATURE_CRITICAL
 *                            state will be reported.
 *
 * @return Error Code
 */


int PLAT_API_SetTempThresholds(float tempHigh, float tempCritical);

/**
 * @brief Get temperature thresholds which will determine the state returned
 *        from a call to mfrGetTemperature
 *
 * @param [out] tempHigh:  Temperature threshold at which mfrTEMPERATURE_HIGH
 *                        state will be reported.
 * @param [out] tempCritical:  Temperature threshold at which mfrTEMPERATURE_CRITICAL
 *                            state will be reported.
 *
 * @return Error Code
 */

int PLAT_API_GetTempThresholds(float *tempHigh, float *tempCritical);



/**
 * @brief Get clock speeds for this device for the given states
 *
 * @param [out] cpu_rate_Normal:        The clock rate to be used when in the 'normal' state
 * @param [out] cpu_rate_Scaled:        The clock rate to be used when in the 'scaled' state
 * @param [out] cpu_rate_Minimal:       The clock rate to be used when in the 'minimal' state
 *
 * @return 1 if operation is attempted 0 otherwise
 */
int PLAT_API_DetemineClockSpeeds(uint32_t *cpu_rate_Normal, uint32_t *cpu_rate_Scaled, uint32_t *cpu_rate_Minimal);

/**
 * @brief Set the clock speed of the CPU
 *
 * @param [in] speed:        One of the predefined parameters
 *
 * @return 1 if operation is attempted 0 otherwise
 */
int PLAT_API_SetClockSpeed(uint32_t speed);

/**
 * @brief Set the clock speed of the CPU
 *
 * @param [out] speed:        One of the predefined parameters
 *
 * @return 1 if operation is attempted 0 otherwise
 */
int PLAT_API_GetClockSpeed(uint32_t *speed);

#endif //ENABLE_THERMAL_PROTECTION

void PLAT_Reset(IARM_Bus_PWRMgr_PowerState_t newState);

/**
 * @brief Close the IR device module.
 *
 * This function must terminate the CPE Power Management module. It must reset any data
 * structures used within Power Management module and release any Power Management
 * specific handles and resources.
 *
 * @param None.
 * @return None.
 */
void PLAT_TERM(void);

/* End of IARM_BUS_PWRMGR_HAL_API doxygen group */
/**
 * @}
 */

#ifdef __cplusplus
}
#endif
#endif


/** @} */
/** @} */
