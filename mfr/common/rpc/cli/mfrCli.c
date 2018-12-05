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


#include <string.h>
#include "iarmUtil.h"
#include "libIARM.h"
#include "libIBus.h"
#include "libIARMCore.h"
#include "mfrMgr.h"
#include "mfrCrypto.h"
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

static char writeImageCbModule[MAX_BUF] = "";
static char cliName[32];
static mfrUpgradeStatusNotify_t cb_notify = {0,};  

static void _eventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
    if ((strcmp(owner, IARM_BUS_MFRLIB_NAME)  == 0) && eventId == IARM_BUS_MFRMGR_EVENT_STATUS_UPDATE)
    {
       IARM_BUS_MfrMgr_StatusUpdate_EventData_t *param = (IARM_BUS_MfrMgr_StatusUpdate_EventData_t *)data;

       if(cb_notify.cb)
       {
          cb_notify.cb(param->status, cb_notify.cbData);
       } 
    }
}

mfrError_t mfr_init( )
{
    IARM_Result_t rpcRet = IARM_RESULT_SUCCESS;
    printf("Initializing MFR module \n");
    rpcRet = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME ,
                            (char *)IARM_BUS_MFRLIB_API_Init,
                            NULL,
                            0);

    if (IARM_RESULT_SUCCESS == rpcRet)
    {
		printf("Iarm call returned success in %s \n",__func__);
        return mfrERR_NONE;
    }

    IARM_Bus_RegisterEventHandler(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRMGR_EVENT_STATUS_UPDATE, _eventHandler);
    return mfrERR_GENERAL;
}

mfrError_t mfr_shutdown( )
{
    IARM_Result_t rpcRet = IARM_RESULT_SUCCESS;
    rpcRet = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME ,
                            (char *)IARM_BUS_MFRLIB_API_Shutdown,
                            NULL,
                            0);
    if (IARM_RESULT_SUCCESS == rpcRet)
    {
        return mfrERR_NONE;
    }
    return mfrERR_GENERAL;
}

mfrError_t mfrGetSerializedData( mfrSerializedType_t type, mfrSerializedData_t *data, mfrEncryptFunction_t crypto)
{
    IARM_Result_t rpcRet;
    IARM_Bus_MFRLib_SerializedData_Param_t param;

    param.type = type;
    param.bufLen = 0;

    data->buf = NULL;
    data->bufLen = 0;

    if(NULL !=crypto)
    {
        strcpy(param.crypto,"mfrCrypto_Encrypt");
    }
    else
    {
    	strcpy(param.crypto,"");
    }  
    
    do{
    
        printf("%s: Calling Get Serialized..\n",__func__);
        rpcRet = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME ,
                            (char *)IARM_BUS_MFRLIB_API_GetSerializedData,
                            (void*)&param,
                            sizeof(param));
    
        if (IARM_RESULT_SUCCESS != rpcRet)
        {
            printf("%s: failed while trying to get size of param..\n",__func__);
           // break;
        }
        
        data->buf = (char *) malloc (param.bufLen);
        memcpy(data->buf,(char *)param.buffer, param.bufLen);
        data->bufLen = param.bufLen;
        data->freeBuf = free;
    
    }while (0);

    if(rpcRet == IARM_RESULT_SUCCESS)
        return mfrERR_NONE; 

    return mfrERR_GENERAL;
}    

mfrError_t mfrSetSerializedData( mfrSerializedType_t type, mfrSerializedData_t *data, mfrDecryptFunction_t crypto)
{
    IARM_Result_t rpcRet;
    IARM_Bus_MFRLib_SerializedData_Param_t param;
    do{
            param.bufLen = (data->bufLen < MAX_SERIALIZED_BUF)?data->bufLen:MAX_SERIALIZED_BUF;

            if(NULL !=crypto)
            {
                strcpy(param.crypto,"mfrCrypto_Decrypt");
            }
	   		else
	    	{
				strcpy(param.crypto,"");
	    	} 
            param.type = type;
            memcpy(((char*)param.buffer), data->buf,param.bufLen);

            rpcRet = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME ,
                            (char *)IARM_BUS_MFRLIB_API_SetSerializedData,
                            (void*)&param,
                            sizeof(IARM_Bus_MFRLib_SerializedData_Param_t));

            if (IARM_RESULT_SUCCESS != rpcRet)
            {
                printf("%s: failed while trying to write param..\n",__func__);
                rpcRet = IARM_RESULT_IPCCORE_FAIL;
                break;
            }
        
    }while(0);

    if(rpcRet == IARM_RESULT_SUCCESS)
        return mfrERR_NONE; 
    return mfrERR_GENERAL;
}

