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
* @defgroup vrexmgr
* @{
**/


#if 0
#include "iarmUtil.h"
#include "iarmStatus.h"
#include "vrexLogInternal.h"
//#include "gpMgr.h"
#include "rf4ceMgr.h"
#include "libIBus.h"
#include "libIBusDaemon.h"
#include "comcastIrKeyCodes.h"
#include "pwrMgr.h"
#include "secure_wrapper.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/time.h>
#include <unistd.h>

/*----------------------------------------------------------------------------*/
/*                                   Macros                                   */
/*----------------------------------------------------------------------------*/
#ifdef STANDALONE_TEST
#define ALIVE_CHECK_PERIOD_SEC   2
#else
#define ALIVE_CHECK_PERIOD_SEC   60
#endif

#define ACCEPTABLE_FAILURES_BEFORE_HANG 3

#ifdef STANDALONE_TEST
#define APPLICATION_NAME "testStatus123\0"
#else
#define APPLICATION_NAME "AppMSOTarget\0"
#endif

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/*----------------------------------------------------------------------------*/
/*                               Data Structures                              */
/*----------------------------------------------------------------------------*/
typedef struct {
    bool initialized;
    pthread_t thread_id;
} iarm_status_thread_t;

typedef struct {
    char * string;
    uint32_t value;
} __iarm_key_map_t;

/*----------------------------------------------------------------------------*/
/*                            File Scoped Variables                           */
/*----------------------------------------------------------------------------*/
static __iarm_key_map_t key_status_to_string_map[] = {
    {"KEYDOWN", KET_KEYDOWN},
    {"KEYUP", KET_KEYUP},
    {"KEYREPEAT", KET_KEYREPEAT}
};

static __iarm_key_map_t key_to_string_map[] = {
    {"DIGIT0", KED_DIGIT0},
    {"DIGIT1", KED_DIGIT1},
    {"DIGIT2", KED_DIGIT2},
    {"DIGIT3", KED_DIGIT3},
    {"DIGIT4", KED_DIGIT4},
    {"DIGIT5", KED_DIGIT5},
    {"DIGIT6", KED_DIGIT6},
    {"DIGIT7", KED_DIGIT7},
    {"DIGIT8", KED_DIGIT8},
    {"DIGIT9", KED_DIGIT9},
    {"PERIOD", KED_PERIOD},

    {"DISCRETE_POWER_ON", KED_DISCRETE_POWER_ON},
    {"DISCRETE_POWER_STANDBY", KED_DISCRETE_POWER_STANDBY},

    {"SEARCH", KED_SEARCH},
    {"SETUP", KED_SETUP},

    {"RF_POWER", KED_RF_POWER},
    {"POWER", KED_POWER},
    {"FP_POWER", KED_FP_POWER},
    {"ARROWUP", KED_ARROWUP},
    {"ARROWDOWN", KED_ARROWDOWN},
    {"ARROWLEFT", KED_ARROWLEFT},
    {"ARROWRIGHT", KED_ARROWRIGHT},
    {"SELECT", KED_SELECT},
    {"ENTER", KED_ENTER},
    {"EXIT", KED_EXIT},
    {"CHANNELUP", KED_CHANNELUP},
    {"CHANNELDOWN", KED_CHANNELDOWN},
    {"VOLUMEUP", KED_VOLUMEUP},
    {"VOLUMEDOWN", KED_VOLUMEDOWN},
    {"MUTE", KED_MUTE},
    {"GUIDE", KED_GUIDE},
    {"VIEWINGGUIDE", KED_VIEWINGGUIDE},
    {"INFO", KED_INFO},
    {"SETTINGS", KED_SETTINGS},
    {"PAGEUP", KED_PAGEUP},
    {"PAGEDOWN", KED_PAGEDOWN},
    {"KEYA", KED_KEYA},
    {"KEYB", KED_KEYB},
    {"KEYC", KED_KEYC},
    {"KEYD", KED_KEYD},
    {"KEY_RED_CIRCLE", KED_KEY_RED_CIRCLE},
    {"KEY_GREEN_DIAMOND", KED_KEY_GREEN_DIAMOND},
    {"KEY_BLUE_SQUARE", KED_KEY_BLUE_SQUARE},
    {"KEY_YELLOW_TRIANGLE", KED_KEY_YELLOW_TRIANGLE},
    {"LAST", KED_LAST},
    {"FAVORITE", KED_FAVORITE},
    {"REWIND", KED_REWIND},
    {"FASTFORWARD", KED_FASTFORWARD},
    {"PLAY", KED_PLAY},
    {"STOP", KED_STOP},
    {"PAUSE", KED_PAUSE},
    {"RECORD", KED_RECORD},
    {"BYPASS", KED_BYPASS},
    {"TVVCR", KED_TVVCR},

    {"REPLAY", KED_REPLAY},
    {"HELP", KED_HELP},
    {"RECALL_FAVORITE_0", KED_RECALL_FAVORITE_0},
    {"CLEAR", KED_CLEAR},
    {"DELETE", KED_DELETE},
    {"START", KED_START},
    {"POUND", KED_POUND},
    {"FRONTPANEL1", KED_FRONTPANEL1},
    {"FRONTPANEL2", KED_FRONTPANEL2},
    {"OK", KED_OK},
    {"STAR", KED_STAR},

    {"TVPOWER", KED_TVPOWER},
    {"PREVIOUS", KED_PREVIOUS},
    {"NEXT", KED_NEXT},
    {"MENU", KED_MENU},
    {"INPUTKEY", KED_INPUTKEY},
    {"LIVE", KED_LIVE},
    {"MYDVR", KED_MYDVR},
    {"ONDEMAND", KED_ONDEMAND},
    {"STB_MENU", KED_STB_MENU},
    {"AUDIO", KED_AUDIO},
    {"FACTORY", KED_FACTORY},
    {"RFENABLE", KED_RFENABLE},
    {"LIST", KED_LIST},
    {"UNDEFINEDKEY", KED_UNDEFINEDKEY},


    {"BACK", KED_BACK},
    {"DISPLAY_SWAP", KED_DISPLAY_SWAP},
    {"PINP_MOVE", KED_PINP_MOVE},
    {"PINP_TOGGLE", KED_PINP_TOGGLE},
    {"PINP_CHDOWN", KED_PINP_CHDOWN},
    {"PINP_CHUP", KED_PINP_CHUP},
    {"DMC_ACTIVATE", KED_DMC_ACTIVATE},
    {"DMC_DEACTIVATE", KED_DMC_DEACTIVATE},
    {"DMC_QUERY", KED_DMC_QUERY},
    {"OTR_START", KED_OTR_START},
    {"OTR_STOP", KED_OTR_STOP}
};

static bool main_init = false;

static iarm_status_thread_t main;

static bool in_power_save = false;
static pthread_mutex_t power_save_mutex;
static pthread_mutex_t status_mutex;

/* Keep a running count of how many heart beats we have successfully processed */
static uint32_t __status_request_count;
static uint32_t __status_kill_count;
static unsigned char __battery_level_loaded[GP_MSO_BUS_API_MAX_BINDED_REMOTES-1];

/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
static void *status_main(void *arg);

static bool status_check_and_process(void);
static IARM_Result_t process_status_msg(gpMSOBusAPI_Packet_t *getRequest);
static void status_handle_hang(void);

static bool is_powered_on(void);
static IARM_Result_t _PowerPreChange(void *arg);

/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
IARM_Result_t IARMSTATUS_Start(void)
{
    IARM_Result_t rv = IARM_RESULT_INVALID_STATE;
    if (false == main_init) {
        pthread_mutex_init (&power_save_mutex, NULL);
        pthread_mutex_init (&status_mutex, NULL);
        if (0 == pthread_create(&main.thread_id, NULL, status_main, NULL) ) {
            main.initialized = true;


            IARM_Bus_RegisterCall(IARM_BUS_COMMON_API_PowerPreChange, _PowerPreChange);

            rv = IARM_RESULT_SUCCESS;

            main_init = true;
            in_power_save = !is_powered_on();
        }

        if (IARM_RESULT_SUCCESS != rv) {
            IARMSTATUS_Stop();
        }
    }

    return rv;
}

IARM_Result_t IARMSTATUS_Stop(void)
{
    IARM_Result_t rv= IARM_RESULT_SUCCESS;

    if (main.initialized) {
        pthread_kill(main.thread_id, SIGTERM);

        pthread_join(main.thread_id, NULL);

        pthread_mutex_destroy (&power_save_mutex);
        pthread_mutex_destroy (&status_mutex);

        main.initialized = false;
    }

    main_init = false;

    return rv;
}

void IARMSTATUS_keyEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
    int i_status;
    int i_key_value;

    gpMSOBusAPI_UserCommand_t *key = (gpMSOBusAPI_UserCommand_t*)data;

    for(i_status = ARRAY_SIZE(key_status_to_string_map) - 1; i_status >= 0; i_status--) {
        if (key_status_to_string_map[i_status].value == key->keyStatus) {
            break;
        }
    }

    for(i_key_value = ARRAY_SIZE(key_to_string_map) - 1; i_key_value >= 0; i_key_value--) {
        if (key_to_string_map[i_key_value].value == key->commandCode) {
            break;
        }
    }

    __TIMESTAMP();
    STATUS_LOG("remoteId <%u> received key %s:0x%08x -- %s:0x%08x\n",
               key->remoteId,
               (-1 == i_status?"UNKNOWN":key_status_to_string_map[i_status].string),
               key->keyStatus,
               (-1 == i_key_value?"UNKNOWN":key_to_string_map[i_key_value].string),
               key->commandCode);

}

