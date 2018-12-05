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
* @defgroup dsmgr
* @{
**/


#include "dsMgrInternal.h"
#include "dsserverregisterlog.h"
#include "dshalregisterlog.h"
#ifdef __cplusplus 
extern "C" {
#endif
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef __cplusplus 
}
#endif
#ifdef ENABLE_SD_NOTIFY
#include <systemd/sd-daemon.h>
#endif
#ifdef RDK_LOGGER_ENABLED

int b_rdk_logger_enabled = 0;

void logCallback(const char *buff)
{
    DEBUG_LOG("%s",buff);
}
void dslogCallback(int priority,const char *buff)
{
    if(priority == 0)
    {
        INFO_LOG("%s",buff);
    }
    else if(priority == 1)
    {
        WARN_LOG("%s",buff);
    }
    else if(priority == 2)
    {
        ERROR_LOG("%s",buff);
    }
    else if(priority == 3)
    {
        DEBUG_LOG("%s",buff);
    }
} 
  
#endif


int main(int argc, char *argv[])
{
    const char* debugConfigFile = NULL;
    int itr=0;

        while (itr < argc)
        {
                if(strcmp(argv[itr],"--debugconfig")==0)
                {
                        itr++;
                        if (itr < argc)
                        {
                                debugConfigFile = argv[itr];
                        }
                        else
                        {
                                break;
                        }
                }
                itr++;
        }

#ifdef RDK_LOGGER_ENABLED

    if(rdk_logger_init(debugConfigFile) == 0) b_rdk_logger_enabled = 1;
    IARM_Bus_RegisterForLog(logCallback);
    DSServer_RegisterForLog(dslogCallback);

//    TODO: This could be enabled only after ensuring that all platforms have DSHal_RegisterForLog implemented 
//    in devicesettings/hal 
//
//    DSHal_RegisterForLog(dslogCallback);

#endif
    DSMgr_Start();
    #ifdef ENABLE_SD_NOTIFY
           sd_notifyf(0, "READY=1\n"
           "STATUS=DsMgr is Successfully Initialized\n"
              "MAINPID=%lu", (unsigned long) getpid());
    #endif
    DSMgr_Loop();
    DSMgr_Stop();
    return 0;
}




/** @} */
/** @} */
