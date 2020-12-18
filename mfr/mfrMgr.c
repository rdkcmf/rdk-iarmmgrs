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


#include <stdio.h>
#include <memory.h>
#include <dlfcn.h>
#include <unistd.h>

#include "mfrMgrInternal.h"
#include "mfrMgr.h"
#include "libIARMCore.h"
#include "safec_lib.h"

static int is_connected = 0;

static char writeImageCbModule[MAX_BUF] = "";

static mfrUpgradeStatus_t lastStatus;

static IARM_Result_t getSerializedData_(void *arg)
{

    IARM_Result_t retCode = IARM_RESULT_IPCCORE_FAIL;
    IARM_Bus_MFRLib_GetSerializedData_Param_t *param = (IARM_Bus_MFRLib_GetSerializedData_Param_t *)arg;
    mfrError_t err = mfrERR_NONE;
    mfrSerializedData_t data;
    errno_t safec_rc = -1;

    err = mfrGetSerializedData((mfrSerializedType_t)(param->type), &(data));

    if(mfrERR_NONE == err)
    {
        safec_rc = memcpy_s(param->buffer, sizeof(param->buffer), data.buf, data.bufLen);

        if(safec_rc != EOK)
            {
                ERR_CHK(safec_rc);
                if(data.freeBuf)
                    {
                        data.freeBuf(data.buf);
                    }
                return IARM_RESULT_INVALID_PARAM;
            }

        param->bufLen = data.bufLen;
        if(data.freeBuf)
        {
             data.freeBuf(data.buf);
        }
	retCode=IARM_RESULT_SUCCESS;
    }
    return retCode;
}

static IARM_Result_t deletePDRI_(void *arg)
{
    typedef mfrError_t (*mfrDeletePDRI_t)(void);

#ifndef RDK_MFRLIB_NAME
    LOG("Please define RDK_MFRLIB_NAME\r\n");
    return IARM_RESULT_INVALID_STATE;
#else
    static mfrDeletePDRI_t func = 0;
    if (func == 0) {
        void *dllib = dlopen(RDK_MFRLIB_NAME, RTLD_LAZY);
        if (dllib) {
            func = (mfrDeletePDRI_t) dlsym(dllib, "mfrDeletePDRI");
            if (func) {
                LOG("mfrDeletePDRI(void) is defined and loaded\r\n");
            }
            else {
                LOG("mfrDeletePDRI(void) is not defined\r\n");
				 dlclose(dllib);
                return IARM_RESULT_INVALID_STATE;
            }
            dlclose(dllib);
        }
        else {
            LOG("Opening RDK_MFRLIB_NAME [%s] failed\r\n", RDK_MFRLIB_NAME);
            return IARM_RESULT_INVALID_STATE;
        }
    }

    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;
    mfrError_t err = mfrERR_NONE;

    if (func) {
    
        err = func();

        if(mfrERR_NONE == err)
        {
            LOG("Calling mfrDeletePDRI returned err %d\r\n", err);
            retCode = IARM_RESULT_SUCCESS;
        }
    }
    return retCode;
#endif

}

