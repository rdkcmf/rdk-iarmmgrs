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
* @defgroup iarmmgrs
* @{
* @defgroup mfr
* @{
**/


#ifndef _MFR_TYPES_H
#define _MFR_TYPES_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define MFR_MAX_STR_SIZE     	(256)
#define MFR_DFAST_INPUT_BYTES   (16)
#define MFR_DFAST_OUTPUT_BYTES  (16)

typedef enum _mfrError_t {
    mfrERR_NONE = 0,
    mfrERR_GENERAL = 0x1000,
    mfrERR_INVALID_PARAM,
    mfrERR_INVALID_STATE,
    mfrERR_OPERATION_NOT_SUPPORTED,
    mfrERR_UNKNOWN,
    /* Please add Error Code here */
} mfrError_t;

typedef struct _mfrSerializedData_t {
    char * buf;                        // buffer containing the data read.
    size_t bufLen;                     // length of the data buffer;
    void (* freeBuf) (char *buf);      // function used to free the buffer. If NULL, the user does not need to free the buffer.
} mfrSerializedData_t;

typedef enum _mfrSerializedType_t {
    mfrSERIALIZED_TYPE_MANUFACTURER                     = 0,
    mfrSERIALIZED_TYPE_MANUFACTUREROUI                  = 1,
    mfrSERIALIZED_TYPE_MODELNAME                        = 2,
    mfrSERIALIZED_TYPE_DESCRIPTION                      = 3,
    mfrSERIALIZED_TYPE_PRODUCTCLASS                     = 4,
    mfrSERIALIZED_TYPE_SERIALNUMBER                     = 5,
    mfrSERIALIZED_TYPE_HARDWAREVERSION                  = 6,
    mfrSERIALIZED_TYPE_SOFTWAREVERSION                  = 7,
    mfrSERIALIZED_TYPE_PROVISIONINGCODE                 = 8,
    mfrSERIALIZED_TYPE_FIRSTUSEDATE                     = 9,
    mfrSERIALIZED_TYPE_DEVICEMAC                        = 10,
    mfrSERIALIZED_TYPE_MOCAMAC                          = 11,
    mfrSERIALIZED_TYPE_HDMIHDCP                         = 12,
    mfrSERIALIZED_TYPE_BOARDVERSION                     = 13,
    mfrSERIALIZED_TYPE_BOARDSERIALNO                    = 14,
    mfrSERIALIZED_TYPE_CMCHIPVERSION                    = 15,
    mfrSERIALIZED_TYPE_DECODERSWVERSION                 = 16,
    mfrSERIALIZED_TYPE_OSKERNELVERSION                  = 17,
    mfrSERIALIZED_TYPE_MFRLIBVERSION                    = 18,
    mfrSERIALIZED_TYPE_FRONTPANELVERSION                = 19,
    mfrSERIALIZED_TYPE_SOFTWAREIMAGEVERSION1            = 20,
    mfrSERIALIZED_TYPE_OCHDVERSION                      = 21,
    mfrSERIALIZED_TYPE_OCAPVERSION                      = 22,
    mfrSERIALIZED_TYPE_BOOTROMVERSION                   = 23,
    mfrSERIALIZED_TYPE_MODELNUMBER                      = 24,
    mfrSERIALIZED_TYPE_MODELSERIALNO                    = 25,
    mfrSERIALIZED_TYPE_VENDORNAME                       = 26,
    mfrSERIALIZED_TYPE_VENDORSERIALNO                   = 27,
    mfrSERIALIZED_TYPE_MANUFACTUREDATE                  = 28,
    mfrSERIALIZED_TYPE_BOOTIMAGENAME                    = 29,
    mfrSERIALIZED_TYPE_BOOTFIRMWAREIMAGENAME            = 30,
    mfrSERIALIZED_TYPE_BOOTAPPLICATIONIMAGENAME         = 31,
    mfrSERIALIZED_TYPE_BOOTDATAIMAGENAME                = 32,
    mfrSERIALIZED_TYPE_CACPAUTHSTATUS                   = 33,
    mfrSERIALIZED_TYPE_CACPHOSTID                       = 34,
    mfrSERIALIZED_TYPE_COMDWNLDCOSIGNNAME               = 35,
    mfrSERIALIZED_TYPE_COMDWNLVENID                     = 36,
    mfrSERIALIZED_TYPE_COMDWNLHWID                      = 37,
    mfrSERIALIZED_TYPE_COMDWNLMFRCODEACCSTRTIME         = 38,
    mfrSERIALIZED_TYPE_COMDWNLCOSIGNCODEACCSTRTIME      = 39,
    mfrSERIALIZED_TYPE_COMDWNLMFRCVCACCSTRTIME          = 40,
    mfrSERIALIZED_TYPE_COMDWNLCOSIGNCVCACCSTRTIME       = 41,
    mfrSERIALIZED_TYPE_COMDWNLDMGRSTATUS                = 42,
    mfrSERIALIZED_TYPE_CDLMFRCODEACCUPGSTRTIME          = 43,
    mfrSERIALIZED_TYPE_CDLCOSIGNCODEACCUPGSTRTIME       = 44,
    mfrSERIALIZED_TYPE_CDLFWMFRCODEACCUPGSTRTIME        = 45,
    mfrSERIALIZED_TYPE_CDLFWCOSIGNCODEACCUPGSTRTIME     = 46,
    mfrSERIALIZED_TYPE_CDLAPPMFRCODEACCUPGSTRTIME       = 47,
    mfrSERIALIZED_TYPE_CDLAPPCOSIGNCODEACCUPGSTRTIME    = 48,  
    mfrSERIALIZED_TYPE_CDLDATAMFRCODEACCUPGSTRTIME      = 49,
    mfrSERIALIZED_TYPE_CDLDATACOSIGNCODEACCUPGSTRTIME   = 50,
    mfrSERIALIZED_TYPE_CDLFWMFRCODEACCBOOTSTRTIME       = 51,
    mfrSERIALIZED_TYPE_CDLFWCOSIGNCODEACCBOOTSTRTIME    = 52,
    mfrSERIALIZED_TYPE_CDLAPPMFRCODEACCBOOTSTRTIME      = 53,
    mfrSERIALIZED_TYPE_CDLAPPCOSIGNCODEACCBOOTSTRTIME   = 54,
    mfrSERIALIZED_TYPE_CDLDATAMFRCODEACCBOOTSTRTIME     = 55,
    mfrSERIALIZED_TYPE_CDLDATACOSIGNCODEACCBOOTSTRTIME  = 56,
    mfrSERIALIZED_TYPE_IEEE1394DTCPKEYTYPE              = 57,
    mfrSERIALIZED_TYPE_IEEE1394DTCPKEYSTATUS            = 58,
    mfrSERIALIZED_TYPE_CACPHOSTROOTCERT                 = 59,
    mfrSERIALIZED_TYPE_CACPHOSTMFRCACERT                = 60,
    mfrSERIALIZED_TYPE_CACPHOSTDEVCERT                  = 61,
    mfrSERIALIZED_TYPE_CACPHOSTDEVPRIVATEKEY            = 62,
    mfrSERIALIZED_TYPE_CACPDHBG1                        = 63,
    mfrSERIALIZED_TYPE_CACPDHPN1                        = 64,
    mfrSERIALIZED_TYPE_CACPDHKEY                        = 65,
    mfrSERIALIZED_TYPE_CACPAUTHKEYH                     = 66,
    //Cable Labs Code Verification Certificates for Common Download
    mfrSERIALIZED_TYPE_COMDWNLDCLCODEVERROOTCA          = 67,
    mfrSERIALIZED_TYPE_COMDWNLDCLCVCCA                  = 68,
    mfrSERIALIZED_TYPE_COMDWNLDMFRCVC                   = 69,

    //Common Download ( code image download NV data)
    mfrSERIALIZED_TYPE_COMDWNLDCVCCAPUBKEY              = 70,

    //Cable Labs Application Code verification Certificates
    mfrSERIALIZED_TYPE_APPCLAPPCVCCA                    = 71,
    mfrSERIALIZED_TYPE_APPMANCVC                        = 72,
    mfrSERIALIZED_TYPE_SECURESERIALNUMBER               = 73,
    //Ieee1394 DTCP certificates and Keys
    mfrSERIALIZED_TYPE_IEEE1394DTCPCERTKEYS             = 74,
    mfrSERIALIZED_TYPE_IEEE1394DTCPSRM                  = 75,
    mfrSERIALIZED_TYPE_IEEE1394DTCPSEED                 = 76,

    // VL certificates and Keys
    mfrSERIALIZED_TYPE_VLCERTKEYS                       = 77,
    mfrSERIALIZED_TYPE_SNMPENTERPRISEID                 = 78,
    mfrSERIALIZED_TYPE_MAX,

} mfrSerializedType_t;

typedef enum _mfrConfigPathType_t
{   
    mfrCONFIG_TYPE_STATIC                           = 0x00000000,
    mfrCONFIG_TYPE_DYNAMIC                          = 0x00000001,
    mfrCONFIG_TYPE_VOLATILE                         = 0x00000002,
    mfrCONFIG_TYPE_COMMON_DOWNLOAD                  = 0x00000003,
    mfrCONFIG_TYPE_DVR_CONTENT                      = 0x00000004,
    mfrCONFIG_TYPE_INVALID                          = 0x7FFFFFFF

} mfrConfigPathType_t;

typedef enum _mfrImageType_t 
{
    mfrIMAGE_TYPE_CDL = 0,
    mfrIMAGE_TYPE_RCDL,

    mfrUPGRADE_IMAGE_MONOLITHIC   = 0x100,
    mfrUPGRADE_IMAGE_FIRMWARE, 
    mfrUPGRADE_IMAGE_APPLICATION,
    mfrUPGRADE_IMAGE_DATA,

    mfrUPGRADE_IMAGE_LAST      

} mfrImageType_t;

typedef enum _mfrUpgradeProgress_t 
{
    mfrUPGRADE_PROGRESS_NOT_STARTED = 0,//not started
    mfrUPGRADE_PROGRESS_STARTED,   	//in progress
    mfrUPGRADE_PROGRESS_ABORTED,   	//failed
    mfrUPGRADE_PROGRESS_COMPLETED, 	//success
    mfrUPGRADE_PROGRESS_BOOTED_WITH_UPGRADED_IMAGE,
    mfrUPGRADE_PROGRESS_BOOTED_WITH_REVERTED_IMAGE,
    mfrUPGRADE_PROGRESS_NONE
} mfrUpgradeProgress_t;

typedef enum _mfrCableCardType_t
{   
    mfrCABLECARD_TYPE_CISCO                                 = 0x00000001,
    mfrCABLECARD_TYPE_MOTO                                  = 0x00000002,
    mfrCABLECARD_TYPE_INVALID                               = 0x7FFFFFFF,

} mfrCableCardType_t;

#define    mfrBOOT_INSTANCE_CURRENT  0
#define    mfrBOOT_INSTANCE_PREVIOUS 1
#define    mfrBOOT_INSTANCE_UPGRADED 2
/* 
struct mfrDFAST2Params_t
This structure is used by the call mfrGetDFAST2Data 
*/
typedef struct _mfrDFAST2Params_t
{   
    unsigned int seedIn [MFR_DFAST_INPUT_BYTES];
    unsigned int keyOut [MFR_DFAST_OUTPUT_BYTES];
} mfrDFAST2Params_t;

/* 
struct mfrHostFirmwareInfo_t
This structure is used by the call mfrSetHostFirmwareInfo
*/ 
typedef struct _mfrHostFrmwareInfo_t
{   
    char            firmwareVersion[MFR_MAX_STR_SIZE];
    int             firmwareDay;
    int             firmwareMonth;
    int             firmwareYear;
} mfrHostFirmwareInfo_t;


typedef struct _mfrUpgradeStatus_t 
{
  mfrUpgradeProgress_t progress;
  mfrError_t error;
  int percentage;
} mfrUpgradeStatus_t;

typedef struct _mfrUpgradeStatusNotify_t 
{
    void * cbData;
    void (*cb) (mfrUpgradeStatus_t status, void *cbData);
    int interval; // number of seconds between two callbacks. 0 means invoking callback only once to report final upgrade result.
} mfrUpgradeStatusNotify_t;


/**
 * @brief crypto funcionality. 
 * This function should be called to encrypt a block of data 
 *
 * @param [in]  :  pClearText: uncrypted data.
 * @param [out] :  pCipherText: crypted data.
 *
 * @return Error Code:  If error code is returned, the crypto has failed, and pCipherText is not usable.
 */
typedef mfrError_t (*mfrEncryptFunction_t)(const mfrSerializedData_t *pClearText,  mfrSerializedData_t  *pCipherText);

/**
 * @brief crypto funcionality. 
 * This function should be called to decrypt a block of data that are encrypted by mfrEncryptFunction_t;
 *
 * @param [in] :  pCipherText: crypted data.
 * @param [out]  :  pClearText: uncrypted data.
 *
 * @return Error Code:  If error code is returned, the crypto has failed, and pClearText is not usable.
 */
typedef mfrError_t (*mfrDecryptFunction_t)(const mfrSerializedData_t *pCipherText, mfrSerializedData_t  *pClearText);

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the mfr library.  
 * This function should be call once before the functions in this API can be used.
 *
 * @param [in]  :  None.
 * @param [out] :  None. 
 *
 * @return Error Code:  If error code is returned, the init has failed.
 */
mfrError_t mfr_init( void );

/**
 * @brief Shutdowns the mfr library.  
 * This function should be called to free resources in mfr library while shutdown.
 *
 * @param [in]  :  None.
 * @param [out] :  None. 
 *
 * @return Error Code:  If error code is returned, the shutdown has failed.
 */
mfrError_t mfr_shutdown( void );

/**
 * @brief Retrieve Serialized Read-Only data from device.  The serialized data is returned as a byte stream. It is upto the
 * application to deserialize and make sense of the data returned.   Please note that even if the serialized data returned is 
 * "string", the buffer is not required to contain the null-terminator.
 *
 * The 'crypto' parameter, if not null, the implementation should used the given crypto function to encrypt the data befure 
 * returning.
 *
 * @param [in] type:  specifies the serialized data to read.
 * @param [in] data:  contains information about the returned data (buffer location, length, and func to free the buffer). 
 * @param [in] crypto: crypto used to protect data across MFR interface.
 *
 * @return Error Code:  If error code is returned, the read has failed and values in data should not be used.
 */
mfrError_t mfrGetSerializedData( mfrSerializedType_t type,  mfrSerializedData_t *data, mfrEncryptFunction_t crypto);


/**
 * @brief Write Serialized data to device.  The serialized data written as a byte stream. 
 * The 'crypto' parameter, if not null, the implementation should used the given crypto function to decrypt the data before 
 * setting the serialize data.
 *
 * @param [in] type:  specifies the serialized data to written.
 * @param [in] data:  contains information about the data to be written (buffer location, length, free function to be called to free buffer). 
 * @param [in] crypto: crypto used to protect data across MFR interface.
 *
 * @return Error Code:  If error code is returned, the write has failed
 */
mfrError_t mfrSetSerializedData( mfrSerializedType_t type,  mfrSerializedData_t *data, mfrDecryptFunction_t crypto);


/**
* @brief Validate and  Write the image into flash.  The process should follow these major steps:
*    1) Validate headers, manufacturer, model.
*    2) Perform Signature check.
*    3) Flash the image.
*    4) Perform CRC on flashed nvram.
*    5) Update boot params and switch banks to prepare for a reboot event.
*    6) All upgrades should be done in the alternate bank. The current bank should not be disturbed at any cost.
*         i.e. a second upgrade will before a reboot will overwrite the non-current bank only.
*
*    State Transition:
*    0) Before the API is invoked, the Upgrade process should be in PROGRESS_NOT_STARTED state. 
*    1) After the API returnes with success, the Upgrade process moves to PROGRESS_STARTED state.
*    2) After the API returnew with error,   the Upgrade process stays in PROGRESS_NO_STARTED state. Notify function will not be invoked.
*    3) The notify function is called at regular interval with proress = PROGRESS_STARTED.
*    4) The last invocation of notify function should have either progress = PROGRESS_COMPLETED or progress = PROGRESS_ABORTED with error code set.
*
*    Application can either rely on the 'notify' callback to monitor image write progress, or use the mfrGetImageWriteProgress() 
*
* @param [in] name:  the path of the image file in the STB file system.
* @param [in] path:  the filename of the image file. 
* @param [in] type:  the type (e.g. format, signature type) of the image.  This can dictate the handling of the image within the MFR library.
* @param[in] callback: function to provide status of the image flashing process.  
* @return Error Code:  If error code is returned, the image flashing is not initiated..
*/
mfrError_t mfrWriteImage(const char *name,  const char *path, mfrImageType_t type,  mfrUpgradeStatusNotify_t notify);

/**
* @brief Set the the progress of image upgrade. 
*
* @return Error Code:  Return mfrERR_NONE if operation is successful, mfrERR_GENERAL if it fails
*/
mfrError_t mfrSetImageWriteProgress(const char * imageName, mfrImageType_t imageType, mfrUpgradeProgress_t progress);

/**
* @brief Retrieves the last/latest progress of image upgrade
*     If the upgrade is in not progress, the progress returned should be same as the final value returned by the 'notify' function 
*     in mfrWriteImage(), or as the value set by the last invocation of mfrSetImageWriteProgress.
*
*     If the upgrade is in progress, the progress returned should be the latest status when the function is called.
*
* @return Error Code:  Return mfrERR_NONE if operation is successful, mfrERR_GENERAL if it fails
*/
mfrError_t mfrGetImageWriteProgress(const char * imageName,mfrImageType_t imageType,mfrUpgradeProgress_t *progress);

/**
* @brief Delete the P-DRI image if it is present
*
* @return Error Code:  Return mfrERR_NONE if P-DRI is succesfully deleted or not present, mfrERR_GENERAL if deletion fails
*/
mfrError_t mfrDeletePDRI(void);

/**
* @brief Delete the platform images
*
* @return Error Code:  Return mfrERR_NONE if the images are scrubbed, mfrERR_GENERAL if scrubbing fails
*/
mfrError_t mfrScrubAllBanks(void);

/**
* @brief Reboots the box with image of given name. 
*
* @param [in] name:  the path of the image file to reboot the box with 
*
* @return Error Code:  Return mfrERR_NONE if operation is successful, mfrERR_GENERAL if it fails
*/
mfrError_t mfrReboot(const char *imageName);

/**
* @brief Sets the cable card type. 
*
* @param [in] type:  the cablecard type used by host.
*
* @return Error Code:  Return mfrERR_NONE if operation is successful, mfrERR_GENERAL if it fails
*/
mfrError_t mfrSetCableCardType(mfrCableCardType_t type);

/**
* @brief Sets the Host firmware information. 
*
* @param [in] firmwareInfo:  the host's firmware information  
*
* @return Error Code:  Return mfrERR_NONE if operation is successful, mfrERR_GENERAL if it fails
*/
mfrError_t mfrSetHostFirmwareInfo(const mfrHostFirmwareInfo_t *firmwareInfo);

/**
* @brief Retrieve the image name used by box when booting  up. (I.e. current active image) 
*
* @param [in] bootInstance: which bootup instance 
* @param [out] bootImageName: buffer to hold the image name
* @param [in] len : size of the output buffer
*
* @return Error Code:  Return mfrERR_NONE if operation is successful, mfrERR_GENERAL if it fails
*/
mfrError_t mfrGetBootImageName(int bootInstance, char *bootImageName, int *len, mfrImageType_t bootImageType);

/**
* @brief Retrieves the path configuration name. 
*
* @param [in] type : which configure path 
* @param [out] path: buffer to hold the path name
* @param [in] len : size of the output buffer
*
* @return Error Code:  Return mfrERR_NONE if operation is successful, mfrERR_GENERAL if it fails
*/
mfrError_t mfrGetPathConfiguration(mfrConfigPathType_t type, char *path, int *len);


/**
* @brief Triggers DFAST operations. 
*
* @param [in] params : DFAST2 params used by MFR implementation 
*
* @return Error Code:  Return mfrERR_NONE if operation is successful, mfrERR_GENERAL if it fails
*/
mfrError_t mfrGetDFAST2Data(mfrDFAST2Params_t *params);

#ifdef __cplusplus
};
#endif

#endif


/** @} */
/** @} */