/**
 * Return the status kill count
 */
uint32_t get_status_kill_count()
{
    uint32_t kill_count;

    pthread_mutex_lock(&status_mutex);
    kill_count = __status_kill_count;
    pthread_mutex_unlock(&status_mutex);

    return kill_count;
}

/**
 * Return the battery level loaded
 */
unsigned char get_battery_level_loaded(unsigned char remoteId)
{
    unsigned char battery_level;

    pthread_mutex_lock(&status_mutex);
    battery_level = __battery_level_loaded[remoteId];
    pthread_mutex_unlock(&status_mutex);

    return battery_level;
}

/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
static void *status_main(void *arg)
{
    bool check;
    __TIMESTAMP();
    STATUS_LOG("-- Start Status --\n");
  

    for (;;) {
        __TIMESTAMP();
        STATUS_LOG(" status heartbeat\n");

        pthread_mutex_lock(&power_save_mutex);
        if (false == in_power_save) {
            check = status_check_and_process();

            if (false == check) {
                int failed_count;

                for (failed_count = 0; failed_count < ACCEPTABLE_FAILURES_BEFORE_HANG; failed_count++) {
                    check = status_check_and_process();

                    if (true == check) {
                        break;
                    }
                }

                if (false == check) {
                    /* We have detected a hang */
                    status_handle_hang();

                    __status_request_count = 0;
                }
            }
        }
        pthread_mutex_unlock(&power_save_mutex);

        sleep(ALIVE_CHECK_PERIOD_SEC);
    }

    return NULL;
}