static IARM_Result_t scrubAllBanks_(void *arg)
{
    typedef mfrError_t (*mfrScrubAllBanks_t)(void);

#ifndef RDK_MFRLIB_NAME
    LOG("Please define RDK_MFRLIB_NAME\r\n");
    return IARM_RESULT_INVALID_STATE;
#else
    static mfrScrubAllBanks_t func = 0;
    if (func == 0) {
        void *dllib = dlopen(RDK_MFRLIB_NAME, RTLD_LAZY);
        if (dllib) {
            func = (mfrScrubAllBanks_t) dlsym(dllib, "mfrScrubAllBanks");
            if (func) {
                LOG("mfrScrubAllBanks(void) is defined and loaded\r\n");
            }
            else {
                LOG("mfrScrubAllBanks(void) is not defined\r\n");
                dlclose(dllib);
				return IARM_RESULT_INVALID_STATE;
            }
            dlclose(dllib);
        }
        else {
            LOG("Opening RDK_MFRLIB_NAME [%s] failed\r\n", RDK_MFRLIB_NAME);
            return IARM_RESULT_INVALID_STATE;
        }
    }

    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;
    mfrError_t err = mfrERR_NONE;

    if (func) {
    
        err = func();

        if(mfrERR_NONE == err)
        {
            LOG("Calling mfrScrubAllBanks returned err %d\r\n", err);
            retCode = IARM_RESULT_SUCCESS;
        }
    }
    return retCode;
#endif

}
#ifdef ENABLE_MFR_WIFI
static IARM_Result_t mfrWifiEraseAllData_(void *arg)
{
    typedef mfrError_t (*mfrWifiEraseAllData_t)(void);

#ifndef RDK_MFRLIB_NAME
    LOG("Please define RDK_MFRLIB_NAME\r\n");
    return IARM_RESULT_INVALID_STATE;
#else
    static mfrWifiEraseAllData_t func = 0;
    if (func == 0) {
        void *dllib = dlopen(RDK_MFRLIB_NAME, RTLD_LAZY);
        if (dllib) {
            func = (mfrWifiEraseAllData_t) dlsym(dllib, "WIFI_EraseAllData");
            if (func) {
                LOG("mfrWifiEraseAllData(void) is defined and loaded\r\n");
            }
            else {
                LOG("mfrWifiEraseAllData(void) is not defined\r\n");
                dlclose(dllib);
                                return IARM_RESULT_INVALID_STATE;
            }
            dlclose(dllib);
        }
        else {
            LOG("Opening RDK_MFRLIB_NAME [%s] failed\r\n", RDK_MFRLIB_NAME);
            return IARM_RESULT_INVALID_STATE;
        }
    }

    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;
    WIFI_API_RESULT err = WIFI_API_RESULT_SUCCESS;    

    if (func) {

        err = func();

        if(WIFI_API_RESULT_SUCCESS == err)
        {
            LOG("Calling mfrWifiEraseAllData returned err %d\r\n", err);
            retCode = IARM_RESULT_SUCCESS;
        }
    }
    return retCode;
#endif

}
static IARM_Result_t mfrWifiCredentials_(void *arg)
{

    IARM_Result_t retCode = IARM_RESULT_IPCCORE_FAIL;
    IARM_BUS_MFRLIB_API_WIFI_Credentials_Param_t *param = (IARM_BUS_MFRLIB_API_WIFI_Credentials_Param_t *)arg;
    WIFI_API_RESULT err = WIFI_API_RESULT_SUCCESS;
    WIFI_DATA data;
    errno_t safec_rc = -1;

    if (param->requestType == WIFI_SET_CREDENTIALS)
    {
            safec_rc = strcpy_s(data.cSSID, sizeof(data.cSSID), param->wifiCredentials.cSSID);
            if(safec_rc != EOK)
            {
                ERR_CHK(safec_rc);
                return IARM_RESULT_INVALID_PARAM;
            }

            safec_rc = strcpy_s(data.cPassword, sizeof(data.cPassword), param->wifiCredentials.cPassword);
            if(safec_rc != EOK)
            {
                ERR_CHK(safec_rc);
                return IARM_RESULT_INVALID_PARAM;
            }

            LOG("WIFI_SetCredentials ssid = %s \r\n", param->wifiCredentials.cSSID);
            err = WIFI_SetCredentials(&(data));
            if(WIFI_API_RESULT_SUCCESS  == err)
            {
                retCode=IARM_RESULT_SUCCESS;
            }
	    else
		LOG("Calling WIFI_SetCredentials returned err %d\r\n", err);
    }
    else
    {
        err = WIFI_GetCredentials(&(data));

        if(WIFI_API_RESULT_SUCCESS  == err)
        {
            safec_rc = strcpy_s(param->wifiCredentials.cSSID, sizeof(param->wifiCredentials.cSSID), data.cSSID);
            if(safec_rc != EOK)
            {
                ERR_CHK(safec_rc);
                return IARM_RESULT_INVALID_PARAM;
            }

            safec_rc = strcpy_s(param->wifiCredentials.cPassword, sizeof(param->wifiCredentials.cPassword), data.cPassword);
            if(safec_rc != EOK)
            {
                ERR_CHK(safec_rc);
                return IARM_RESULT_INVALID_PARAM;
            }

            retCode=IARM_RESULT_SUCCESS;
            LOG("WIFI_GetCredentials ssid = %s \r\n", data.cSSID);
        }
	else
	    LOG("Calling WIFI_GetCredentials returned err %d\r\n", err);
    }
    param->returnVal=err;
    return retCode;
}
#endif

