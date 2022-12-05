/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2021 RDK Management
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
* @defgroup deepsleepmgr
* @{
**/


#ifdef ENABLE_DEEP_SLEEP

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <sys/stat.h>
#include <pthread.h>
#include "iarmUtil.h"
#include "pwrMgr.h"
#include "libIBus.h"
//#include "plat_deepsleep.h"
#ifdef __cplusplus
}
#endif
#include "pwrlogger.h"
#include "plat_power.h"
#include "deepSleepMgr.h"
#include "libIBusDaemon.h"
#include "irMgr.h"
#include "comcastIrKeyCodes.h"
#include "manager.hpp"


/* For glib APIs*/
#include <glib.h>
/* Deep Sleep static Functions */
static IARM_Result_t _DeepSleepWakeup(void *arg);
static IARM_Result_t _SetDeepSleepTimer(void *arg);
static IARM_Result_t _GetDeepSleepStatus(void *arg);
static IARM_Result_t _GetLastWakeupReason(void *arg);
static IARM_Result_t _GetLastWakeupKeyCode(void *arg);

IARM_Result_t GetPwrMgrDeepSleepStatus(int *status);
IARM_Result_t PwrMgrDeepSleepWakeup(IARM_Bus_CommonAPI_PowerPreChange_Param_t *arg);

static gboolean heartbeatMsg(gpointer data);
static gboolean deep_sleep_delay_timer_fn(gpointer data);

/* Variables for Deep Sleep */
static uint32_t deep_sleep_delay_timeout = 0;
static uint32_t deep_sleep_wakeup_timer = 0;
static bool nwStandbyMode_gs = false;
GMainLoop *deepSleepMgr_Loop = NULL;
static GMainLoop *mainloop = NULL;
static guint dsleep_delay_event_src = 0;
static DeepSleepStatus_t IsDeviceInDeepSleep = DeepSleepStatus_NotStarted;
static gboolean isLxcRestart = 0;
extern void  _handleDeepsleepTimeoutWakeup ();


void IARM_Bus_PWRMGR_RegisterDeepSleepAPIs()
{
    /*  Register for IARM events */
    IARM_Bus_RegisterCall(IARM_BUS_COMMON_API_DeepSleepWakeup,_DeepSleepWakeup);
    IARM_Bus_RegisterCall(IARM_BUS_DEEPSLEEPMGR_API_SetDeepSleepTimer, _SetDeepSleepTimer);
    IARM_Bus_RegisterCall("GetDeepSleepStatus", _GetDeepSleepStatus);
    IARM_Bus_RegisterCall(IARM_BUS_DEEPSLEEPMGR_API_GetLastWakeupReason, _GetLastWakeupReason);
    IARM_Bus_RegisterCall(IARM_BUS_DEEPSLEEPMGR_API_GetLastWakeupKeyCode, _GetLastWakeupKeyCode);
}


void PwrMgrDeepSleepTimeout()
{
#if !defined (_DISABLE_SCHD_REBOOT_AT_DEEPSLEEP)
    /*Scheduled maintanace reboot is disabled for XiOne/Llama/Platco*/
    system("echo 0 > /opt/.rebootFlag");
    system(" echo `/bin/timestamp` ------------- Reboot timer expired while in Deep Sleep --------------- >> /opt/logs/receiver.log");
    system("sleep 5; /rebootNow.sh -s DeepSleepMgr -o 'Rebooting the box due to reboot timer expired while in Deep Sleep...'");
#endif /*End of _DISABLE_SCHD_REBOOT_AT_DEEPSLEEP*/
}



