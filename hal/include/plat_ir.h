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
* @brief IARM-Bus IR Manager HAL Public API.
*
* This API defines the HAL for the IARM-Bus IR Manager interface.
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
* - BE:       ig-Endian.
* - cb:       allback function (suffix).
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

/** @defgroup IARM_BUS IARM-Bus HAL API
*   @ingroup IARM_RDK
*
*  IARM-Bus is a platform agnostic Inter-process communication (IPC) interface. It allows
*  applications to communicate with each other by sending Events or invoking Remote
*  Procedure Calls. The common programming APIs offered by the RDK IARM-Bus interface is
*  independent of the operating system or the underlying IPC mechanism.
*
*  Two applications connected to the same instance of IARM-Bus are able to exchange events
*  or RPC calls. On a typical system, only one instance of IARM-Bus instance is needed. If
*  desired, it is possible to have multiple IARM-Bus instances. However, applications
*  connected to different buses will not be able to communicate with each other.
*/





/**
* @defgroup iarmmgrs
* @{
* @defgroup hal
* @{
**/


#ifndef _IARM_IRMGR_PLATFORM_
#define _IARM_IRMGR_PLATFORM_
#ifdef __cplusplus 
extern "C" {
#endif

/** @addtogroup IARM_BUS_IRMGR_HAL_API IARM-Bus HAL IR Manager API.
*  @ingroup IARM_BUS
*
*  Described here in are the IARM-Bus HAL types and functions that are part of the
*  IR Manager application. The IR Manager application manages user input on the IR
*  interface.
*
*  @{
*/

/**
* @brief IR Input key event handler callback function type definition.
*
* The Event Data sent contains Key Type and Key Code of the pressed IR key.
*
* @param [in]  keyType  Key Type (e.g. Key Down, Key Up, Key Repeat) of the pressed IR key.
* @param [in]  keyCode  Key Code of the pressed IR key.
*
* @return None.
*
* @execution Synchronous.
* @sideeffect None.
*
* @note This function must not suspend and must not invoke any blocking system
* calls. It should probably just send a message to a driver event handler task.
*/
typedef void (*PLAT_IrKeyCallback_t)(int keyType, int keyCode);


/**
* @brief Register callback function to which IR Key events should be posted.
*
* This function registers the calling applications callback function.  The application
* will then be notified of IR Key events via this callback function.
*
* @param [in]  func    Function reference of the callback function to be registered.
* @return None.
*/
void PLAT_API_RegisterIRKeyCallback(PLAT_IrKeyCallback_t func);

/**
 * @brief Initialize the underlying IR module.
 *
 * This function must initialize all the IR specific user input device modules.
 *
 * @param     None.
 * @return    Return Code.
 * @retval    0 if successful.
 */
int  PLAT_API_INIT(void);

/**
 * @brief Close the IR device module.
 *
 * This function must terminate all the IR specific user input device modules. It must
 * reset any data structures used within IR module and release any IR specific handles
 * and resources.
 *
 * @param None.
 * @return None.
 */
void PLAT_API_TERM(void);

/**
 * @brief Execute key event loop.
 *
 * This function executes the platform-specific key event loop. This will generally
 * translate between platform-specific key codes and Comcast standard keycode definitions.
 *
 * @param None.
 * @return None.
 */
void PLAT_API_LOOP();

/* End of IARM_BUS_IRMGR_HAL_API doxygen group */
/**
 * @}
 */
#ifdef __cplusplus 
}
#endif /* __cplusplus */
#endif


/** @} */
/** @} */