static void writeImageCb(mfrUpgradeStatus_t status, void *cbData)
{
    IARM_Bus_MFRLib_CommonAPI_WriteImageCb_Param_t param;
    errno_t safec_rc = -1;

    param.status = status;

    LOG("In writeImage callback: error = %d, percentage = %02.02\n",status.error,status.percentage/100,status.percentage%100);

    if(cbData)
    {
         safec_rc = memcpy_s(param.cbData, sizeof(param.cbData), cbData, sizeof(param.cbData));
         if(safec_rc != EOK)
            {
                ERR_CHK(safec_rc);
                return;
            }

    } 

    IARM_Bus_Call(writeImageCbModule,IARM_BUS_MFRLIB_COMMON_API_WriteImageCb, &param, sizeof(IARM_Bus_MFRLib_CommonAPI_WriteImageCb_Param_t)); 
    
    lastStatus = status;
}

static IARM_Result_t writeImage_(void *arg)
{
    
    typedef mfrError_t (*mfrWriteImage_)(const char *,  const char *, mfrImageType_t ,  mfrUpgradeStatusNotify_t );

#ifndef RDK_MFRLIB_NAME
    LOG("Please define RDK_MFRLIB_NAME\r\n");
    LOG("Exiting writeImage_\n");
    return IARM_RESULT_INVALID_STATE;
#else
    static mfrWriteImage_ func = 0;
    LOG("In writeImage_\n");
    if (func == 0) {
        void *dllib = dlopen(RDK_MFRLIB_NAME, RTLD_LAZY);
        if (dllib) {
            func = (mfrWriteImage_) dlsym(dllib, "mfrWriteImage");
            if (func) {
                LOG("mfrWriteImage is defined and loaded\r\n");
            }
            else {
                LOG("mfrWriteImage is not defined\r\n");
                LOG("Exiting writeImage_\n");
				dlclose(dllib);
                return IARM_RESULT_INVALID_STATE;
            }
            dlclose(dllib);
        }
        else {
            LOG("Opening RDK_MFRLIB_NAME [%s] failed\r\n", RDK_MFRLIB_NAME);
            LOG("Exiting writeImage_\n");
            return IARM_RESULT_INVALID_STATE;
        }
    }

    IARM_Result_t retCode = IARM_RESULT_INVALID_STATE;
    mfrError_t err = mfrERR_NONE;
    errno_t safec_rc = -1;
    if (func) {

        IARM_Bus_MFRLib_WriteImage_Param_t *pParam = (IARM_Bus_MFRLib_WriteImage_Param_t *) arg;
        
        mfrUpgradeStatusNotify_t notifyStruct;

        notifyStruct.cbData = pParam->cbData;
        notifyStruct.interval = pParam->interval;
        notifyStruct.cb = writeImageCb;

        safec_rc = strcpy_s(writeImageCbModule, sizeof(writeImageCbModule),pParam->callerModuleName);
        if(safec_rc != EOK)
            {
                ERR_CHK(safec_rc);
                return IARM_RESULT_INVALID_PARAM;
            }

        lastStatus.progress = mfrUPGRADE_PROGRESS_NOT_STARTED;
   
        err = func(pParam->name, pParam->path, pParam->type, notifyStruct);

        if(mfrERR_NONE == err)
        {
            LOG("Calling mfrWriteImage returned err %d\r\n", err);
            retCode = IARM_RESULT_SUCCESS;

            /* Poll for upgrade start */
            while( lastStatus.progress == mfrUPGRADE_PROGRESS_NOT_STARTED )
            {
               LOG("Waiting for upgrade to start\n");
               sleep(1);
            }
        
            /* Poll for completion */    
            while( lastStatus.progress == mfrUPGRADE_PROGRESS_STARTED )
            {
               LOG("Waiting for upgrade to complete\n");
               sleep(1);
            }

            LOG("Update process complete..\n");

            if(lastStatus.progress != mfrUPGRADE_PROGRESS_COMPLETED)
            {
               retCode = IARM_RESULT_IPCCORE_FAIL;
            }		

        }
    }
    LOG("Exiting writeImage_\n");
    return retCode;
#endif

}