static void SetPwrMgrDeepSleepMode(void *data)
{
    LOG("[%s:%d] Entering...\n",__FUNCTION__,__LINE__);
    IARM_Bus_PWRMgr_EventData_t *param = (IARM_Bus_PWRMgr_EventData_t *)data;

    if(IsDeviceInDeepSleep != DeepSleepStatus_InProgress)
    {
        LOG("Event IARM_BUS_PWRMGR_EVENT_MODECHANGED: State Changed %d -- > %d\r\n",
            param->data.state.curState, param->data.state.newState);

        if(IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP == param->data.state.newState)
        {

            LOG("GOT EVENT TO GO TO DEEP SLEEP \r\n");

            /*Here we are changing the DeesSleep State assuming that the api would succeed.
            This is because, once the deep sleep api is called, the CPU would be idle.*/
            /*Call Deep Sleep API*/
            deep_sleep_wakeup_timer = param->data.state.deep_sleep_timeout;
            FILE *fpST = NULL;
            uint32_t SleepTimeInSec = 0;
            struct stat buf;
            IsDeviceInDeepSleep = DeepSleepStatus_InProgress;

            /* Read the Delay Sleep Time  */
            fpST = fopen("/tmp/deepSleepTimer","r");

            if (NULL != fpST) {
                if(0 > fscanf(fpST,"%d",&SleepTimeInSec)) {
                    __TIMESTAMP();
                    LOG("Error: fscanf on SleepTimeInSec failed");
                } else {
                    deep_sleep_delay_timeout = SleepTimeInSec ;
                    __TIMESTAMP();
                    LOG(" /tmp/ override Deep Sleep Time is %d \r\n",deep_sleep_delay_timeout);
                }
                fclose (fpST);
            }

            LOG("Deep Sleep wakeup time value is %d Secs.. \r\n",deep_sleep_wakeup_timer);
            if (deep_sleep_delay_timeout) {
                /* start a Deep sleep timer thread */
                LOG("Schedule Deep SLeep After %d Sec.. \r\n",deep_sleep_delay_timeout);
                dsleep_delay_event_src = g_timeout_add_seconds ((guint) deep_sleep_delay_timeout,deep_sleep_delay_timer_fn,deepSleepMgr_Loop);
            } else {
                LOG("Enter to Deep sleep Mode..stop Receiver with sleep 10 before DS \r\n");
                system("sleep 5");
                if ((stat("/lib/systemd/system/lxc.service", &buf) == 0) && (stat("/opt/lxc_service_disabled",&buf) !=0)) {
                    LOG("stopping lxc service\r\n");
                    system("systemctl stop lxc.service");
                    isLxcRestart = 1;
                } else {
#ifndef ENABLE_LLAMA_PLATCO_SKY_XIONE
                    LOG("stopping xre-receiver service\r\n");
                    system("systemctl stop xre-receiver.service");
                    LOG("stopping wpeframework service\r\n");
                    system("systemctl stop wpeframework.service");
#else
                    LOG("Skipping Stopping service while entering DEEPSLEEP.\n");
#endif
#ifndef ENABLE_LLAMA_PLATCO
                    LOG("Unmounting SDcard partition\r\n");
                    system("sh /lib/rdk/disk_checkV2 deepsleep ON");
#else
                    LOG("Skipping Unmounting SDcard partition.\r\n");
#endif
                }
#ifndef ENABLE_LLAMA_PLATCO_SKY_XIONE
                LOG("Enter to Deep sleep Mode..stop fog service before DS \r\n");
                system("systemctl stop fog.service");
#endif
                int status = -1;
                int retryCount = 0;
                bool userWakeup = 0;
                while(retryCount< 5) {
                    LOG("Device entering Deep sleep Mode.. \r\n");
                    userWakeup = 0;
#ifdef ENABLE_LLAMA_PLATCO_SKY_XIONE
                    nwStandbyMode_gs = param->data.state.nwStandbyMode;
                    LOG("\nCalling PLAT_DS_SetDeepSleep with nwStandbyMode: %s\n",
                        nwStandbyMode_gs?("Enabled"):("Disabled"));
#endif
                    LOG("Device entered to Deep sleep Mode.. \r\n");
                    status = PLAT_DS_SetDeepSleep(deep_sleep_wakeup_timer,&userWakeup, nwStandbyMode_gs);
                    LOG("Device resumed from Deep sleep Mode. \r\n");

                    if(status != 0) {
                        sleep(5);
                        retryCount++;
                        if(retryCount >= 5) {
                            LOG("ERROR: Device failed to enter into Deep sleep Mode.. \r\n");
                            IsDeviceInDeepSleep = DeepSleepStatus_Failed;
                            break;
                        }
                    } else {
                        IsDeviceInDeepSleep = DeepSleepStatus_Completed;
                        break;
                    }
                }

                if (userWakeup) {
                    /* Always send KED_DEEPSLEEP_WAKEUP when user action wakes the device from deep sleep. Previously this was sent
                    if we woke from a GPIO event, however there are cases where IR events aren't always passed when exiting
                    deep sleep resulting in the device not fully resuming. To resolve this we will ensure the WAKE event
                    is always sent here */
                    LOG("Resumed due to user action. Sending KED_DEEPSLEEP_WAKEUP. \r\n");
                    IARM_Bus_IRMgr_EventData_t eventData;
                    eventData.data.irkey.keyType = KET_KEYDOWN;
                    eventData.data.irkey.keyCode = KED_DEEPSLEEP_WAKEUP;
                    eventData.data.irkey.isFP = 0;
                    eventData.data.irkey.keySrc = IARM_BUS_IRMGR_KEYSRC_IR;

                    IARM_Bus_BroadcastEvent(IARM_BUS_IRMGR_NAME, (IARM_EventId_t) IARM_BUS_IRMGR_EVENT_IRKEY, (void *)&eventData, sizeof(eventData));
                    eventData.data.irkey.keyType = KET_KEYUP;
                    IARM_Bus_BroadcastEvent(IARM_BUS_IRMGR_NAME, (IARM_EventId_t) IARM_BUS_IRMGR_EVENT_IRKEY, (void *)&eventData, sizeof(eventData));
                } else {
                    LOG("Resumed without user action. Not sending KED_DEEPSLEEP_WAKEUP. \r\n");
                }

                #ifdef USE_WAKEUP_TIMER_EVT
                DeepSleep_WakeupReason_t wakeupReason = DEEPSLEEP_WAKEUPREASON_UNKNOWN;
                int reasonStatus = PLAT_DS_GetLastWakeupReason(&wakeupReason);
                if (DEEPSLEEP_WAKEUPREASON_TIMER == wakeupReason){
                    LOG("Calling IARM_BUS_PWRMGR_API_handleDeepsleepTimeoutWakeup on wakeupReason:%d \n", wakeupReason);
                    _handleDeepsleepTimeoutWakeup();
                }
                #endif
            }
        }
    } else {
        LOG("[%s]DeepSleepStatus InProgress. Failed to Update DeepSleep ModeChange \r\n", __FUNCTION__);
    }

    if(data) {
        free(data);
        data = NULL;
    }
    LOG("[%s:%d] Exiting...\r\n",__FUNCTION__,__LINE__);
}

