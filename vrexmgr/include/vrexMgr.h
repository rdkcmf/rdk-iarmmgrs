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
* @file vrexMgr.h
*
* @brief IARM-Bus VREX Manager Public API.
*
* This API defines the public interfaces for Voice Recognition manager
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
/** @defgroup IARM_BUS IARM-VREX Manager API
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

/** @todo complete the implementation with adding required RPCs.
*/

/** @addtogroup IARM_BUS_VREXMGR_API IARM-VREX Manager API.
*  @ingroup IARM_BUS
*
*  Described herein are the VREX Manager events and functions.
*
*  @{
*/




/**
* @defgroup iarmmgrs
* @{
* @defgroup vrexmgr
* @{
**/


#ifndef _IARM_BUS_VREXMGR_H
#define _IARM_BUS_VREXMGR_H

#include "libIARM.h"

#ifdef __cplusplus
extern "C" 
{
#endif

#define IARM_BUS_VREXMGR_NAME 					    "VREXMgr" /*!< IARM BUS  name for VREX manager */

#define IARM_BUS_VREXMGR_SPEECH_FRAGMENTLENGTH  1024
#define IARM_BUS_VREXMGR_SPEECH_MIME_LENGTH       64
#define IARM_BUS_VREXMGR_SPEECH_SUBTYPE_LENGTH    64
#define IARM_BUS_VREXMGR_SPEECH_LANG_LENGTH        3

#define IARM_BUS_VREXMGR_ERROR_MESSAGE_LENGTH    256

/*! Published Events from sys manager  */
typedef enum _VREXMgr_EventId_t {
    IARM_BUS_VREXMGR_EVENT_MOTION = 0,
	IARM_BUS_VREXMGR_EVENT_SPEECH,
	IARM_BUS_VREXMGR_EVENT_ERROR,
	IARM_BUS_VREXMGR_EVENT_SUCCESS,
	IARM_BUS_VREXMGR_EVENT_SETTINGS,
	IARM_BUS_VREXMGR_EVENT_MAX				/*!< Max Event Id */
} IARM_Bus_VREXMgr_EventId_t;

typedef enum _VREXMgr_SpeechType {
    IARM_BUS_VREXMGR_SPEECH_BEGIN = 0,
    IARM_BUS_VREXMGR_SPEECH_FRAGMENT,
    IARM_BUS_VREXMGR_SPEECH_END
} IARM_Bus_VREXMgr_SpeechType_t;

// Note that these *must* match AudioReasonEnd enum in vrexSession.h
typedef enum _VREXMgr_SpeechEndReason {
    IARM_BUS_VREXMGR_SPEECH_DONE = 0,
    IARM_BUS_VREXMGR_SPEECH_ABORT,
    IARM_BUS_VREXMGR_SPEECH_ERROR
} IARM_Bus_VREXMgr_SpeechEndReason_t;

typedef struct {
    /// @brief payload description for the Motion Event
    /// @brief The acceleration values in G's against the various axis
    double   x;
    double   y;
    double   z;
} _MOTION_EVENT;

typedef enum _VREXMgr_VoiceCallType {
	KNOCKKNOCK=1,
	SENDSTATE=2,
	SPEECH=3
}IARM_Bus_VREXMgr_VoiceCallType_t;


typedef struct {
	unsigned char jsonData[IARM_BUS_VREXMGR_ERROR_MESSAGE_LENGTH];
} _JSON_EVENT;

typedef struct {
    unsigned char     type;
    union {
        struct _SPEECH_BEGIN_EVENT {
            /// @brief The mime type of the data audio/vnd.wave;codec=1 for PCM or audio/x-adpcm for ADPCM
            /// see http://www.isi.edu/in-notes/rfc2361.txt wrt mime types
            unsigned char   mimeType[IARM_BUS_VREXMGR_SPEECH_MIME_LENGTH];
            /// @brief The subtype (using exising definitions such as PCM_16_16K, PCM_16_32K, PCM_16_22K)
            unsigned char   subType[IARM_BUS_VREXMGR_SPEECH_SUBTYPE_LENGTH];
            /// @brief The language code (ISO-639-1)
            unsigned char   language[IARM_BUS_VREXMGR_SPEECH_LANG_LENGTH];
        } begin;

        struct _SPEECH_FRAGMENT_EVENT {
            /// @brief The length of the fragment
            unsigned long   length;

            /// @brief The voice data itself
            unsigned char   fragment[IARM_BUS_VREXMGR_SPEECH_FRAGMENTLENGTH];
        } fragment;

        struct _SPEECH_END_EVENT {
            /// @brief The reason for ending
            unsigned char   reason;
        } end;
    } data;
} _SPEECH_EVENT;

/*! Event Data associated with Sys Managers */ 
typedef struct _IARM_BUS_VREXMgr_EventData_t {
    /// @brief A unique identifier of the remote that transmitted the motion command
    unsigned char       remoteId;
	union {
		_MOTION_EVENT motionEvent;
        _SPEECH_EVENT speechEvent;
        _JSON_EVENT jsonEvent;
	} data;
} IARM_Bus_VREXMgr_EventData_t;


/*! Possible Firmware Download state */
typedef enum _VREXMgr_CARD_FWDNLDState_t
{
  IARM_BUS_VREXMGR_CARD_FWDNLD_START,
  IARM_BUS_VREXMGR_CARD_FWDNLD_COMPLETE,
} IARM_Bus_VREXMGR_FWDNLDState_t;



/*
 * Declare RPC API names and their arguments
 * TODO: This will be populated later code download functions
 */
#define IARM_BUS_VREXMGR_API_AnnounceFirmware   "AnnounceFirmware"

#ifdef __cplusplus
}
#endif

#endif

/* End of IARM_BUS_VREXMGR_API doxygen group */
/**
 * @}
 */



/** @} */
/** @} */