IARM_Result_t MFRLib_Start(void)
{
    IARM_Result_t err = IARM_RESULT_SUCCESS;

    LOG("Entering [%s] - [%s] - disabling io redirect buf\r\n", __FUNCTION__, IARM_BUS_MFRLIB_NAME);
    setvbuf(stdout, NULL, _IOLBF, 0);

    do{

        if(mfr_init()!= mfrERR_NONE)
        {
            LOG("Error initializing MFR library..\n");
	    err = IARM_RESULT_INVALID_STATE;
	    break;
        }

        err = IARM_Bus_Init(IARM_BUS_MFRLIB_NAME);

        if(IARM_RESULT_SUCCESS != err)
        {
            LOG("Error initializing IARM.. error code : %d\n",err);
            break;
        }

        err = IARM_Bus_Connect();

        if(IARM_RESULT_SUCCESS != err)
        {
            LOG("Error connecting to IARM.. error code : %d\n",err);
            break;
        }
        is_connected = 1;
        err = IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_GetSerializedData,getSerializedData_);
        if(IARM_RESULT_SUCCESS != err)
        {
            LOG("Error registering call(getSerializedData) in IARM.. error code : %d\n",err);
            break;
        }

        err = IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_DeletePDRI, deletePDRI_);
        if(IARM_RESULT_SUCCESS != err)
        {
            LOG("Error registering call(deletePDRI) in IARM.. error code : %d\n",err);
            break;
        }

        err = IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_ScrubAllBanks, scrubAllBanks_);

        if(IARM_RESULT_SUCCESS != err)
        {
            LOG("Error registering call(scrubAllBanks) in IARM.. error code : %d\n",err);
            break;
        }
        err = IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_WriteImage, writeImage_);

        if(IARM_RESULT_SUCCESS != err)
        {
            LOG("Error registering call(writeImage) in IARM.. error code : %d\n",err);
            break;
        }
#ifdef ENABLE_MFR_WIFI
        err = IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_WIFI_EraseAllData, mfrWifiEraseAllData_);

        if(IARM_RESULT_SUCCESS != err)
        {
            LOG("Error registering call(wifiEraseAllData) in IARM.. error code : %d\n",err);
            break;
        }
        err = IARM_Bus_RegisterCall(IARM_BUS_MFRLIB_API_WIFI_Credentials, mfrWifiCredentials_);

        if(IARM_RESULT_SUCCESS != err)
        {
            LOG("Error registering call(mfrWifiGetCredentials) in IARM.. error code : %d\n",err);
            break;
        }
        
#endif
    }while(0);

    if(err != IARM_RESULT_SUCCESS)
    {
        if(is_connected)
        {
            IARM_Bus_Disconnect();
        }
        IARM_Bus_Term();
    }

    return err;

}

IARM_Result_t MFRLib_Stop(void)
{
    if(is_connected)
	{
		IARM_Bus_Disconnect();
		IARM_Bus_Term();
	}
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t MFRLib_Loop()
{
    time_t curr = 0;
    while(1)
    {
        time(&curr);
        LOG("I-ARM MFR Lib: HeartBeat at %s\r\n", ctime(&curr));
        sleep(300);
    }
    return IARM_RESULT_SUCCESS;
}





/** @} */
/** @} */