/**
 * Populate the passed in getRequest with the status information from rf4ce module
 */
static bool status_check_and_process(void)
{
    bool rv = false;
    gpMSOBusAPI_Packet_t getRequest;
    IARM_Result_t result;
    bzero(&getRequest, sizeof(gpMSOBusAPI_Packet_t));
    getRequest.msgId  = gpMSOBusAPI_MsgId_GetRfStatus;
    getRequest.length = sizeof(gpMSOBusAPI_RfStatus_t);
    getRequest.index  = 0;

    __TIMESTAMP();
    STATUS_LOG(" Requesting status\n");

    result = IARM_Bus_Call(IARM_BUS_GPMGR_NAME, IARM_BUS_GPMGR_API_MsgRequest, (void *)&getRequest, sizeof(gpMSOBusAPI_Packet_t));

    if (IARM_RESULT_SUCCESS == result) {
        result = process_status_msg(&getRequest);
    }

    if (IARM_RESULT_SUCCESS == result) {
        rv = true;
    }

    return rv;
}

/**
 * Process the status message and push the information to the log
 */
static IARM_Result_t process_status_msg(gpMSOBusAPI_Packet_t *getRequest)
{
    IARM_Result_t rv = IARM_RESULT_INVALID_STATE;

    __TIMESTAMP();
    if (    (gpMSOBusAPI_MsgId_GetRfStatus == getRequest->msgId)
         && (1))

    {
        int i = 0;
        gpMSOBusAPI_RfStatus_t *rf = &getRequest->msg.RfStatus;

        __status_request_count++;

        rv = IARM_RESULT_SUCCESS;

        if (1 == __status_request_count) {
            STATUS_LOG("RfStatus:  \n");
            STATUS_LOG(" versionInfo: version: major (%u)\n", rf->versionInfo.version.major);
            STATUS_LOG("                       minor (%u)\n", rf->versionInfo.version.minor);
            STATUS_LOG("                       revision (%u)\n", rf->versionInfo.version.revision);
            STATUS_LOG("                       patch (%u)\n", rf->versionInfo.version.patch);
            STATUS_LOG("            : ctrl (%u)\n", rf->versionInfo.ctrl);
            STATUS_LOG("            : number (%u)\n", rf->versionInfo.number);
            STATUS_LOG("            : changeList (%u)\n", rf->versionInfo.changeList);
            STATUS_LOG(" macAddress %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n", rf->macAddress[0],
                                                            rf->macAddress[1],
                                                            rf->macAddress[2],
                                                            rf->macAddress[3],
                                                            rf->macAddress[4],
                                                            rf->macAddress[5],
                                                            rf->macAddress[6],
                                                            rf->macAddress[7]);
            STATUS_LOG(" shortAddress (%u)\n", rf->shortAddress);
            STATUS_LOG(" panId (%u)\n", rf->panId);
            STATUS_LOG(" activeChannel: number (%u)\n", rf->activeChannel.Number);
            STATUS_LOG("              : pollution (%u)\n", rf->activeChannel.Pollution);
            for (i = 0; i < 2; i++) {
                gpMSOBusAPI_RfChannel_t *backupChannel = &rf->backupChannels[i];
                STATUS_LOG(" backupChannel[%u]: number (%u)\n", i, backupChannel->Number);
                STATUS_LOG("                 : pollution (%u)\n", backupChannel->Pollution);
            }
            STATUS_LOG(" lowUptime (%u)\n", rf->lowUptime);
            STATUS_LOG(" nbrBindRemotes (%u)\n", rf->nbrBindRemotes);
            for (i = 0; i < GP_MSO_BUS_API_MAX_BINDED_REMOTES-1; i++) {
                gpMSOBusAPI_BindRemote_t *boundRemote = &rf->bindRemotes[i];

                if (0xFFFF != boundRemote->ShortAddress) {
                    STATUS_LOG(" bindRemotes[%u]: macAddress %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
                               i,
                               boundRemote->MacAddress[0],
                               boundRemote->MacAddress[1],
                               boundRemote->MacAddress[2],
                               boundRemote->MacAddress[3],
                               boundRemote->MacAddress[4],
                               boundRemote->MacAddress[5],
                               boundRemote->MacAddress[6],
                               boundRemote->MacAddress[7]);
                    STATUS_LOG("               : shortAddress (%u)\n",
                               boundRemote->ShortAddress);
                    STATUS_LOG("               : commandCount (%u)\n",
                               boundRemote->CommandCount);
                    STATUS_LOG("               : lastCommand (%u)\n",
                               boundRemote->LastCommand);
                    STATUS_LOG("               : timestampLastCommand (%u)\n",
                               boundRemote->TimestampLastCommand);
                    STATUS_LOG("               : signalStrength (%u)\n",
                               boundRemote->SignalStrength);
                    STATUS_LOG("               : linkQuality (%u)\n",
                               boundRemote->LinkQuality);
                    STATUS_LOG("               : versionInfoSw (%X:%X:%X:%X)\n",
                               boundRemote->VersionInfoSw[0],
                               boundRemote->VersionInfoSw[1],
                               boundRemote->VersionInfoSw[2],
                               boundRemote->VersionInfoSw[3]);
                    STATUS_LOG("               : versionInfoHw: manufacturer (%u)\n",
                               boundRemote->VersionInfoHw.manufacturer);
                    STATUS_LOG("               :              : model (%u)\n",
                               boundRemote->VersionInfoHw.model);
                    STATUS_LOG("               :              : hwRevision (%u)\n",
                               boundRemote->VersionInfoHw.hwRevision);
                    STATUS_LOG("               :              : lotCode (%u)\n",
                               boundRemote->VersionInfoHw.lotCode);
                    STATUS_LOG("               : versionInfoIrdb (%X:%X:%X:%X)\n",
                               boundRemote->VersionInfoIrdb[0],
                               boundRemote->VersionInfoIrdb[1],
                               boundRemote->VersionInfoIrdb[2],
                               boundRemote->VersionInfoIrdb[3]);
                    STATUS_LOG("               : batteryLevelLoaded (%u)\n",
                               boundRemote->BatteryLevelLoaded);
                    STATUS_LOG("               : betterLevelUnloaded (%u)\n",
                               boundRemote->BatteryLevelUnloaded);
                    STATUS_LOG("               : timestampeBatteryUpdate (%u)\n",
                               boundRemote->TimestampBatteryUpdate);
                    STATUS_LOG("               : type[%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X]\n",
                               boundRemote->Type[0],
                               boundRemote->Type[1],
                               boundRemote->Type[2],
                               boundRemote->Type[3],
                               boundRemote->Type[4],
                               boundRemote->Type[5],
                               boundRemote->Type[6],
                               boundRemote->Type[7],
                               boundRemote->Type[8]);
                    STATUS_LOG("               : controllerProperties (%u)\n",
                               boundRemote->ControllerProperties);
                    STATUS_LOG("               : remoteId (%u)\n",
                               boundRemote->remoteId);

                }
            }
        }

        // Save the battery level for each remote
        pthread_mutex_lock(&status_mutex);
        for (i = 0; i < GP_MSO_BUS_API_MAX_BINDED_REMOTES-1; i++) {
            gpMSOBusAPI_BindRemote_t *boundRemote = &rf->bindRemotes[i];

            if (0xFFFF != boundRemote->ShortAddress) {
                __battery_level_loaded[boundRemote->remoteId] = boundRemote->BatteryLevelLoaded;
            }
        }
        pthread_mutex_unlock(&status_mutex);

        STATUS_LOG("Status HeartBeat:\n");

        for (i = 0; i < GP_MSO_BUS_API_MAX_BINDED_REMOTES-1; i++) {
            gpMSOBusAPI_BindRemote_t *remote = &rf->bindRemotes[i];
            if (0 == i) {
                STATUS_LOG("Active Channel(%u) LowUptime(%u) Successful Status Requests(%u) %.13s Kills (%u)\n",
                        rf->activeChannel, rf->lowUptime,
                        __status_request_count, APPLICATION_NAME, __status_kill_count);
            }

            if (0xFFFF != remote->ShortAddress) {
                STATUS_LOG(" RemoteID(%u) Model(%.9s) Controller Props(0x%08X) LQI(%u)\n",
                           remote->remoteId, remote->Type, remote->ControllerProperties, remote->LinkQuality);
            }
        }

    } else {
        STATUS_LOG("received unknown message response (%u) instead of status (%u)\n", getRequest->msgId, gpMSOBusAPI_MsgId_GetRfStatus);
    }

    return rv;
}