mfrError_t mfrWriteImage(const char *name,  const char *path, mfrImageType_t type,  mfrUpgradeStatusNotify_t notify)
{
    IARM_Result_t rpcRet = IARM_RESULT_SUCCESS;
    IARM_Bus_MFRLib_WriteImage_Param_t param;

    strcpy(param.name,name);
    strcpy(param.path,path);
    param.type = type;   
    param.interval = notify.interval;

    rpcRet = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME ,
                            (char *)IARM_BUS_MFRLIB_API_WriteImage,
                            (void *)&param,
                            sizeof(param));

    if(IARM_RESULT_SUCCESS == rpcRet)
    {
        cb_notify = notify;
        return mfrERR_NONE;
    }
    return mfrERR_GENERAL;
}

mfrError_t mfrSetImageWriteProgress(const char * imageName, mfrImageType_t imageType, mfrUpgradeProgress_t progress)
{

    IARM_Result_t rpcRet = IARM_RESULT_SUCCESS;
    IARM_Bus_MFRLib_SetImageWriteProgress_Param_t param;

    strcpy(param.imageName,imageName);
    param.imageType = imageType;   
    param.progress = progress;

    rpcRet = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME ,
                            (char *)IARM_BUS_MFRLIB_API_SetImageWriteProgress,
                            (void *)&param,
                            sizeof(param));

    if(IARM_RESULT_SUCCESS == rpcRet)
    {
        return mfrERR_NONE;
    }
    return mfrERR_GENERAL;
}


mfrError_t mfrGetImageWriteProgress(const char * imageName,mfrImageType_t imageType,mfrUpgradeProgress_t *progress)
{

    IARM_Result_t rpcRet = IARM_RESULT_SUCCESS;
    IARM_Bus_MFRLib_GetImageWriteProgress_Param_t param;
    printf("Inside %s  \n",__func__);
    strcpy(param.imageName,imageName);
    param.imageType = imageType;
    rpcRet = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME ,
                            (char *)IARM_BUS_MFRLIB_API_GetImageWriteProgress,
                            (void *)&param,
                            sizeof(param));

    if(IARM_RESULT_SUCCESS == rpcRet)
    {
		*progress = param.progress;
        return mfrERR_NONE;
    }
    return mfrERR_GENERAL;
}


mfrError_t mfrDeletePDRI( )
{
    IARM_Result_t rpcRet = IARM_RESULT_SUCCESS;
    rpcRet = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME ,
                            (char *)IARM_BUS_MFRLIB_API_DeletePDRI,
                            NULL,
                            0);
    if(IARM_RESULT_SUCCESS == rpcRet)
    {
        return mfrERR_NONE;
    }
    return mfrERR_GENERAL;
}

mfrError_t mfrScrubAllBanks( )
{
    IARM_Result_t rpcRet = IARM_RESULT_SUCCESS;
    rpcRet = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME ,
                            (char *)IARM_BUS_MFRLIB_API_ScrubAllBanks,
                            NULL,
                            0);
    if(IARM_RESULT_SUCCESS == rpcRet)
    {
        return mfrERR_NONE;
    }
    return mfrERR_GENERAL;
}


mfrError_t mfrReboot(const char *imageName)
{

    IARM_Result_t rpcRet = IARM_RESULT_SUCCESS;
    IARM_Bus_MFRLib_Reboot_Param_t param;

    strcpy(param.imageName,imageName);

    rpcRet = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME ,
                            (char *)IARM_BUS_MFRLIB_API_Reboot,
                            (void *)&param,
                            sizeof(param));

    if(IARM_RESULT_SUCCESS == rpcRet)
    {
        return mfrERR_NONE;
    }
    return mfrERR_GENERAL;
}


