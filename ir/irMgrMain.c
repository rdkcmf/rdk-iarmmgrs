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
* @defgroup ir
* @{
**/


#include "irMgrInternal.h"
#ifdef __cplusplus 
extern "C" {
#endif
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "plat_ir.h"
#include "libIBus.h"
#ifdef __cplusplus 
}
#endif
#ifdef RDK_LOGGER_ENABLED

int b_rdk_logger_enabled = 0;

void logCallback(const char *buff)
{
    LOG("%s",buff);
}

#endif
#include "irMgr.h"
#include "cap.h"
#ifdef ENABLE_SD_NOTIFY
#include <systemd/sd-daemon.h>
#endif

#define RFC_BUFFER_SIZE 100
int get_irmgr_RFC_config()
{
    int is_enabled = 0;
    const char * cmd = "tr181Set -g Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.XRRemoteAsLinuxDevice.Enable 2>&1 1>/dev/null";
    /*Note: the above redirection is necessary because tr181 prints the value of parameter to stderr instead of stdout. */
    printf("%s: getting RFC config using command \"%s\"\n", __FUNCTION__, cmd);
    FILE * pipe = popen(cmd, "r");
    if(pipe)
    {
        char buffer[RFC_BUFFER_SIZE];
        if(NULL == fgets(buffer, RFC_BUFFER_SIZE, pipe))
        {
            printf("%s: could not parse output of command <%s>.\n", __FUNCTION__, cmd);
        }
        else
        {
            printf("%s: read output \"%s\"\n", __FUNCTION__, buffer);
            if(0 == strncasecmp(buffer, "true", 4))
            {
                is_enabled = 1;
            }
        }
    }
    printf("%s: the feature is %s.\n", __FUNCTION__, (1 == is_enabled? "enabled" : "disabled"));
    pclose(pipe);
    return is_enabled;
}

static bool drop_root()
{
    bool ret = false,retval = false;
    cap_user appcaps = {{0, 0, 0, '\0', 0, 0, 0, '\0'}};
    ret = isBlacklisted();
    if(ret)
    {
         LOG("NonRoot feature is disabled\n");
    }
    else
    {
        LOG("NonRoot feature is enabled\n");
         appcaps.caps = NULL;
         appcaps.user_name = NULL;
         if(init_capability() != NULL) {
            if(drop_root_caps(&appcaps) != -1) {
               if(update_process_caps(&appcaps) != -1) {
                   read_capability(&appcaps);
                   retval = true;
               }
            }
         }
    }
    return retval;
}


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

#endif
     if(!drop_root())
    {
         LOG("drop_root function failed!\n");
    }


    int inputEnabled = get_irmgr_RFC_config();
    if(0 == inputEnabled)
    {
        //Dev-friendly testing aid in case TR181 method isn't available/functional.
        //TODO: This needs to be removed once TR181 method is verified to be fully functional
        inputEnabled = (access("/opt/remote_input_enable", F_OK) == 0);
    }
#ifdef ENABLE_LINUX_REMOTE_KEYS
    inputEnabled = 1;
    printf("%s: Linux key proceesing enabbled default for RNE build\n",__FUNCTION__);
#endif
    if (inputEnabled) {
        UINPUT_init();
        IRMgr_Register_uinput(UINPUT_GetDispatcher());
    }

    IRMgr_Start(argc, argv);
    #ifdef ENABLE_SD_NOTIFY
       sd_notifyf(0, "READY=1\n"
              "STATUS=IRMgr is Successfully Initialized\n"
              "MAINPID=%lu", (unsigned long) getpid());
    #endif

#ifdef PID_FILE_PATH
#define xstr(s) str(s)
#define str(s) #s
    // write pidfile because sd_notify() does not work inside container
    IARM_Bus_WritePIDFile(xstr(PID_FILE_PATH) "/irmgr.pid");
#endif

    IRMgr_Loop();
    IRMgr_Stop();

    if (inputEnabled) {
        UINPUT_term();
    }
    return 0;
}




/** @} */
/** @} */
