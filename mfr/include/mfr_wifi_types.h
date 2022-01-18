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
*  File        :  mfr_wifi_types.h
*
*  Description :  Types and definitions for WIFI data module
*
*
*********************************************************************
********************************************************************/

#ifndef __MFR_WIFI_TYPES_H__
#define __MFR_WIFI_TYPES_H__

#ifdef __cplusplus
extern "C" {
#endif



#define WIFI_MAX_SSID_LEN        32       //!< Maximum SSID name
#define WIFI_MAX_PASSWORD_LEN    64       //!< Maximum password length

#define WIFI_DATA_LENGTH        512

typedef enum _WIFI_API_RESULT
{
    WIFI_API_RESULT_SUCCESS = 0,
    WIFI_API_RESULT_FAILED,
    WIFI_API_RESULT_NULL_PARAM,
    WIFI_API_RESULT_INVALID_PARAM,
    WIFI_API_RESULT_NOT_INITIALIZED,
    WIFI_ERR_OPERATION_NOT_SUPPORTED

} WIFI_API_RESULT;

// NOTE: this order needs to correspond to whats in DRI code.
typedef enum _WIFI_DATA_TYPE
{
    WIFI_DATA_UNKNOWN = 0,
    WIFI_DATA_SSID,
    WIFI_DATA_PASSWORD
} WIFI_DATA_TYPE;

typedef struct
{
    char cSSID[WIFI_MAX_SSID_LEN+1];
    char cPassword[WIFI_MAX_PASSWORD_LEN+1];
    int  iSecurityMode;
} WIFI_DATA;

#ifdef __cplusplus
}
#endif

#endif  //__MFR_WIFI_TYPES_H__