static void* DeepsleepStateChangeThread(void* arg)
{
    SetPwrMgrDeepSleepMode(arg);
    pthread_exit(NULL);
}

void HandleDeepSleepStateChange(void *data)
{

    pthread_t pwrMgrDSEventThreadId;

    IARM_Bus_PWRMgr_EventData_t *param = (IARM_Bus_PWRMgr_EventData_t *)data;

    LOG("Event IARM_BUS_PWRMGR_EVENT_MODECHANGED: State Changed %d -- > %d\r\n",
        param->data.state.curState, param->data.state.newState);

    int err = pthread_create(&pwrMgrDSEventThreadId, NULL, DeepsleepStateChangeThread, data);
    if(err != 0) {
        LOG("DeepsleepStateChangeThread thread create failed \r\n");
    } else {
        err = pthread_detach(pwrMgrDSEventThreadId);
        if(err != 0) {
            LOG("DeepsleepStateChangeThread thread detach failed \r\n");
        }
    }
}

static IARM_Result_t _DeepSleepWakeup(void *arg)
{
    IARM_Bus_CommonAPI_PowerPreChange_Param_t *param = (IARM_Bus_CommonAPI_PowerPreChange_Param_t *) arg;

#ifndef ENABLE_LLAMA_PLATCO_SKY_XIONE
    const char* syscommand = "systemctl restart mocadriver.service &";
#endif

    LOG("RPC IARM_BUS_COMMON_API_DeepSleepWakeup : State Changed %d -- > %d\r", param->curState, param->newState);

    /* Support Deep sleep to Power ON, Light Sleep and Standby Transition. */
    if( (IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP == param->curState) &&
            (IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP != param->newState)) {
        LOG("GOT EVENT TO EXIT FROM DEEP SLEEP \r\n");

        /*Call Wake up API */
        PLAT_DS_DeepSleepWakeup();

        /* Remove the Event source  */
        if(dsleep_delay_event_src) {
            g_source_remove(dsleep_delay_event_src);
            dsleep_delay_event_src = 0;
        }

        if(IsDeviceInDeepSleep) {
            /*Restart Moca service when exit from Deep Sleep*/
#ifndef ENABLE_LLAMA_PLATCO_SKY_XIONE
            LOG("Restarting Moca Service After Waking up from Deep Sleep.. \r\n");
            system(syscommand);
#endif
#ifndef ENABLE_LLAMA_PLATCO
            LOG("Mounting SDcard partition After Waking up from Deep Sleep..\r\n");
            system("sh /lib/rdk/disk_checkV2 deepsleep OFF");
#endif
#ifndef ENABLE_LLAMA_PLATCO_SKY_XIONE
            LOG("Restarting fog Service After Waking up from Deep Sleep.. \r\n");
            system("systemctl restart fog.service &");
#endif
            if (isLxcRestart) {
                LOG("Restarting Lxc Service After Waking up from Deep Sleep\r\n");
                system("systemctl restart lxc.service");
                isLxcRestart = 0;
            } else {
#ifndef ENABLE_LLAMA_PLATCO_SKY_XIONE
                LOG("Restarting WPEFramework Service After Waking up from Deep Sleep\r\n");
                system("systemctl restart wpeframework.service");
                LOG("Restarting Receiver Service After Waking up from Deep Sleep\r\n");
                system("systemctl restart xre-receiver.service");
#else
                LOG("Skipping restart of Services in Sky Platform\n");
#endif

            }
        }
        IsDeviceInDeepSleep = DeepSleepStatus_NotStarted;

        LOG("Device woke up from Deep sleep Mode.. \r\n");
    }
    return IARM_RESULT_SUCCESS;
}



