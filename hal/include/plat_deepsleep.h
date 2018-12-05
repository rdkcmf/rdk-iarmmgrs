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
 * @file
 *
 * @brief IARM-Bus Power Manager Public API.
 *
 * This API defines the structures and functions for the 
 * IARM-Bus Power Manager interface.
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
 
#ifndef _PLATDEEPSLEEP_H
#define _PLATDEEPSLEEP_H

#include "deepSleepMgr.h"

/** @addtogroup IARM_BUS_PWRMGR_API IARM-Bus Power Manager API.
 *  @ingroup IARM_BUS
 *
 *  Described herein are the IARM-Bus types and functions that are part of the Power
 *  Manager application. This manager monitors Power IR key events and reacts to power
 *  state changes based on Comcast RDK Power Management Specification. It dispatches
 *  Power Mode Change events to IARM-Bus. All listeners should releases resources when
 *  entering POWER OFF/STANDBY state and re-acquire them when entering POWER ON state.
 *
 *  @{
 */

#ifdef __cplusplus
extern "C" 
{
#endif

/*
 * Declare RPC API names and their arguments
 */

/**
 * @brief Initialize the underlying Deep Sleep Management module.
 *
 * This function must initialize all aspects of the CPE's Power Management module.
 *
 * @param None.
 * @return    Return Code.
 * @retval    0 if successful.
 */
int PLAT_DS_INIT(void);

/**
 * @brief Enter Deep Sleep Mode.
 *
 * This function sets the CPE's Power State to Deep Sleep.
 *
 * @param [in]  None.
 * @return    None.
 */
void PLAT_DS_SetDeepSleep(void);


/**
 * @brief Get the CPE Power State.
 *
 * This function returns the current power state of the CPE.
 *
 * @param [in]  None
 * @return    None
 */
void PLAT_DS_DeepSleepWakeup(void);


void PLAT_DS_Reboot(void);

/**
 * @brief Close the Deep Sleep manager.
 *
 * This function must terminate the CPE Deep Sleep Management module. It must reset any data
 * structures used within Power Management module and release any Power Management
 * specific handles and resources.
 *
 * @param None.
 * @return None.
 */
void PLAT_DS_TERM(void);

/* End of IARM_BUS_PWRMGR_HAL_API doxygen group */
/**
 * @}
 */


#ifdef __cplusplus
}
#endif
#endif

/* End of IARM_BUS_PWRMGR_API doxygen group */
/**
 * @}
 */

