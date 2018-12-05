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
/********************************************************************
*********************************************************************
*
*  File        :  mfr_wifi.h
*
*  Description :  Offsets of various access start times.
*
*
*********************************************************************
********************************************************************/

#ifndef __MFR_WIFI_API_H__
#define __MFR_WIFI_API_H__


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 *
 * @brief Manufacturer Library WIFI Public API.
 *
 * This API defines the Manufacturer API for the Device Settings WIFI interface.
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

/** @addtogroup MFR_WIFI_API Device Settings MFR WIFI Public API.
 *  @ingroup MFR_WIFI
 *  @{
 */

/**
 * @brief Initialize the underlying WIFI data sub-system.
 *
 * This function must initialize the WIFI data module and any associated data
 * structures.
 *
 * @param None
 * @return    Error Code.
 * @retval    ::WIFI_API_RESULT
 */
WIFI_API_RESULT WIFI_Init(void);

/**
 * @brief Terminate the the WIFI data sub-system.
 *
 * This function resets any data structures used within the platform wifi module,
 * and releases the wifi specific device handles.
 *
 * @param None
 * @return    Error Code.
 * @retval    ::WIFI_API_RESULT
 */
WIFI_API_RESULT WIFI_DeInit(void);

/**
 * @brief
 *
 * @param None
 * @return    Error Code.
 * @retval    ::WIFI_API_RESULT
 */
WIFI_API_RESULT WIFI_GetCredentials(WIFI_DATA *pData);

/**
 * @brief
 *
 * @param None
 * @return    Error Code.
 * @retval    ::WIFI_API_RESULT
 */
WIFI_API_RESULT WIFI_SetCredentials(WIFI_DATA *pData);

/**
 * @brief
 *
 * @param None
 * @return    Error Code.
 * @retval    ::WIFI_API_RESULT
 */
WIFI_API_RESULT WIFI_EraseAllData (void);


#ifdef __cplusplus
}
#endif

#endif // __MFR_WIFI_API_H__