IARM_Result_t PwrMgrDeepSleepWakeup(IARM_Bus_CommonAPI_PowerPreChange_Param_t *arg)
{
    IARM_Bus_CommonAPI_PowerPreChange_Param_t *param = (IARM_Bus_CommonAPI_PowerPreChange_Param_t *) arg;

#ifndef ENABLE_LLAMA_PLATCO_SKY_XIONE
    const char* syscommand = "systemctl restart mocadriver.service &";
#endif

    LOG("RPC IARM_BUS_COMMON_API_DeepSleepWakeup : State Changed %d -- > %d\r", param->curState, param->newState);

    /* Support Deep sleep to Power ON, Light Sleep and Standby Transition. */
    if( (IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP == param->curState) &&
            (IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP != param->newState)) {
        LOG("GOT EVENT TO EXIT FROM DEEP SLEEP \r\n");

        /*Call Wake up API */
        PLAT_DS_DeepSleepWakeup();

        /* Remove the Event source  */
        if(dsleep_delay_event_src) {
            g_source_remove(dsleep_delay_event_src);
            dsleep_delay_event_src = 0;
        }

        if(IsDeviceInDeepSleep) {
            /*Restart Moca service when exit from Deep Sleep*/
#ifndef ENABLE_LLAMA_PLATCO_SKY_XIONE
            LOG("Restarting Moca Service After Waking up from Deep Sleep.. \r\n");
            system(syscommand);
#endif
#ifndef ENABLE_LLAMA_PLATCO
            LOG("Mounting SDcard partition After Waking up from Deep Sleep..\r\n");
            system("sh /lib/rdk/disk_checkV2 deepsleep OFF");
#endif
#ifndef ENABLE_LLAMA_PLATCO_SKY_XIONE
            LOG("Restarting fog Service After Waking up from Deep Sleep.. \r\n");
            system("systemctl restart fog.service &");
#endif
            if (isLxcRestart) {
                LOG("Restarting Lxc Service After Waking up from Deep Sleep\r\n");
                system("systemctl restart lxc.service");
                isLxcRestart = 0;
            } else {
#ifndef ENABLE_LLAMA_PLATCO_SKY_XIONE
                LOG("Restarting WPEFramework Service After Waking up from Deep Sleep\r\n");
                system("systemctl restart wpeframework.service");
                LOG("Restarting Receiver Service After Waking up from Deep Sleep\r\n");
                system("systemctl restart xre-receiver.service");
#else
                LOG("Skipping restart of Services in Sky Platform\n");
#endif

            }
        }
        IsDeviceInDeepSleep = DeepSleepStatus_NotStarted;

        LOG("Device woke up from Deep sleep Mode.. \r\n");
    }
    return IARM_RESULT_SUCCESS;
}

