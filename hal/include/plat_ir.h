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
 * @defgroup IARM_PLAT_IR_API IR Manager (HAL Types and Public API)
 * Described here in are the IARM-Bus HAL types and functions that are part of the
 * IR Manager application. The IR Manager application manages user input on the IR interface.
 * @ingroup IARMMGR_HAL
 */

/**
* @defgroup iarmmgrs
* @{
* @defgroup hal
* @{
*/

#ifndef _IARM_IRMGR_PLATFORM_
#define _IARM_IRMGR_PLATFORM_
#ifdef __cplusplus 
extern "C" {
#endif

/** @addtogroup IARM_PLAT_IR_API HAL
 *  @{
 */

typedef enum PLAT_xmp_tag {
   XMP_TAG_COMCAST = 0x00,
   XMP_TAG_PLATCO  = 0x01,
   XMP_TAG_UNDEFINED,
} PLAT_xmp_tag_t;

typedef enum PLAT_xmp_owner {
   XMP_OWNER_NORMAL  = 0x00,
   XMP_OWNER_PAIRING = 0x01,
   XMP_OWNER_UNDEFINED,
} PLAT_xmp_owner_t;

/**
 * @brief IR Key struct that maintains meta data
 */
typedef struct PLAT_irKey_metadata {
   int              type;  ///< Event type of key press (up, down, repeat, etc)
   int              code;  ///< Code of the pressed key (left, right, power, etc)
   PLAT_xmp_tag_t   tag;   ///< Designates which device key belongs
   PLAT_xmp_owner_t owner; ///< Designates how key should be handled
} PLAT_irKey_metadata_t;

/**
* @brief IR Input key event handler callback function type definition.
*
* The Event Data sent contains Key Type, Code, Owner, and Tag of the pressed IR key.
*
* @param[in] irKey IR Key struct that contains data on pressed IR Key
*
* @return None.
*
* @note This function must not suspend and must not invoke any blocking system
* calls. It should probably just send a message to a driver event handler task.
*/
typedef void (*PLAT_IrKeyCallback_Extended_t)(PLAT_irKey_metadata_t *irKey);

/**
 * @brief This API must registers a callback function to which IR Key events should be posted.
 *
 * This function registers the calling applications callback function. The application will then
 * be notified of IR Key events via this callback function.
 *
 * @param[in] func Function reference of the callback function to be registered.
 *
 * @return None.
*/
void PLAT_API_RegisterIRKeyCallbackExtended(PLAT_IrKeyCallback_Extended_t func);

/**
* @brief IR Input key event handler callback function type definition.
*
* The Event Data sent contains Key Type and Key Code of the pressed IR key.
*
* @param[in] keyType  Key Type (e.g. Key Down, Key Up, Key Repeat) of the pressed IR key.
* @param[in] keyCode  Key Code of the pressed IR key.
*
* @return None.
*
* @note This function must not suspend and must not invoke any blocking system
* calls. It should probably just send a message to a driver event handler task.
*/
typedef void (*PLAT_IrKeyCallback_t)(int keyType, int keyCode);

/**
 * @brief This API must registers a callback function to which IR Key events should be posted.
 *
 * This function registers the calling applications callback function. The application will then
 * be notified of IR Key events via this callback function.
 *
 * @param[in] func Function reference of the callback function to be registered.
 *
 * @return None.
*/
void PLAT_API_RegisterIRKeyCallback(PLAT_IrKeyCallback_t func);

/**
 * @brief This API initializes the underlying IR module.
 *
 * This function must initialize all the IR specific user input device modules.
 *
 * @return The status of the operation.
 * @retval Returns O if successful, appropriate error code otherwise
 */
int  PLAT_API_INIT(void);

/**
 * @brief This API is used to terminate the IR device module.
 *
 * This function must terminate all the IR specific user input device modules. It must
 * reset any data structures used within IR module and release any IR specific handles
 * and resources.
 */
void PLAT_API_TERM(void);

/**
 * @brief This function executes the  key event loop.
 *
 * This function executes the platform-specific key event loop. This will generally
 * translate between platform-specific key codes and Comcast standard keycode definitions.
 */
void PLAT_API_LOOP();

/** @} */ //End of Doxygen Tag

#ifdef __cplusplus 
}
#endif /* __cplusplus */
#endif


/** @} */
/** @} */