mfrError_t mfrSetCableCardType(mfrCableCardType_t type)
{

    IARM_Result_t rpcRet = IARM_RESULT_SUCCESS;
    IARM_Bus_MFRLib_SetCableCardType_Param_t param;

    param.type = type;   

    rpcRet = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME ,
                            (char *)IARM_BUS_MFRLIB_API_SetCableCardType,
                            (void *)&param,
                            sizeof(param));

    if(IARM_RESULT_SUCCESS == rpcRet)
    {
        return mfrERR_NONE;
    }
    return mfrERR_GENERAL;
}

mfrError_t mfrSetHostFirmwareInfo(const mfrHostFirmwareInfo_t *firmwareInfo)
{

    IARM_Result_t rpcRet = IARM_RESULT_SUCCESS;
    IARM_Bus_MFRLib_SetHostFirmwareInfo_Param_t param;

    strcpy(param.version,firmwareInfo->firmwareVersion);
    param.day = firmwareInfo-> firmwareDay;   
    param.month = firmwareInfo->firmwareMonth;   
    param.year = firmwareInfo->firmwareYear;   

    rpcRet = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME ,
                            (char *)IARM_BUS_MFRLIB_API_SetHostFirmwareInfo,
                            (void *)&param,
                            sizeof(param));

    if(IARM_RESULT_SUCCESS == rpcRet)
    {
        return mfrERR_NONE;
    }
    return mfrERR_GENERAL;
}

mfrError_t mfrGetBootImageName(int bootInstance, char *bootImageName, int *len, mfrImageType_t bootImageType)
{

    IARM_Result_t rpcRet = IARM_RESULT_SUCCESS;
    IARM_Bus_MFRLib_GetBootImageName_Param_t param;

//  strcpy(param.imageName,bootImageName);
    param.bootInstance = bootInstance;   
//  param.len = *len;
    param.type = bootImageType;
    rpcRet = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME ,
                            (char *)IARM_BUS_MFRLIB_API_GetBootImageName,
                            (void *)&param,
                            sizeof(param));

    if(IARM_RESULT_SUCCESS == rpcRet)
    {
	*len = param.len;
	memcpy(bootImageName,param.imageName, *len);
	return mfrERR_NONE;
    }
    return mfrERR_GENERAL;
}
mfrError_t mfrGetPathConfiguration(mfrConfigPathType_t type, char *path, int *len)
{

    IARM_Result_t rpcRet = IARM_RESULT_SUCCESS;
    IARM_Bus_MFRLib_GetPathConfiguration_Param_t param;

  //strcpy(param.path,path);
    param.type = type;   
  //param.len = *len;
    rpcRet = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME ,
                            (char *)IARM_BUS_MFRLIB_API_GetPathConfiguration,
                            (void *)&param,
                            sizeof(param));

    if(IARM_RESULT_SUCCESS == rpcRet)
    {
	*len = param.len;
	if(NULL != path)
	{
	    memcpy(path, param.path, param.len);
	    return mfrERR_NONE;
	}
    }
    return mfrERR_GENERAL;
}

mfrError_t mfrGetDFAST2Data(mfrDFAST2Params_t *params)
{

    IARM_Result_t rpcRet = IARM_RESULT_SUCCESS;
    IARM_Bus_MFRLib_GetDFAST2Data_Param_t param;

    memcpy(param.seedIn,params->seedIn,sizeof(param.seedIn));
    memcpy(param.keyOut,params->keyOut,sizeof(param.keyOut));

    rpcRet = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME ,
                            (char *)IARM_BUS_MFRLIB_API_GetDFAST2Data,
                            (void *)&param,
                            sizeof(param));

    if(IARM_RESULT_SUCCESS == rpcRet)
    {
        return mfrERR_NONE;
    }
    return mfrERR_GENERAL;
}
    


/** @} */
/** @} */