static IARM_Result_t _GetDeepSleepStatus(void *arg)
{
    int *status = (int *)arg;
    *status = IsDeviceInDeepSleep;
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t GetPwrMgrDeepSleepStatus(int *status)
{
    *status = IsDeviceInDeepSleep;
    return IARM_RESULT_SUCCESS;
}

static IARM_Result_t _SetDeepSleepTimer(void *arg)
{
    IARM_Bus_DeepSleepMgr_SetDeepSleepTimer_Param_t *param = (IARM_Bus_DeepSleepMgr_SetDeepSleepTimer_Param_t *)arg;

    if(param != NULL) {
        LOG("Deep sleep timer set to : %d Seconds \r\n", param->timeout);
        deep_sleep_delay_timeout = param->timeout;
        return IARM_RESULT_SUCCESS;
    }
    return IARM_RESULT_IPCCORE_FAIL;
}

static gboolean deep_sleep_delay_timer_fn(gpointer data)
{
    struct stat buf;
    int status = -1;

    LOG("Deep Sleep Timer Expires :Enter to Deep sleep Mode..stop Receiver with sleep 10 before DS \r\n");
    system("sleep 10");

    if ((stat("/lib/systemd/system/lxc.service", &buf) == 0) && (stat("/opt/lxc_service_disabled",&buf) !=0)) {
        system("systemctl stop lxc.service");
        isLxcRestart = 1;
    } else {
#ifndef ENABLE_LLAMA_PLATCO_SKY_XIONE
        system("systemctl stop xre-receiver.service");
        system("systemctl stop wpeframework.service");
#else
        LOG("Skiping Stopping of services in Sky Llama Platform\n");
#endif
    }
    bool userWakeup = 0;
    status = PLAT_DS_SetDeepSleep(deep_sleep_wakeup_timer,&userWakeup, nwStandbyMode_gs);
    if(status != 0) {
        LOG("deep_sleep_delay_timer_fn: Failed to enter deepsleep state \n");
    }

    #ifdef USE_WAKEUP_TIMER_EVT
    //Call pwrmgr InvokeDeepsleepTimeout here
    DeepSleep_WakeupReason_t wakeupReason = DEEPSLEEP_WAKEUPREASON_UNKNOWN;
    int reasonStatus = PLAT_DS_GetLastWakeupReason(&wakeupReason);
    if (DEEPSLEEP_WAKEUPREASON_TIMER == wakeupReason){
        LOG("Calling IARM_BUS_PWRMGR_API_handleDeepsleepTimeoutWakeup on wakeupReason:%d \n", wakeupReason);
        _handleDeepsleepTimeoutWakeup();
    }
    #endif
    return FALSE; // Send False so the handler should not be called again
}

static IARM_Result_t _GetLastWakeupReason(void *arg)
{
    DeepSleep_WakeupReason_t *wakeupReason = (DeepSleep_WakeupReason_t *)arg;
    int status = PLAT_DS_GetLastWakeupReason(wakeupReason);
    return (IARM_Result_t) status;
}

static IARM_Result_t _GetLastWakeupKeyCode(void *arg)
{
    IARM_Bus_DeepSleepMgr_WakeupKeyCode_Param_t *wakeupKeyCode = (IARM_Bus_DeepSleepMgr_WakeupKeyCode_Param_t *)arg;
    int status = PLAT_DS_GetLastWakeupKeyCode(wakeupKeyCode);
    return (IARM_Result_t) status;

}
#endif
/** @} */
/** @} */