static void status_increment_kill_count(void)
{
    pthread_mutex_lock(&status_mutex);
    __status_kill_count++;
    pthread_mutex_unlock(&status_mutex);
}

/**
 * Log and kill the current process
 */
static void status_handle_hang(void)
{
    char command[100];

    __TIMESTAMP();
    STATUS_LOG("HUNG iarmmgr detected.  Killing NOW\n");

    snprintf(command, sizeof(command), "killall %s", APPLICATION_NAME);
    command[sizeof(command)-1] = '\0';
    v_secure_system(command);
    status_increment_kill_count();
}

static bool is_powered_on(void)
{
    IARM_Bus_PWRMgr_GetPowerState_Param_t param;
    param.curState = IARM_BUS_PWRMGR_POWERSTATE_ON;
    
    return (bool) ((IARM_Bus_Call(IARM_BUS_PWRMGR_NAME, "GetPowerState", (void *)&param, sizeof(param)) == IARM_RESULT_SUCCESS) && (param.curState == IARM_BUS_PWRMGR_POWERSTATE_ON));
}

/**
 * Set the state of the logger based on the power state events
 */
static IARM_Result_t _PowerPreChange(void *arg)
{
    IARM_Bus_CommonAPI_PowerPreChange_Param_t *param = (IARM_Bus_CommonAPI_PowerPreChange_Param_t *) arg;
    IARM_Result_t result = IARM_RESULT_SUCCESS;

    __TIMESTAMP();
    STATUS_LOG("PowerPreChange state to %d (STANDBY %d ::ON %d)\n",
               param->newState,
               IARM_BUS_PWRMGR_POWERSTATE_STANDBY,
               IARM_BUS_PWRMGR_POWERSTATE_ON);

    pthread_mutex_lock(&power_save_mutex);
    in_power_save = (param->newState != IARM_BUS_PWRMGR_POWERSTATE_ON);
    pthread_mutex_unlock(&power_save_mutex);

    return result;
}
#endif


/** @} */
/** @} */
