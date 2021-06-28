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
* @defgroup power
* @{
**/


#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C"
{
#endif
#ifdef __cplusplus
}
#endif

#ifdef OFFLINE_MAINT_REBOOT
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <net/if.h>
#endif

#include <sys/stat.h>
#include "iarmUtil.h"
#include "irMgr.h"
#include "sysMgr.h"
#include "comcastIrKeyCodes.h"
#include "libIBus.h"
#include "plat_power.h"
#include "pwrMgrInternal.h"
#include <pthread.h>
#include "libIBusDaemon.h"
#include "manager.hpp"
#include "host.hpp"
#include "list.hpp"
#include "sleepMode.hpp"
#include "pwrlogger.h"
#include "frontPanelIndicator.hpp"
#include "resetModes.h"
#include "rfcapi.h"
#include "iarmcec.h"
#include "deepSleepMgr.h"
#include "productTraits.h"

/* For glib APIs*/
#include <glib.h>

#ifndef NO_RF4CE
static void _speechEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
#ifdef USE_UNIFIED_CONTROL_MGR_API_1
#include "ctrlm_ipc.h"
#include "ctrlm_ipc_rcu.h"
static void _ctrlmMessageHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
#else
#ifdef USE_UNIFIED_RF4CE_MGR_API_4
#include "vrexMgrApi.h"
#else
#include "vrexMgr.h"
#endif /* USE_UNIFIED_RF4CE_MGR_API_4 */
#ifdef RF4CE_GENMSO_API
#include "rf4ceMgr.h"
static void _rfMessageHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
#elif defined(RF4CE_API)
#include "rf4ceMgr.h"
static void _rf4ceMessageHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
#elif defined(RF4CE_GPMSO_API)
#include "rf4ceMgr.h"
static void _gpMessageHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
#else
#warning "No RF4CE API defined"
#endif
#endif  /* USE_UNIFIED_CONTROL_MGR_API_1 */
#endif  /* NO_RF4CE */

#define PADDING_SIZE 32
#define _UIMGR_SETTINGS_MAGIC 0xFEBEEFAC
/*LED settings*/
typedef struct _PWRMgr_LED_Settings_t{
    unsigned int brightness;
    unsigned int color;
}PWRMgr_LED_Settings_t;

typedef struct _PWRMgr_Settings_t{
    uint32_t magic;
    uint32_t version;
    uint32_t length;
    volatile IARM_Bus_PWRMgr_PowerState_t powerState;
    PWRMgr_LED_Settings_t ledSettings;
    #ifdef ENABLE_DEEP_SLEEP
        uint32_t deep_sleep_timeout;    
    #endif
    #ifdef ENABLE_LLAMA_PLATCO_SKY_XIONE
        bool nwStandbyMode;
    #endif    
    char padding[PADDING_SIZE];
} PWRMgr_Settings_t;

typedef enum _UIDev_PowerState_t {
    UIDEV_POWERSTATE_OFF,
    UIDEV_POWERSTATE_STANDBY,
    UIDEV_POWERSTATE_ON,
    UIDEV_POWERSTATE_UNKNOWN
} UIDev_PowerState_t;

typedef struct _UIMgr_Settings_t{
    uint32_t magic;
    uint32_t version;
    uint32_t length;
    UIDev_PowerState_t powerState;
    char padding[PADDING_SIZE];
} UIMgr_Settings_t ;

static PWRMgr_Settings_t m_settings = {0};
static const char *m_settingsFile = NULL;
static bool isCecLocalLogicEnabled = false;

#define MAX_NUM_VIDEO_PORTS 5
typedef struct{
    char port[PWRMGR_MAX_VIDEO_PORT_NAME_LENGTH];
    bool isEnabled;
}PWRMgr_Standby_Video_State_t;
static PWRMgr_Standby_Video_State_t g_standby_video_port_setting[MAX_NUM_VIDEO_PORTS];
static pwrMgrProductTraits::ux_controller * ux = nullptr;

#ifdef _ENABLE_FP_KEY_SENSITIVITY_IMPROVEMENT
#ifndef POWER_KEY_SENSITIVITY
#define POWER_KEY_SENSITIVITY 500 
#endif
static struct timeval powerKeyIntervals[] = { 
    /*  
     * [0] is timestamp of DOWN
     * [1] is timestamp of UP
     */
    {0, 0}, {0, 0}
};
#define PWRMGR_SET_POWER_KEYDOWN_TM() do\
{\
    if (POWER_KEY_SENSITIVITY) {\
        struct timeval __tv;\
        gettimeofday(&__tv, NULL);\
        printf("[POWERKEY-DOWN][%ld\[%d]\r\n", __tv.tv_sec, __tv.tv_usec);\
        fflush(stdout);\
        powerKeyIntervals[0] = (__tv);\
        powerKeyIntervals[1].tv_sec = 0;\
        powerKeyIntervals[1].tv_usec = 0;\
    }\
} while(0)

#define PWRMGR_SET_POWER_KEYUP_TM() do\
{\
    if (POWER_KEY_SENSITIVITY && powerKeyIntervals[0].tv_sec != 0) {\
        struct timeval __tv;\
        gettimeofday(&__tv, NULL);\
        printf("[POWERKEY-UP  ][%ld\[%d]\r\n", __tv.tv_sec, __tv.tv_usec);\
        fflush(stdout);\
        powerKeyIntervals[1] = (__tv);\
        }\
    else {\
    }\
} while(0)\

#define TIMEVAL_DIFF(t2,t1) ((int)(((((int)(t2.tv_sec - t1.tv_sec)) - 1) * 1000) + (((1000*1000 + t2.tv_usec) - t1.tv_usec)/1000)))
#define PWRMGR_GET_POWER_KEY_INTERVAL() \
                   (((powerKeyIntervals[1].tv_sec) && (powerKeyIntervals[1].tv_sec >= powerKeyIntervals[0].tv_sec)) ?\
                   (TIMEVAL_DIFF(powerKeyIntervals[1], powerKeyIntervals[0])) :\
                   (POWER_KEY_SENSITIVITY))

static volatile IARM_Bus_PWRMgr_PowerState_t transitionState;
static volatile IARM_Bus_PWRMgr_PowerState_t targetState;

static pthread_cond_t   powerStateCond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t  powerStateMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t asyncPowerThreadId = NULL;

static void _SetPowerStateAsync(IARM_Bus_PWRMgr_PowerState_t curState, IARM_Bus_PWRMgr_PowerState_t newState);
#endif


static void _controlEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
static void _irEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
static void _systemStateChangeHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
static IARM_Result_t _SetPowerState(void *arg);
static IARM_Result_t _GetPowerState(void *arg);
static IARM_Result_t _WareHouseReset(void *arg);
static IARM_Result_t _WareHouseClear(void *arg);
static IARM_Result_t _ColdFactoryReset(void *);
static IARM_Result_t _FactoryReset(void *);
static IARM_Result_t _UserFactoryReset(void *);
static IARM_Result_t _GetPowerStateBeforeReboot(void *arg);
static IARM_Result_t _HandleReboot(void *arg);

static int _InitSettings(const char *settingsFile);
static int _WriteSettings(const char *settingsFile);
static void _DumpSettings(const PWRMgr_Settings_t *pSettings);

static int _SetLEDStatus(IARM_Bus_PWRMgr_PowerState_t state);

int _SetAVPortsPowerState(IARM_Bus_PWRMgr_PowerState_t powerState);
static IARM_Bus_PWRMgr_PowerState_t _ConvertUIDevToIARMBusPowerState(UIDev_PowerState_t powerState);
static IARM_Result_t _SetStandbyVideoState(void *arg);
static IARM_Result_t _GetStandbyVideoState(void *arg);
static int ecm_connectivity_lost = 0;
time_t xre_timer; // Hack to fix DELIA-11393
static bool deepSleepWakeup = false;
extern void IARM_Bus_PWRMGR_RegisterSleepTimerAPIs(void *);

/*EAS handling */
static IARM_Result_t _SysModeChange(void *arg);
static IARM_Bus_Daemon_SysMode_t isEASInProgress = IARM_BUS_SYS_MODE_NORMAL;

/*pwrMgr Glib variables */
GMainLoop *pwrMgr_Gloop = NULL;
static gboolean heartbeatMsg(gpointer data);
std::string powerStateBeforeReboot_gc;
static IARM_Bus_PWRMgr_PowerState_t g_last_known_power_state = IARM_BUS_PWRMGR_POWERSTATE_ON;
#ifdef ENABLE_DEEP_SLEEP  

    /* PwrMgr Static Functions for Deep SLeep feature */

    /* IARM RPC Handler to Set Deep Sleep Wakeup Timer */
    static IARM_Result_t _SetDeepSleepTimeOut(void *arg);
    
    /* Gloop Wakeup Timer Handler */
    static gboolean deep_sleep_wakeup_fn(gpointer data);

    /* Calculate Wakeup Time */
    static uint32_t getWakeupTime();

    /* Gloop Handler to invoke deep sleep , if the box boots in deep sleep and timer expires.*/
    static gboolean invoke_deep_sleep_on_bootup_timeout(gpointer data);

    /* Variables for Deep Sleep */
    static uint32_t deep_sleep_wakeup_timeout_sec = 28800; //8*60*60 - 8 hours
    static uint8_t IsWakeupTimerSet = 0;
    static guint wakeup_event_src = 0;
    static guint dsleep_bootup_event_src = 0;
    static time_t timeAtDeepSleep = 0;
#endif
#ifdef ENABLE_LLAMA_PLATCO_SKY_XIONE
    static bool nwStandbyMode_gs = false;
#endif

#ifdef OFFLINE_MAINT_REBOOT
gint64 standby_time = 0;
static bool rfcUpdated = false;
static int standby_reboot_threshold = 86400;
static int force_reboot_threshold = 172800;

#define MAX_RFC_LEN 15

#define UPTIME_ABOVE_STANDBY_REBOOT(time) (time >= standby_reboot_threshold)
#define UPTIME_ABOVE_FORCE_REBOOT(time)   (time >= force_reboot_threshold)
#define REBOOT_GRACE_INTERVAL(u, s)       ((u - s) >= 900)

#define STANDBY_REBOOT_ENABLE "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.StandbyReboot.Enable"
#define STANDBY_REBOOT        "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.StandbyReboot.StandbyAutoReboot"
#define FORCE_REBOOT          "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.StandbyReboot.ForceAutoReboot"
#endif

static IARM_Result_t _SetNetworkStandbyMode(void *arg);
static IARM_Result_t _GetNetworkStandbyMode(void *arg);

static void setPowerStateBeforeReboot (IARM_Bus_PWRMgr_PowerState_t powerState) {
    switch (powerState) {
        case IARM_BUS_PWRMGR_POWERSTATE_OFF:
            powerStateBeforeReboot_gc = std::string ("OFF");
            break;
        case IARM_BUS_PWRMGR_POWERSTATE_STANDBY:
            powerStateBeforeReboot_gc = std::string ("STANDBY");
            break;
        case IARM_BUS_PWRMGR_POWERSTATE_ON:
            powerStateBeforeReboot_gc = std::string ("ON");
            break;
        case IARM_BUS_PWRMGR_POWERSTATE_STANDBY_LIGHT_SLEEP:
            powerStateBeforeReboot_gc = std::string ("LIGHT_SLEEP");
            break;
        case IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP:
            powerStateBeforeReboot_gc = std::string ("DEEP_SLEEP");
            break;
        default :
            powerStateBeforeReboot_gc = std::string ("UNKNOWN");
            break;
    }
    printf ("[%s]:[%d] powerStateBeforeReboot: %s \n", __FUNCTION__, __LINE__, powerStateBeforeReboot_gc.c_str());
}

static bool get_video_port_standby_setting(const char * port)
{
    if(NULL == port)
    {
        __TIMESTAMP();LOG("Error! Port name is NULL!\n");
        return false;
    }
    for(int i = 0; i < MAX_NUM_VIDEO_PORTS; i++)
    {
        if(0 == strncasecmp(port, g_standby_video_port_setting[i].port, PWRMGR_MAX_VIDEO_PORT_NAME_LENGTH))
        {
            return g_standby_video_port_setting[i].isEnabled;
        }
    }
    return false; //Default setting: video port is disabled in standby mode
}

IARM_Result_t PWRMgr_Start(int argc, char *argv[])
{
    char *settingsFile = NULL;
    time(&xre_timer); // Hack to fix DELIA-11393

    if (argc == 2) settingsFile = argv[1];

	setvbuf(stdout, NULL, _IOLBF, 0);
    LOG("Entering [%s] - [%s] - disabling io redirect buf\r\n", __FUNCTION__,IARM_BUS_PWRMGR_NAME);

#ifdef POWERMGR_PRODUCT_PROFILE_ID
    if(true == pwrMgrProductTraits::ux_controller::initialize_ux_controller(POWERMGR_PRODUCT_PROFILE_ID))
    {
        ux = pwrMgrProductTraits::ux_controller::get_instance();
    }
#endif
    if(nullptr == ux)
    {
        LOG("pwrmgr product traits not supported.\n");
    }

    PLAT_INIT();
            
    IARM_Bus_Init(IARM_BUS_PWRMGR_NAME);
    IARM_Bus_Connect();

   /* LOG("Initing PwrMgr Settings START\r\n");*/
   try {
        device::Manager::Initialize();
    }
    catch (...){
        LOG("Exception Caught during [device::Manager::Initialize]\r\n");
    }

    _InitSettings(settingsFile);	   
#ifdef _ENABLE_FP_KEY_SENSITIVITY_IMPROVEMENT
    transitionState = m_settings.powerState;
    targetState = m_settings.powerState;
#endif 
    IARM_Bus_RegisterEvent(IARM_BUS_PWRMGR_EVENT_MAX);
    IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_Reboot, _HandleReboot);
    IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_SetPowerState, _SetPowerState);
    IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_GetPowerState, _GetPowerState);
    IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_WareHouseReset, _WareHouseReset);
    IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_WareHouseClear, _WareHouseClear);

    IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_ColdFactoryReset, _ColdFactoryReset);
    IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_FactoryReset, _FactoryReset);
    IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_UserFactoryReset, _UserFactoryReset);
    IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_SetStandbyVideoState, _SetStandbyVideoState);
    IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_GetStandbyVideoState, _GetStandbyVideoState);
    #ifdef ENABLE_DEEP_SLEEP  
        IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_SetDeepSleepTimeOut, _SetDeepSleepTimeOut);
    #endif  
    IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_SetNetworkStandbyMode, _SetNetworkStandbyMode);
    IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_GetNetworkStandbyMode, _GetNetworkStandbyMode);
 
    IARM_Bus_RegisterEventHandler(IARM_BUS_IRMGR_NAME, IARM_BUS_IRMGR_EVENT_IRKEY, _irEventHandler);
    IARM_Bus_RegisterEventHandler(IARM_BUS_IRMGR_NAME, IARM_BUS_IRMGR_EVENT_CONTROL, _controlEventHandler);

    IARM_Bus_RegisterEventHandler(IARM_BUS_SYSMGR_NAME, IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE, _systemStateChangeHandler);

#ifndef NO_RF4CE
#ifdef USE_UNIFIED_CONTROL_MGR_API_1
    IARM_Bus_RegisterEventHandler(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_SESSION_BEGIN, _speechEventHandler);
    IARM_Bus_RegisterEventHandler(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_SESSION_ABORT, _speechEventHandler);
    IARM_Bus_RegisterEventHandler(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_SESSION_SHORT, _speechEventHandler);
    IARM_Bus_RegisterEventHandler(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_RCU_IARM_EVENT_KEY_GHOST, _ctrlmMessageHandler);
#else
#ifdef USE_UNIFIED_RF4CE_MGR_API_4
    IARM_Bus_RegisterEventHandler(VREX_MGR_IARM_BUS_NAME, VREX_MGR_IARM_EVENT_VOICE_BEGIN, _speechEventHandler);
    IARM_Bus_RegisterEventHandler(VREX_MGR_IARM_BUS_NAME, VREX_MGR_IARM_EVENT_VOICE_SESSION_ABORT, _speechEventHandler);
    IARM_Bus_RegisterEventHandler(VREX_MGR_IARM_BUS_NAME, VREX_MGR_IARM_EVENT_VOICE_SESSION_SHORT, _speechEventHandler);
#else
    IARM_Bus_RegisterEventHandler(IARM_BUS_VREXMGR_NAME, IARM_BUS_VREXMGR_EVENT_SPEECH, _speechEventHandler);
#endif /* USE_UNIFIED_RF4CE_MGR_API_4 */
#ifdef RF4CE_GENMSO_API
    IARM_Bus_RegisterEventHandler(IARM_BUS_RFMGR_NAME,IARM_BUS_RFMGR_EVENT_MSG_IND,_rfMessageHandler);
#elif defined(RF4CE_API)
    IARM_Bus_RegisterEventHandler(IARM_BUS_RF4CEMGR_NAME,IARM_BUS_RF4CEMGR_EVENT_MSG_IND,_rf4ceMessageHandler);
#elif defined(RF4CE_GPMSO_API)
    IARM_Bus_RegisterEventHandler(IARM_BUS_GPMGR_NAME,IARM_BUS_GPMGR_EVENT_MSG_IND,_gpMessageHandler);
#else
#warning "No RF4CE API defined"
#endif
#endif  /* NO_RF4CE */
#endif  /* USE_UNIFIED_CONTROL_MGR_API_1 */
		
	initReset();

    /*Register EAS handler so that we can ensure audio settings for EAS */
    IARM_Bus_RegisterCall(IARM_BUS_COMMON_API_SysModeChange,_SysModeChange);
    IARM_Bus_RegisterCall(IARM_BUS_PWRMGR_API_GetPowerStateBeforeReboot, _GetPowerStateBeforeReboot);

#ifdef ENABLE_THERMAL_PROTECTION
    initializeThermalProtection();
#endif //ENABLE_THERMAL_PROTECTION

    /* Create Main loop for Power Manager */
    pwrMgr_Gloop = g_main_loop_new ( NULL , FALSE );
    if(pwrMgr_Gloop != NULL){
        g_timeout_add_seconds (300 , heartbeatMsg , pwrMgr_Gloop); 
    }
    else {
        LOG("Fails to Create a main Loop for [%s] \r\n",IARM_BUS_PWRMGR_NAME);
    }

    IARM_Bus_PWRMGR_RegisterSleepTimerAPIs(pwrMgr_Gloop);
    RFC_ParamData_t rfcParam;
    WDMP_STATUS status = getRFCParameter("TestComponent", "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.CECLocalLogic.Enable", &rfcParam);
    if(strncmp(rfcParam.value, "true",4) == 0)
    {
        isCecLocalLogicEnabled= true;
        LOG("pwrMgr:RFC CEC Local Logic feature enabled \r\n");
    }
    char *rdk_deep_sleep_wakeup = getenv("RDK_DEEPSLEEP_WAKEUP_ON_POWER_BUTTON");
    deepSleepWakeup = (rdk_deep_sleep_wakeup && atoi(rdk_deep_sleep_wakeup));

    return IARM_RESULT_SUCCESS;
}

IARM_Result_t PWRMgr_Loop()
{
   
   /* Power Mgr loop */
    if(pwrMgr_Gloop)
    { 
        g_main_loop_run (pwrMgr_Gloop);
        g_main_loop_unref(pwrMgr_Gloop);
    }
	return IARM_RESULT_SUCCESS;
}

#ifdef OFFLINE_MAINT_REBOOT
bool isStandbyRebootEnabled()
{
	RFC_ParamData_t rfcParam;
	char* key = STANDBY_REBOOT_ENABLE;
	if (WDMP_SUCCESS == getRFCParameter("PwrMgr", key, &rfcParam))
	{
		return (strncasecmp(rfcParam.value, "true", 4) == 0);
	}

	return false;
}

int getStandbyRebootValue(char* key)
{
	RFC_ParamData_t param;
	char rfcVal[MAX_RFC_LEN+1] = {0};
	int len = 0;

	if (WDMP_SUCCESS == getRFCParameter("PwrMgr", key, &param))
	{
		len = strlen(param.value);
		if (len > MAX_RFC_LEN)
		{
			len = MAX_RFC_LEN;
		}

		if ( (param.value[0] == '"') && (param.value[len] == '"'))
		{
			strncpy (rfcVal, &param.value[1], len - 1);
			rfcVal[len] = '\0';
		}
		else
		{
			strncpy (rfcVal, param.value, MAX_RFC_LEN-1);
			rfcVal[len] = '\0';
		}
		return atoi(rfcVal);
	}

	return -1;
}

bool isOfflineMode()
{
#if 0
	struct stat buf;
	return ((stat("/tmp/addressaquired_ipv4", &buf) != 0)
		&& (stat("/tmp/addressaquired_ipv6", &buf) != 0));
#endif

	struct ifaddrs *ifAddr, *ifAddrIt;
	bool offline = true;

	getifaddrs(&ifAddr);
	for (ifAddrIt = ifAddr; ifAddrIt != NULL; ifAddrIt = ifAddrIt->ifa_next)
	{
		if (NULL != ifAddrIt->ifa_addr
			&& (!strcmp(ifAddrIt->ifa_name, "eth0:0") || !strcmp(ifAddrIt->ifa_name, "wlan0:0")))
		{
			LOG("ifa_name=%s sa_family=%s ifa_flags=0x%X\n",
				ifAddrIt->ifa_name,
				ifAddrIt->ifa_addr->sa_family == AF_INET? "AF_INET" : ifAddrIt->ifa_addr->sa_family == AF_INET6? "AF_INET6" : "None",
			        (ifAddrIt->ifa_flags & IFF_RUNNING) );
		}

		if (NULL != ifAddrIt->ifa_addr
			&& (ifAddrIt->ifa_addr->sa_family == AF_INET || ifAddrIt->ifa_addr->sa_family == AF_INET6)
			&& (!strcmp(ifAddrIt->ifa_name, "eth0:0") || !strcmp(ifAddrIt->ifa_name, "wlan0:0"))
			&& (ifAddrIt->ifa_flags & IFF_RUNNING))
		{
			offline = false;
		}
	}
	freeifaddrs(ifAddr);

	return offline;
}

bool isInStandby()
{
	PWRMgr_Settings_t *pSettings = &m_settings;
	IARM_Bus_PWRMgr_PowerState_t curState = pSettings->powerState;
	LOG("%s PowerState = %d\n", __func__, curState);
	return  (curState == IARM_BUS_PWRMGR_POWERSTATE_STANDBY
		|| curState == IARM_BUS_PWRMGR_POWERSTATE_STANDBY_LIGHT_SLEEP);
}

#endif


static gboolean heartbeatMsg(gpointer data)
{
    time_t curr = 0;
    time(&curr);
    LOG("I-ARM POWER Mgr: HeartBeat at %s\r\n", ctime(&curr));

#ifdef OFFLINE_MAINT_REBOOT
    if (!rfcUpdated)
    {
        LOG("StandbyReboot.Enable = %s\n", isStandbyRebootEnabled() ? "true" : "false");

        standby_reboot_threshold = getStandbyRebootValue(STANDBY_REBOOT);
        if (standby_reboot_threshold == -1)
        {
            standby_reboot_threshold = 86400;
        }
        LOG("StandbyReboot.StandbyAutoReboot = %d\n", standby_reboot_threshold);

        force_reboot_threshold = getStandbyRebootValue(FORCE_REBOOT);
        if (force_reboot_threshold == -1)
        {
            force_reboot_threshold = 172800;
        }
        LOG("StandbyReboot.ForceAutoReboot = %d\n", force_reboot_threshold);
        rfcUpdated = true;
    }

    if (isStandbyRebootEnabled())
    {
        gint64 uptime = g_get_monotonic_time()/G_USEC_PER_SEC;
        if (UPTIME_ABOVE_STANDBY_REBOOT(uptime))
        {
            if (REBOOT_GRACE_INTERVAL(uptime, standby_time) && isInStandby())
            {
                LOG("Going to reboot after %lld\n", uptime);
                sleep(10);
                system("sh /rebootNow.sh -s PwrMgr -o 'Standby Maintenance reboot'");
            }

            if (UPTIME_ABOVE_FORCE_REBOOT(uptime))
            {
                LOG("Going to reboot after %lld\n", uptime);
                sleep(10);
                system("sh /rebootNow.sh -s PwrMgr -o 'Forced Maintenance reboot'");
            }
        }
    }
#endif

    return TRUE;
}


IARM_Result_t PWRMgr_Stop(void)
{
   
    if(pwrMgr_Gloop)
    { 
        g_main_loop_quit(pwrMgr_Gloop);
    }
  
    try {
        device::Manager::DeInitialize();
    }
    catch (...){
        LOG("Exception Caught during [device::Manager::Initialize]\r\n");
    }

    IARM_Bus_RemoveEventHandler(IARM_BUS_IRMGR_NAME, IARM_BUS_IRMGR_EVENT_IRKEY, _irEventHandler);
#ifndef NO_RF4CE
  #ifdef USE_UNIFIED_CONTROL_MGR_API_1
    IARM_Bus_RemoveEventHandler(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_SESSION_BEGIN, _speechEventHandler);
    IARM_Bus_RemoveEventHandler(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_SESSION_ABORT, _speechEventHandler);
    IARM_Bus_RemoveEventHandler(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_SESSION_SHORT, _speechEventHandler);
    IARM_Bus_RemoveEventHandler(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_RCU_IARM_EVENT_KEY_GHOST, _ctrlmMessageHandler);
  #else
    #ifdef USE_UNIFIED_RF4CE_MGR_API_4
    IARM_Bus_RemoveEventHandler(VREX_MGR_IARM_BUS_NAME, VREX_MGR_IARM_EVENT_VOICE_BEGIN, _speechEventHandler);
    IARM_Bus_RemoveEventHandler(VREX_MGR_IARM_BUS_NAME, VREX_MGR_IARM_EVENT_VOICE_SESSION_ABORT, _speechEventHandler);
    IARM_Bus_RemoveEventHandler(VREX_MGR_IARM_BUS_NAME, VREX_MGR_IARM_EVENT_VOICE_SESSION_SHORT, _speechEventHandler);
    #else
    IARM_Bus_RemoveEventHandler(IARM_BUS_VREXMGR_NAME, IARM_BUS_VREXMGR_EVENT_SPEECH, _speechEventHandler);
    #endif /* USE_UNIFIED_RF4CE_MGR_API_4 */
    #ifdef RF4CE_GENMSO_API
    IARM_Bus_RemoveEventHandler(IARM_BUS_RFMGR_NAME, IARM_BUS_RFMGR_EVENT_MSG_IND, _rfMessageHandler);
    #elif defined(RF4CE_API)
    IARM_Bus_RemoveEventHandler(IARM_BUS_RF4CEMGR_NAME, IARM_BUS_RF4CEMGR_EVENT_MSG_IND, _rf4ceMessageHandler);
    #elif defined(RF4CE_GPMSO_API)
    IARM_Bus_RemoveEventHandler(IARM_BUS_GPMGR_NAME, IARM_BUS_GPMGR_EVENT_MSG_IND, _gpMessageHandler);
    #else
    #warning "No RF4CE API defined"
    #endif
  #endif  /* USE_UNIFIED_CONTROL_MGR_API_1 */
#endif  /* NO_RF4CE */

    IARM_Bus_Disconnect();
    IARM_Bus_Term();
    PLAT_TERM();
    return IARM_RESULT_SUCCESS;
}

static void _systemStateChangeHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
    IARM_Bus_SYSMgr_EventData_t *sysEventData = (IARM_Bus_SYSMgr_EventData_t*)data;
    int state;
    int error;
    FILE *fp;
    unsigned long secs   = 0;
    IARM_Bus_SYSMgr_SystemState_t stateId = sysEventData->data.systemStates.stateId;
    state = sysEventData->data.systemStates.state;
    error = sysEventData->data.systemStates.error;

    if (strcmp(owner, IARM_BUS_SYSMGR_NAME)  == 0) 
    {
        switch(stateId) 
        {
            case IARM_BUS_SYSMGR_SYSSTATE_DSG_CA_TUNNEL:
            case IARM_BUS_SYSMGR_SYSSTATE_ECM_IP:
                if(1 == error)
                {
                    fp = fopen("/proc/uptime", "r");
                    if (NULL != fp)
                    {
                        setvbuf(fp, (char *) NULL, _IONBF, 0);
                        fseek(fp, 0, SEEK_SET);
                        if(0 > fscanf(fp, "%ld", &secs))
                        {
                            LOG("Error: fscanf on uptime failed \r\n");
                            /*Proc is not accessible(may never happen) taken as ecm connection lost */
                            ecm_connectivity_lost = 1;
                        }
                        fclose(fp);
                    }
                    else
                    {
                        LOG("Error: fopen on uptime failed \r\n");
                        /*Proc is not up(may never happen) taken as ecm connection lost */
                        ecm_connectivity_lost = 1;
                    }
                    /* Refer DELIA-6512 */ 
                    if(secs > 300)
                    {   
                        if(0 == ecm_connectivity_lost)
                        {
                            if (stateId == IARM_BUS_SYSMGR_SYSSTATE_ECM_IP)
                            {
                                LOG("[PwrMgr] ECM connectivity Lost on ECM_IP Event..\r\n");
                                ecm_connectivity_lost = 1; 
                            }
                            else if ((stateId == IARM_BUS_SYSMGR_SYSSTATE_DSG_CA_TUNNEL) && (0 == state))
                            {
                                LOG("[PwrMgr] ECM connectivity Lost on DSG_CA_TUNNEL Event ..\r\n");
                                ecm_connectivity_lost = 1;
                            }
                        }
                    }
                }
                else if(0 == error)
                {
                    /* Refer DELIA-6512 */ 
                    if(1 == ecm_connectivity_lost)
                    {
                        if (stateId == IARM_BUS_SYSMGR_SYSSTATE_ECM_IP)
                        {
                            LOG("[PwrMgr] ECM connectivity recovered on ECM_IP Event..\r\n");
                            ecm_connectivity_lost = 0;
                        }
                        else if ((stateId == IARM_BUS_SYSMGR_SYSSTATE_DSG_CA_TUNNEL) && (2 == state))
                        {
                            LOG("[PwrMgr] ECM connectivity recovered on DSG_CA_TUNNEL Event..\r\n");
                            ecm_connectivity_lost = 0;
                        }
                    }
                }
            break;
           
            default:
            break;
        }
    }
}

static void _irEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
	IARM_Bus_IRMgr_EventData_t *irEventData = (IARM_Bus_IRMgr_EventData_t*)data;

	int keyCode = irEventData->data.irkey.keyCode;
	int keyType = irEventData->data.irkey.keyType;
	int isFP = irEventData->data.irkey.isFP;

	int resetState = 0;

	{

            if((keyType != KET_KEYUP) && (keyType != KET_KEYDOWN) && (keyType != KET_KEYREPEAT))
            {
                LOG("Unexpected Key type recieved %X\r\n",keyType);
                return;
            }

	    /*LOG("Power Manager Get IR Key (%x, %x) From IR Manager\r\n", keyCode, keyType);*/
#ifdef _ENABLE_FP_KEY_SENSITIVITY_IMPROVEMENT
            if( (keyType == KET_KEYDOWN) && (keyCode == KED_FP_POWER))
            {
                PWRMGR_SET_POWER_KEYDOWN_TM();
            }
#endif
            
            if( (keyType == KET_KEYUP) && (keyCode == KED_FP_POWER) && isFP && ecm_connectivity_lost )
            {
                /*DELIA-1801:Requirement is to bring up the box in "ON" state always when there is a ecm connectivity loss*/
                LOG("[PwrMgr] Rebooting the box due to ECM connectivity Loss ..\r\n");
                m_settings.powerState = IARM_BUS_PWRMGR_POWERSTATE_ON;
                _WriteSettings(m_settingsFile);
                PwrMgr_Reset(IARM_BUS_PWRMGR_POWERSTATE_ON, false);
                return;
            }  
       
		{
            PWRMgr_Settings_t *pSettings = &m_settings;
            /* Intercept PowerKey */
            IARM_Bus_PWRMgr_PowerState_t curState = pSettings->powerState;
            IARM_Bus_PWRMgr_PowerState_t newState = ((curState == IARM_BUS_PWRMGR_POWERSTATE_ON) ? IARM_BUS_PWRMGR_POWERSTATE_STANDBY : IARM_BUS_PWRMGR_POWERSTATE_ON);
#ifdef _ENABLE_FP_KEY_SENSITIVITY_IMPROVEMENT
            //LOG("Power Manager Settings State vs new State(%x, %x) transition state %x \r\n", curState, newState, transitionState);
#endif           
			
			if (( (keyType == KET_KEYUP) && (keyCode == KED_DISCRETE_POWER_ON)  && (newState == IARM_BUS_PWRMGR_POWERSTATE_ON)  ) || 
                 ((keyType == KET_KEYUP) && (keyCode == KED_DISCRETE_POWER_STANDBY) && (newState == IARM_BUS_PWRMGR_POWERSTATE_STANDBY))) 
			{
            	IARM_Bus_PWRMgr_SetPowerState_Param_t param;
                LOG("Setting Discrete Powerstate cur=%d,new=%d\r\n", curState, newState);
				param.newState = newState;
                                param.keyCode = keyCode;
                _SetPowerState((void *)&param);
				return;
	        }
        
		}
		

	    if (keyCode == KED_UNDEFINEDKEY)
	        return;
	    else
	    {
	        static int skipWakupKey = 0;
	        int isWakeupKey = 1;
	        int isPassthruKey= 0;
	        static const int passthruKeys[] = {
	            KED_FP_POWER,
	            KED_RF_POWER,
	            KED_POWER,
	            KED_VOLUMEDOWN,
	            KED_VOLUMEUP,
	            KED_MUTE
	        };

			static const int skipWakeupKeys[] = {
                KED_MUTE,
                KED_VOLUMEDOWN,
                KED_VOLUMEUP,
                KED_DISCRETE_POWER_ON,
                KED_DISCRETE_POWER_STANDBY,
                KED_XR2V3,
                KED_XR5V2,
                KED_XR11V2,
                KED_XR13,
                KED_XR11_NOTIFY,
                KED_XR15V1_NOTIFY,
                KED_INPUTKEY,
                KED_UNDEFINEDKEY,
            };
			
			static const char *keytypeString[3] = {"Pressed", "Released", "Repeat"};

	#ifdef _ENABLE_WAKEUP_KEY
	{
	            int i = 0;
	            for (i = 0; i < sizeof(passthruKeys) / sizeof(passthruKeys[0]); i++) {
	                if (keyCode == passthruKeys[i]) {
	                    isPassthruKey = 1;
	                    break;
	                }
	            }

			   for (i = 0; i < sizeof(skipWakeupKeys) / sizeof(skipWakeupKeys[0]); i++) {
                if (keyCode == skipWakeupKeys[i]) {
                    isWakeupKey = 0;
                    break;
				}
				}
	        }
	#else
	        {
	            isPassthruKey = 1;
	            isWakeupKey = 0;
	        }
	#endif

			if(isWakeupKey) {
				__TIMESTAMP();LOG("%s Power %s\n",((isFP == 1) ? "FrontPanel":"RF/IR"),keytypeString[((keyType>>8)-0x80)]);		
			}
            
            #ifdef _DISABLE_KEY_POWEROFF
                const int disableFPResetLogic = 1;
            #else
                const int disableFPResetLogic = 0;
            #endif
            
            if (disableFPResetLogic && keyCode == KED_FP_POWER && isFP) { 
                printf("Disable FP Reset Logic\r\n");
            }else {
                /* check if key is Control key and skip Reset sequence step if so */
    	        static const int skipControlKeys[] = {
                    KED_XR2V3,
                    KED_XR5V2,
                    KED_XR11V2,
                    KED_XR13
                    };
    	        bool is_control_key = false;
	        for (int i = 0; i < sizeof(skipControlKeys) / sizeof(skipControlKeys[0]); ++i) {
	            if (keyCode == skipControlKeys[i]) {
	               	is_control_key = true;
	                break;
	                }
	            }
                if (!is_control_key) {
                   resetState = checkResetSequence(keyType,keyCode);
                   }
            	else {
                   __TIMESTAMP();LOG("Control Key (%02x, %02x), do not check Reset sequence", keyCode, keyType);
                   }
            }
            
            //LOG("After resetcheck Power Manager keyCode and isWakeupKey(%x, %x, %x) \r\n", keyCode, keyType, isWakeupKey);
	        /* Only wakup key or power key can change power state */
	        if ((keyCode == KED_FP_POWER) || (keyCode == KED_RF_POWER) || isWakeupKey)
	        {
	        	PWRMgr_Settings_t *pSettings = &m_settings;
	            /* Intercept PowerKey */
	        	IARM_Bus_PWRMgr_PowerState_t curState = pSettings->powerState;
#ifdef _ENABLE_FP_KEY_SENSITIVITY_IMPROVEMENT
                if (POWER_KEY_SENSITIVITY) {
                    curState = transitionState;
                }
#endif
                IARM_Bus_PWRMgr_PowerState_t newState;
                dsSleepMode_t  sleepMode = dsHOST_SLEEP_MODE_LIGHT;
                try
                {
                    sleepMode = static_cast<dsSleepMode_t>(device::Host::getInstance().getPreferredSleepMode().getId());
                }
                catch(...)
                {
                    LOG("PwrMgr: Exception coughht while processing getPreferredSleepMode\r\n");
                }
                if(sleepMode == dsHOST_SLEEP_MODE_DEEP)
                {
                    newState = ((curState == IARM_BUS_PWRMGR_POWERSTATE_ON) ? IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP: IARM_BUS_PWRMGR_POWERSTATE_ON);
                }
                else
                {
                    newState = ((curState == IARM_BUS_PWRMGR_POWERSTATE_ON) ? IARM_BUS_PWRMGR_POWERSTATE_STANDBY : IARM_BUS_PWRMGR_POWERSTATE_ON);
                }
                if((keyCode == KED_DEEPSLEEP_WAKEUP) && (keyType == KET_KEYUP) && (curState == IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP))
                {
                    #ifdef PLATCO_BOOTTO_STANDBY                    
                        newState = IARM_BUS_PWRMGR_POWERSTATE_STANDBY;
                        LOG("KED_DEEPSLEEP_WAKEUP in DEEP_SLEEP so change the state to STANDBY cur=%d,new=%d\r\n", curState, newState);
                    #else
                        newState = IARM_BUS_PWRMGR_POWERSTATE_ON;
                        LOG("KED_DEEPSLEEP_WAKEUP in DEEP_SLEEP so change the state to ON cur=%d,new=%d\r\n", curState, newState);
                    #endif
                }
#ifdef _ENABLE_FP_KEY_SENSITIVITY_IMPROVEMENT
                //LOG("After adjustment Power Manager Settings State vs new State(%x, %x) transition state %x \r\n", curState, newState, transitionState);
#endif
	            
	            {
	                /* On KEY_KEYDOWN, Check if to skip the wakeup key :
	                 * Skip the first key (DOWN/REPEAT/UP) if in standby mode
	                 * Passthru the key if (DOWN/REPEAT/UP) in ON mode
	                 */
	                if (isWakeupKey && keyType == KET_KEYDOWN) {
	                    if (curState != IARM_BUS_PWRMGR_POWERSTATE_ON) {
	                        skipWakupKey = 0;
	                    }
	                    else {
	                        skipWakupKey = 0;
	                    }
	                }
	            }

    #ifdef _DISABLE_KEY_POWEROFF
            const int disableKeyPowerOff = 1;
    #else
            const int disableKeyPowerOff = 0;
    #endif

	#ifdef _ENABLE_WAKEUP_KEY
	            /* For IR POWER, always switch to ON state, for FP POWER, always toggle the power */
                   if (((keyCode == KED_RF_POWER || isWakeupKey) && curState != IARM_BUS_PWRMGR_POWERSTATE_ON && (keyCode == KED_SELECT ? difftime(time(NULL), xre_timer) >= 3.0:1)) || (keyCode == KED_FP_POWER))
	#else
	            /* For IR POWER, always toggle the power, for FP POWER, always toggle the power */
                   if ((keyCode == KED_RF_POWER) || (isWakeupKey && curState != IARM_BUS_PWRMGR_POWERSTATE_ON && (keyCode == KED_SELECT ? difftime(time(NULL), xre_timer) >= 3.0:1)) || (keyCode == KED_FP_POWER))
	#endif
	            {
#ifdef _ENABLE_FP_KEY_SENSITIVITY_IMPROVEMENT
                    LOG("before handling Power Manager Settings State vs new State(%x, %x) transition state %x \r\n", curState, newState, transitionState);
#endif
	                /* Notify application of Power Changes */
	                if (keyType == KET_KEYUP && curState != newState && (resetState == 0)) {
	                    LOG("Setting Powerstate cur=%d,new=%d\r\n", curState, newState);
	                    IARM_Bus_PWRMgr_SetPowerState_Param_t param;
	                    param.newState = newState;
                            param.keyCode = keyCode;
			int doNothandlePowerKey = ((disableKeyPowerOff) && (curState == IARM_BUS_PWRMGR_POWERSTATE_ON) && (newState != IARM_BUS_PWRMGR_POWERSTATE_ON));
                        if ((deepSleepWakeup) && (curState != IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP)){
                            doNothandlePowerKey = 1;
                            LOG("RDK_DEEPSLEEP_WAKEUP_ON_POWER_BUTTON is set, power-ON only from DEEPSLEEP\r\n");
                        }

#ifdef _ENABLE_FP_KEY_SENSITIVITY_IMPROVEMENT
                            if (doNothandlePowerKey) {
                                printf("Ignore Power Key when in ON state \r\n");
                            }
                            else
                            {
                                PWRMGR_SET_POWER_KEYUP_TM();
                                if (PWRMGR_GET_POWER_KEY_INTERVAL() >= POWER_KEY_SENSITIVITY) {
                                    LOG("Taking  PowerKey for Sensitivity %d,  cur=%d,new=%d\r\n", PWRMGR_GET_POWER_KEY_INTERVAL(), curState, newState);
                                    _SetPowerStateAsync(curState, newState);
                                }
                                else {
                                    LOG("Ignoring PowerKey for Sensitivity %d,  cur=%d,new=%d\r\n", PWRMGR_GET_POWER_KEY_INTERVAL(), curState, newState);
                                }
                            }
#else
                        if (!doNothandlePowerKey) {
                            _SetPowerState((void *)&param);
                        }
#endif
                        if (!doNothandlePowerKey) {
                            setResetPowerState(newState);
                        }
	                }
                    else {
	                    LOG("NOT NOT NOT Setting Powerstate cur=%d,new=%d on keyType %x\r\n", curState, newState, keyType);
                    }
	            }
	            else
	            {
	                /* Do nothing */
	            }
	        }
	    }
	}

}

static IARM_Result_t _SetPowerState(void *arg)
{
	IARM_Result_t retCode = IARM_RESULT_SUCCESS;
	IARM_Bus_PWRMgr_SetPowerState_Param_t *param = (IARM_Bus_PWRMgr_SetPowerState_Param_t *)arg;
	PWRMgr_Settings_t *pSettings = &m_settings;
	IARM_Bus_CommonAPI_PowerPreChange_Param_t powerPreChangeParam;
	IARM_Bus_PWRMgr_PowerState_t newState = param->newState;
	IARM_Bus_PWRMgr_PowerState_t curState = pSettings->powerState;    /* Notify application of Power Changes */
	static const char *powerstateString[5] = {"OFF","STANDBY","ON", "LIGHTSLEEP", "DEEPSLEEP"};

	if(curState != newState) {

#ifdef ENABLE_DEEP_SLEEP     
		/* * When Changing from Deep sleep wakeup 
		 * Notify Deep sleep manager first followed by 
		 * Power pre change call.
		 */  
		//Changing power state ,Remove event source 
		if(dsleep_bootup_event_src)
		{
			g_source_remove(dsleep_bootup_event_src);
			dsleep_bootup_event_src = 0;
			__TIMESTAMP();LOG("Removed Deep sleep boot up event Time source %d  \r\n",dsleep_bootup_event_src);
		}

		if(  (IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP == curState)
				&& (IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP != newState))
		{
			IARM_Result_t rpcRet = IARM_RESULT_SUCCESS;
			int dsStatus= 0;
			rpcRet = IARM_Bus_Call(IARM_BUS_DEEPSLEEPMGR_NAME,
					(char *)"GetDeepSleepStatus",
					(void *)&dsStatus,
					sizeof(dsStatus));
			if((DeepSleepStatus_InProgress == dsStatus) || (IARM_RESULT_SUCCESS != rpcRet))
			{
				LOG("%s  deepsleep in  progress  ignoreing the request dsStatus %d rpcRet :%d\r\n",__FUNCTION__,dsStatus,rpcRet);
				return retCode;
			}
			IARM_Bus_CommonAPI_PowerPreChange_Param_t deepSleepWakeupParam;

			__TIMESTAMP();LOG("Waking up from Deep Sleep.. \r\n");
			deepSleepWakeupParam.curState = curState;     
			deepSleepWakeupParam.newState = newState;

			/* Notify Deep sleep manager on Power Mode change */
			IARM_BusDaemon_DeepSleepWakeup(deepSleepWakeupParam);
		}
#endif

		__TIMESTAMP();LOG("Power Mode Change from %s to %s start\n",powerstateString[curState],powerstateString[newState]);

		powerPreChangeParam.newState = param->newState;
		powerPreChangeParam.curState = pSettings->powerState;  

		IARM_BusDaemon_PowerPrechange(powerPreChangeParam);
		__TIMESTAMP();LOG("Power Mode Change from %s to %s end\n",powerstateString[curState],powerstateString[newState]);

		if (nullptr != ux) //If ux_controller is supported, it will set up AV ports and LEDs in the below call.
			ux->applyPowerStateChangeConfig(newState, curState);
		else { // Since ux_controller is not supported, we needs to set up AV ports and LED here.
			_SetAVPortsPowerState(newState);
			_SetLEDStatus(newState);
		}
#ifdef OFFLINE_MAINT_REBOOT
		if(newState != IARM_BUS_PWRMGR_POWERSTATE_ON){

			standby_time = g_get_monotonic_time()/G_USEC_PER_SEC;
			LOG("Power state changed at %lld\n", standby_time);
		}
#endif

		pSettings->powerState = newState;
		_WriteSettings(m_settingsFile);
#ifdef _ENABLE_FP_KEY_SENSITIVITY_IMPROVEMENT
		if(transitionState != newState)
		{
			transitionState = newState;
		}
		if(targetState != newState)
		{
			targetState = newState;
		}
#endif

		if (newState != IARM_BUS_PWRMGR_POWERSTATE_ON) {
			time(&xre_timer); // Hack to fix DELIA-11393
			LOG("Invoking clean up script\r\n");
			if(param->keyCode != KED_FP_POWER)
			{
				system("/lib/rdk/standbyCleanup.sh");
			}
			else
			{
				__TIMESTAMP();LOG("Standby operation due to KED_FP_POWER key press, Invoking script with forceShutdown \r\n");
				system("/lib/rdk/standbyCleanup.sh --forceShutdown");
			}
		}

		/* Independent of Deep sleep */
		PLAT_API_SetPowerState(newState);

		/*  * Power Change Event 
		 * Used by Deep sleep and HDMI CEC. 
		 */    
		LOG("[PwrMgr] Post Power Mode Change Event \r\n");    
		{
			IARM_Bus_PWRMgr_EventData_t _eventData;
			_eventData.data.state.curState = curState;
			_eventData.data.state.newState = newState;

#ifdef ENABLE_DEEP_SLEEP
			if(IsWakeupTimerSet)
			{
				/* Use the wakeup timer set by XRE */
				_eventData.data.state.deep_sleep_timeout = deep_sleep_wakeup_timeout_sec;
			}
			else
			{ 
				/* Set the  wakeup time till 2AM */
				deep_sleep_wakeup_timeout_sec = getWakeupTime();
				_eventData.data.state.deep_sleep_timeout = deep_sleep_wakeup_timeout_sec;
			}

#ifdef ENABLE_LLAMA_PLATCO_SKY_XIONE
			_eventData.data.state.nwStandbyMode = nwStandbyMode_gs;
#endif


			/* Start a Deep sleep Wakeup Time source
			 * Reboot the box after elapse of user configured / Calculated  Timeout.
			 */  
			if(IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP == newState)
			{

				/* 
				   - In Deep Sleep, the timer resets and the does not reboot after wakeup time out 
				   - As Gloop calls the handler only after Soc wakeup from dep sleep and after expiry of deep_sleep_wakeup_timeout_sec 
				   - So if Wakeup timeout is 10 minutes , box reboots after 20 minutes,
				   - To handle this, the handler is called every 30 sec and check for expiry of deep sleep wakeup timeout. 
				   */    
				time(&timeAtDeepSleep);
				wakeup_event_src = g_timeout_add_seconds ((guint)30,deep_sleep_wakeup_fn,pwrMgr_Gloop); 
				__TIMESTAMP();LOG("Added Deep Sleep Wakeup Time Source %d for %d Sec \r\n",wakeup_event_src,deep_sleep_wakeup_timeout_sec);
#ifdef ENABLE_LLAMA_PLATCO_SKY_XIONE
				__TIMESTAMP();LOG("Networkstandbymode for Source %d is: %s \r\n",wakeup_event_src, (nwStandbyMode_gs?("Enabled"):("Disabled")));
#endif

			}
			else if(wakeup_event_src)
			{
				//We got some key event, Remove event source 
				__TIMESTAMP();LOG("Removed Deep sleep Wakeup Time source %d for %d Sec.. \r\n",wakeup_event_src,deep_sleep_wakeup_timeout_sec);
				g_source_remove(wakeup_event_src);
				wakeup_event_src = 0;
				timeAtDeepSleep = 0;
			}
#endif
			IARM_Bus_BroadcastEvent( IARM_BUS_PWRMGR_NAME, 
					IARM_BUS_PWRMGR_EVENT_MODECHANGED, (void *)&_eventData, sizeof(_eventData));
			if(IARM_BUS_PWRMGR_POWERSTATE_ON == newState)
			{
				__TIMESTAMP();LOG("IARMCEC_SendCECImageViewOn and IARMCEC_SendCECActiveSource. \r\n");
				IARMCEC_SendCECImageViewOn(isCecLocalLogicEnabled);
				IARMCEC_SendCECActiveSource(isCecLocalLogicEnabled,KET_KEYDOWN,KED_MENU);
			}
		}

	}
	else
	{
		LOG("Warning:PowerState is same as requested\r\n");
	}
	return retCode;
}


static IARM_Result_t _GetPowerState(void *arg)
{
    PWRMgr_Settings_t *pSettings = &m_settings;
    IARM_Bus_PWRMgr_GetPowerState_Param_t *param = (IARM_Bus_PWRMgr_GetPowerState_Param_t *)arg;
    param->curState = pSettings->powerState;
     //LOG("_GetPowerState return %d\r\n", pSettings->powerState);
    return IARM_RESULT_SUCCESS;
}

static IARM_Result_t _GetPowerStateBeforeReboot(void *arg)
{
    IARM_Bus_PWRMgr_GetPowerStateBeforeReboot_Param_t *param = (IARM_Bus_PWRMgr_GetPowerStateBeforeReboot_Param_t *)arg;
    memset (param->powerStateBeforeReboot, '\0', MAX_PWR_STATE_BEF_REBOOR_STR_LEN);
    strncpy (param->powerStateBeforeReboot, powerStateBeforeReboot_gc.c_str(), strlen(powerStateBeforeReboot_gc.c_str()));
    return IARM_RESULT_SUCCESS;
}


static IARM_Result_t _HandleReboot(void *arg)
{
    IARM_Bus_PWRMgr_RebootParam_t *param = (IARM_Bus_PWRMgr_RebootParam_t *)arg;
    param->reboot_reason_custom[sizeof(param->reboot_reason_custom) - 1] = '\0'; //Just to be on the safe side.
    param->reboot_reason_custom[sizeof(param->reboot_reason_other) - 1] = '\0';
    param->requestor[sizeof(param->requestor) - 1] = '\0';

    if(nullptr != ux)
    {
         if(0 == strncmp(PWRMGR_REBOOT_REASON_MAINTENANCE, param->reboot_reason_custom, sizeof(param->reboot_reason_custom)))
            ux->applyPreMaintenanceRebootConfig(m_settings.powerState);
         else
            ux->applyPreRebootConfig(m_settings.powerState);
    }
    performReboot(param->requestor, param->reboot_reason_custom, param->reboot_reason_other);
    return IARM_RESULT_SUCCESS;
}

static IARM_Result_t _WareHouseReset(void *arg)
{
    IARM_Bus_PWRMgr_WareHouseReset_Param_t *param = (IARM_Bus_PWRMgr_WareHouseReset_Param_t *)arg;
    int ret = param->suppressReboot ? processWHResetNoReboot() : processWHReset();
    LOG("_WareHouseReset returned : %d\r\n", ret);
    fflush(stdout);
    if (ret == 0)
       return IARM_RESULT_SUCCESS;
    else
       return IARM_RESULT_IPCCORE_FAIL;
}

static IARM_Result_t _WareHouseClear(void *arg)
{
    IARM_Bus_PWRMgr_WareHouseReset_Param_t *param = (IARM_Bus_PWRMgr_WareHouseReset_Param_t *)arg;
    int ret = param->suppressReboot ? processWHClearNoReboot() : processWHClear();
    LOG("_WareHouseClear returned : %d\r\n", ret);
    fflush(stdout);
    if (ret == 0)
       return IARM_RESULT_SUCCESS;
    else
       return IARM_RESULT_IPCCORE_FAIL;
}

static IARM_Result_t _ColdFactoryReset(void *)
{
    int ret = processColdFactoryReset();
    LOG("_ColdFactoryReset returned : %d\r\n", ret);
    fflush(stdout);
    if (ret == 0)
        return IARM_RESULT_SUCCESS;
    else
        return IARM_RESULT_IPCCORE_FAIL;
}

static IARM_Result_t _FactoryReset(void *)
{
    int ret = processFactoryReset();
    LOG("_FactoryReset returned : %d\r\n", ret);
    fflush(stdout);
    if (ret == 0)
        return IARM_RESULT_SUCCESS;
    else
        return IARM_RESULT_IPCCORE_FAIL;
}

static IARM_Result_t _UserFactoryReset(void *)
{
    int ret = processUserFactoryReset();
    LOG("_UserFactoryReset returned : %d\r\n", ret);
    fflush(stdout);
    if (ret == 0)
        return IARM_RESULT_SUCCESS;
    else
        return IARM_RESULT_IPCCORE_FAIL;
}

static IARM_Result_t _SetStandbyVideoState(void *arg)
{
    IARM_Bus_PWRMgr_StandbyVideoState_Param_t *param = (IARM_Bus_PWRMgr_StandbyVideoState_Param_t *)arg;
    if(NULL == param->port)
    {
        param->result = -1;
        __TIMESTAMP();LOG("[PwrMgr] empty port name. Cannot proceed.\n");
        return IARM_RESULT_SUCCESS;
    }
    else
        param->result = 0;

    int i = 0;
    for(i = 0; i < MAX_NUM_VIDEO_PORTS; i++)
    {
        if(0 == strncasecmp(param->port, g_standby_video_port_setting[i].port, PWRMGR_MAX_VIDEO_PORT_NAME_LENGTH))
        {
            /*Found a match. Update it*/
            g_standby_video_port_setting[i].isEnabled = ((0 == param->isEnabled) ? false : true);
            break;
        }
    }
    if(MAX_NUM_VIDEO_PORTS == i)
    {
        /*No matching entries are present. Add one.*/
        for(i = 0; i < MAX_NUM_VIDEO_PORTS; i++)
        {
            if('\0' == g_standby_video_port_setting[i].port[0])
            {
                strncpy(g_standby_video_port_setting[i].port, param->port, (PWRMGR_MAX_VIDEO_PORT_NAME_LENGTH - 1));
                g_standby_video_port_setting[i].isEnabled = ((0 == param->isEnabled) ? false : true);
                break;
            }
        }
    }
    if(MAX_NUM_VIDEO_PORTS == i)
    {
        __TIMESTAMP();LOG("Error! Out of room to write new video port setting for standby mode.\n");
    }

    try
    {
        device::VideoOutputPort &vPort = device::Host::getInstance().getVideoOutputPort(param->port);
        if((IARM_BUS_PWRMGR_POWERSTATE_ON != m_settings.powerState) && (IARM_BUS_PWRMGR_POWERSTATE_OFF != m_settings.powerState))
        {
            /*We're currently in one of the standby states. This new setting needs to be applied right away.*/
            __TIMESTAMP();LOG("[PwrMgr] Setting standby %s port status to %s.\n", param->port, ((1 == param->isEnabled)? "enabled" : "disabled"));
            if(1 == param->isEnabled)
                vPort.enable();
            else
                vPort.disable();
        }
        else
        {
            __TIMESTAMP();LOG("[PwrMgr] video port %s will be %s when going into standby mode.\n", param->port, ((1 == param->isEnabled)? "enabled" : "disabled"));
        }
    }
    catch (...)
    {
        __TIMESTAMP();LOG("Exception Caught during [PWRMgr - _SetStandbyVideoState]. Possible bad video port.\n");
        param->result = -1;
    }
    return IARM_RESULT_SUCCESS;
}

static IARM_Result_t _GetStandbyVideoState(void *arg)
{
    IARM_Bus_PWRMgr_StandbyVideoState_Param_t *param = (IARM_Bus_PWRMgr_StandbyVideoState_Param_t *)arg;
    if(NULL == param->port)
    {
        __TIMESTAMP();LOG("Bad port name. Cannot get state.\n");
        return IARM_RESULT_SUCCESS;
    }

    try
    {
        device::VideoOutputPort &vPort = device::Host::getInstance().getVideoOutputPort(param->port);

    }
    catch (...)
    {
        __TIMESTAMP();LOG("Exception Caught during [PWRMgr - _GetStandbyVideoState]. Possible bad video port.\n");
        param->result = -1;
        return IARM_RESULT_SUCCESS;
    }
    param->isEnabled = ((true == get_video_port_standby_setting(param->port))? 1 : 0);
    param->result = 0;
    return IARM_RESULT_SUCCESS;
}

static int _InitSettings(const char *settingsFile)
{
    if (settingsFile == NULL) settingsFile = "/opt/uimgr_settings.bin";

    m_settingsFile = settingsFile;
    LOG("Initializing settings at file %s\r\n", settingsFile);

    int ret = open(settingsFile, O_CREAT|O_RDWR, S_IRWXU|S_IRUSR);
    int fd = ret;
    struct stat buf;

    if (fd >= 0) {

        PWRMgr_Settings_t *pSettings = &m_settings;
        int read_size = sizeof(uint32_t) * 3;
        lseek(fd, 0, SEEK_SET);
        ret = read(fd, pSettings,read_size);
        if((ret == read_size))
        {
            switch(pSettings->version)
            {
                case 0:
                    {
                        UIMgr_Settings_t uiMgrSettings;
                        lseek(fd, 0, SEEK_SET);
						pSettings->length = sizeof(UIMgr_Settings_t) - PADDING_SIZE;
                        read_size = pSettings->length; 
                        ret = read(fd, &uiMgrSettings,read_size);
                        if(ret == read_size)
                        {

                            pSettings->magic = _UIMGR_SETTINGS_MAGIC;
                            pSettings->version = 1;
                            pSettings->length = sizeof(PWRMgr_Settings_t) - PADDING_SIZE;
                            pSettings->powerState = 
                            _ConvertUIDevToIARMBusPowerState(uiMgrSettings.powerState);
                            g_last_known_power_state = pSettings->powerState;
                           #ifdef ENABLE_DEEP_SLEEP
                                 pSettings->deep_sleep_timeout = deep_sleep_wakeup_timeout_sec;
                            #endif
                            #ifdef ENABLE_LLAMA_PLATCO_SKY_XIONE
                                pSettings->nwStandbyMode = nwStandbyMode_gs;
                            #endif
                            lseek(fd, 0, SEEK_SET);
                            write(fd, pSettings, pSettings->length);

                        }
                        else
                        {
                            ret = 0;  //error case, not able to read full length
                        }
                    }

                    break;  
                case 1:
                    {
                        if((pSettings->length == (sizeof(PWRMgr_Settings_t) - PADDING_SIZE )))
						{
							LOG("[PwrMgr] Length of Persistence matches with Current Data Size \r\n");
                            lseek(fd, 0, SEEK_SET);
							read_size = pSettings->length; 
							ret = read(fd, pSettings,read_size);
							if(ret != read_size)
							{
								ret = 0;  //error case, not able to read full length
                                LOG("[PwrMgr] error case, not able to read full length \r\n");
							}
                            else
                            {
                                g_last_known_power_state = pSettings->powerState;
                                #ifdef ENABLE_DEEP_SLEEP
                                    deep_sleep_wakeup_timeout_sec = pSettings->deep_sleep_timeout;
                                    __TIMESTAMP();LOG("Persisted deep_sleep_delay = %d Secs \r\n",deep_sleep_wakeup_timeout_sec);
                               #endif
                               #ifdef ENABLE_LLAMA_PLATCO_SKY_XIONE
                                   nwStandbyMode_gs = pSettings->nwStandbyMode;
                                    __TIMESTAMP();LOG("Persisted network standby mode is: %s \r\n", nwStandbyMode_gs?("Enabled"):("Disabled"));
                               #endif
                               #ifdef PLATCO_BOOTTO_STANDBY
                                   if(stat("/tmp/pwrmgr_restarted",&buf) != 0)
                                   {
                                       setPowerStateBeforeReboot (pSettings->powerState);
                                       pSettings->powerState = IARM_BUS_PWRMGR_POWERSTATE_STANDBY;
                                       __TIMESTAMP();LOG("Setting default powerstate to standby\n\r");
                                   }
                               #endif
                           }
						}
                        else if (((pSettings->length < (sizeof(PWRMgr_Settings_t) - PADDING_SIZE ))))
                        {
                            /* New Code reading the old version persistent file  information */
                            LOG("[PwrMgr] Length of Persistence is less than  Current Data Size \r\n");
                            lseek(fd, 0, SEEK_SET);
                            read_size = pSettings->length;
                            ret = read(fd, pSettings,read_size);
                            if(ret != read_size)
                            {
                                LOG("[PwrMgr] Read Failed for Data Length %d \r\n",ret);
                                ret = 0;  //error case, not able to read full length
                            }
                            else
                            {
                               /*TBD - The struct should be initialized first so that we dont need to add 
                                        manually the new fields. 
                                */
                               g_last_known_power_state = pSettings->powerState;
                               lseek(fd, 0, SEEK_SET);         
                               #ifdef ENABLE_DEEP_SLEEP
                                   pSettings->deep_sleep_timeout = deep_sleep_wakeup_timeout_sec;
                               #endif 
                               #ifdef ENABLE_LLAMA_PLATCO_SKY_XIONE
                                   pSettings->nwStandbyMode = nwStandbyMode_gs;
                               #endif
                               #ifdef PLATCO_BOOTTO_STANDBY
                               if(stat("/tmp/pwrmgr_restarted",&buf) != 0) {
                                   setPowerStateBeforeReboot (pSettings->powerState);
                                   pSettings->powerState = IARM_BUS_PWRMGR_POWERSTATE_STANDBY;
                               }
                               #endif
                              pSettings->length = (sizeof(PWRMgr_Settings_t) - PADDING_SIZE );
                                LOG("[PwrMgr] Write PwrMgr Settings File With Current Data Length %d \r\n",pSettings->length);
                                ret = write(fd, pSettings, pSettings->length);
                                if(ret != pSettings->length)
                                {
                                    LOG("[PwrMgr] Write Failed For  New Data Length %d \r\n",ret);
                                    ret = 0;  //error case, not able to read full length
                                }
                            }
                        }
                        else if (((pSettings->length > (sizeof(PWRMgr_Settings_t) - PADDING_SIZE ))))
                        {
                            /* Old Code reading the migrated new  version persistent file  information */
                            LOG("[PwrMgr] Length of Persistence is more than  Current Data Size. \r\n");
                       
                            lseek(fd, 0, SEEK_SET);
                            read_size = (sizeof(PWRMgr_Settings_t) - PADDING_SIZE );
                            ret = read(fd,pSettings,read_size);
                            if(ret != read_size)
                            {
                                LOG("[PwrMgr] Read Failed for Data Length %d \r\n",ret);
                                ret = 0;  //error case, not able to read full length
                            }
                            else
                            {
                               /*Update the length and truncate the file */
                                g_last_known_power_state = pSettings->powerState;
                                lseek(fd, 0, SEEK_SET);         
                                pSettings->length = (sizeof(PWRMgr_Settings_t) - PADDING_SIZE );
                                LOG("[PwrMgr] Write and Truncate  PwrMgr Settings File With Current  Data Length %d ........\r\n",pSettings->length);
                                ret = write(fd, pSettings, pSettings->length);
                                if(ret != pSettings->length)
                                {
                                    LOG("[PwrMgr] Write Failed For  New Data Length %d \r\n",ret);
                                    ret = 0;  //error case, not able to read full length
                                }
                                else
                                {
                                    /* Truncate the File information */
                                    int fret = 0;
                                    lseek(fd, 0, SEEK_SET);
                                    fret = ftruncate(fd,pSettings->length);
                                    if(fret != 0)
                                    {
                                        LOG("[PwrMgr] Truncate Failed For  New Data Length %d \r\n",fret);
                                        ret = 0;  //error case, not able to read full length
                                    }
                                }
                                
                            }

                        }
                        else
						{
							ret = 0;  //Version 1 but not with current size and data...
						}
                    }
                    break;
                default:
                    ret = 0; //Consider it as an invalid file.
            }
        }
        else
        {
            /**/
            #ifdef PLATCO_BOOTTO_STANDBY
            if(stat("/tmp/pwrmgr_restarted",&buf) != 0) {
                powerStateBeforeReboot_gc = std::string("UNKNOWN");
                printf ("[%s]:[%d] powerStateBeforeReboot: %s\n", __FUNCTION__, __LINE__, powerStateBeforeReboot_gc.c_str());
            }
            #endif
            ret = 0;
        } 
        if (ret == 0) {
            
            lseek(fd, 0, SEEK_SET);         
            LOG("Initial Creation of UIMGR Settings\r\n");
            pSettings->magic = _UIMGR_SETTINGS_MAGIC;
            pSettings->version = 1;
            pSettings->length = sizeof(*pSettings) - PADDING_SIZE;
            pSettings->powerState = IARM_BUS_PWRMGR_POWERSTATE_ON;
            #ifdef ENABLE_DEEP_SLEEP
                pSettings->deep_sleep_timeout = deep_sleep_wakeup_timeout_sec;
            #endif
            #ifdef ENABLE_LLAMA_PLATCO_SKY_XIONE
                pSettings->nwStandbyMode = nwStandbyMode_gs;
            #endif
            #ifdef PLATCO_BOOTTO_STANDBY
            if(stat("/tmp/pwrmgr_restarted",&buf) != 0)
                pSettings->powerState = IARM_BUS_PWRMGR_POWERSTATE_STANDBY;
            #endif
            ret = write(fd, pSettings, pSettings->length);
            if (ret < 0) {
            }
        }

        #ifdef ENABLE_DEEP_SLEEP
            /* If Persistent power mode is Deep Sleep 
               start a thread to put box to deep sleep after specified time. 
            */
                if(pSettings->powerState == IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP)
                {
                    __TIMESTAMP();LOG("Box Reboots with Deep Sleep mode.. Start a Event Time SOurce  .. \r\n");
                      guint dsleep_bootup_timeout = 3600; //In sec  
                      dsleep_bootup_event_src = g_timeout_add_seconds (dsleep_bootup_timeout,invoke_deep_sleep_on_bootup_timeout,pwrMgr_Gloop); 
                    __TIMESTAMP();LOG("Added Time source %d to put the box to deep sleep after %d Sec.. \r\n",dsleep_bootup_event_src,dsleep_bootup_timeout);
                }
        #endif 
    

        /* Sync with platform if it is supported */
        {
            if(nullptr != ux)
                ux->applyPostRebootConfig(pSettings->powerState, g_last_known_power_state); // This will set up ports, lights and bootloader pattern internally.

       	    IARM_Bus_PWRMgr_PowerState_t state;
            ret = PLAT_API_GetPowerState(&state);
            if (ret == 0) {
                if (pSettings->powerState == state) {
                    LOG("PowerState is already sync'd with hardware to %d\r\n", state);
                }
                else {
                    int loopCount = 0;
                    LOG("PowerState sync hardware state %d with UIMGR to %d\r\n", state, pSettings->powerState);
                    do {
                        loopCount++;
                        if(nullptr == ux) // Since ux_controller is not supported, ports need to be set up explicitly.
                            _SetAVPortsPowerState(pSettings->powerState);                        
                        ret = PLAT_API_SetPowerState(pSettings->powerState);
                        sleep(1);
                        PLAT_API_GetPowerState(&state);
                    } while(state != pSettings->powerState && loopCount < 10);

                    if (state != pSettings->powerState) {
                        LOG("CRITICAL ERROR: PowerState sync failed \r\n");
                        pSettings->powerState = state;
                    }
                }
                if(nullptr == ux) // Since ux_controller is not supported, ports need to be set up explicitly.
                {
                    _SetAVPortsPowerState(pSettings->powerState);
                    _SetLEDStatus(pSettings->powerState);
                }

                if (fd >= 0) {
                    lseek(fd, 0, SEEK_SET);
                    ret = write(fd, pSettings, pSettings->length);
                    if (ret < 0) {
                    }
                }
            }
            else {
                /* Use settings stored in uimgr file */
            }
        }

        #ifdef PLATCO_BOOTTO_STANDBY
        if(stat("/tmp/pwrmgr_restarted",&buf) != 0) { 
            IARM_Bus_PWRMgr_EventData_t _eventData;
            _eventData.data.state.curState = IARM_BUS_PWRMGR_POWERSTATE_OFF;
            _eventData.data.state.newState = IARM_BUS_PWRMGR_POWERSTATE_STANDBY;
            LOG("%s: Init setting powermode change from OFF to STANDBY \r\n", __FUNCTION__);
            IARM_Bus_BroadcastEvent( IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_EVENT_MODECHANGED, (void *)&_eventData, sizeof(_eventData));
        } 
        #endif

        if (ret > 0 && ret < (int)sizeof(*pSettings)) {
            _DumpSettings(pSettings);
            /* @TODO: assert pSettings->length == FileLength */
        }

        if (ret > (int)sizeof(*pSettings)) {
            LOG("Error: Should not have read that much ! \r\n");
            /* @TODO: Error Handling */
            /* Init Settings to default, truncate settings to 0 */
        }

        fsync(fd);
        close(fd);
    }

    if (ret < 0) {
        LOG("Error: Failed to operate on uimgr_settings.bin, reason=[%s]\r\n", strerror(errno));
    }

    return ret;
}


static int _WriteSettings(const char *settingsFile)
{
    PWRMgr_Settings_t *pSettings = &m_settings;
    int fd = open(settingsFile, O_WRONLY);
    int ret = fd;

    if (fd >= 0) {
        lseek(fd, 0, SEEK_SET);
        PWRMgr_Settings_t *pSettings = &m_settings;
        ret = write(fd, pSettings, pSettings->length);
        fsync(fd);
        close(fd);
    }

    if (ret < 0) {
        LOG("Error: Failed to write on [%s], reason=[%s]\r\n", settingsFile, strerror(errno));
    }
    else {
        LOG("Settings updated successfully\r\n");
        _DumpSettings(pSettings);
    }

    return ret;
}

static void _DumpSettings(const PWRMgr_Settings_t *pSettings)
{
    LOG("PWRMGR-Settings-Mag: %X\r\n", pSettings->magic);
    LOG("PWRMGR-Settings-Ver: %d\r\n", pSettings->version);
    LOG("PWRMGR-Settings-Len: %d\r\n", pSettings->length );
    LOG("PWRMGR-Settings-PWR: %d\r\n", pSettings->powerState);
    LOG("PWRMGR-Settings-Brightness: %d\r\n", pSettings->ledSettings.brightness);
    #ifdef ENABLE_DEEP_SLEEP
        LOG("PWRMGR-Settings-Deep Sleep Timeout: %d\r\n", pSettings->deep_sleep_timeout);
    #endif 
}

static int _SetLEDStatus(IARM_Bus_PWRMgr_PowerState_t powerState)
{
    try {

        if( powerState != IARM_BUS_PWRMGR_POWERSTATE_ON )
        {
#ifdef FP_POWER_LED_ON_IN_LOW_POWER_MODE //For devices like TVs that are expected to have lights on when in one of the standby modes.
            device::FrontPanelIndicator::getInstance("Power").setState(true); 
            LOG("[PWRMgr-_SetLEDStatus] Settings the Power LED State to ON\r\n");
#else
            device::FrontPanelIndicator::getInstance("Power").setState(false); 
            LOG("[PWRMgr-_SetLEDStatus] Settings the Power LED State to OFF \r\n");
#endif
        }
        else
        {
            device::FrontPanelIndicator::getInstance("Power").setState(true);
            LOG("[PWRMgr-_SetLEDStatus] Settings the Power LED State to ON \r\n");
        }
    }
    catch (...) {
        LOG("Exception Caught during [PWRMgr - _SetLEDStatus]\r\n");
        return 0;
    }
    return 0;
}

static IARM_Bus_PWRMgr_PowerState_t _ConvertUIDevToIARMBusPowerState(UIDev_PowerState_t powerState)
{
    IARM_Bus_PWRMgr_PowerState_t ret = IARM_BUS_PWRMGR_POWERSTATE_ON;
    switch(powerState)
    {
        case UIDEV_POWERSTATE_OFF:
            ret = IARM_BUS_PWRMGR_POWERSTATE_OFF;
            break;
        case UIDEV_POWERSTATE_STANDBY:
            ret = IARM_BUS_PWRMGR_POWERSTATE_ON;
            break;
    }
    return ret;
}

int _SetAVPortsPowerState(IARM_Bus_PWRMgr_PowerState_t powerState)
{

    try {
        if( powerState != IARM_BUS_PWRMGR_POWERSTATE_ON ){
            if(IARM_BUS_PWRMGR_POWERSTATE_OFF != powerState)
            {
                //We're in one of the standby modes. Certain ports may have to be left on. 
                device::List<device::VideoOutputPort> videoPorts =  device::Host::getInstance().getVideoOutputPorts();
                for (size_t i = 0; i < videoPorts.size(); i++)
                {
                    bool doEnable = get_video_port_standby_setting(videoPorts.at(i).getName().c_str());
                    LOG("Video port %s will be %s in standby mode.\n", videoPorts.at(i).getName().c_str(), (doEnable? "enabled" : "disabled")); 
                    if(false == doEnable) 
                        videoPorts.at(i).disable();
                }
            }
            else
            {
                //Disable all ports when going into POWERSTATE_OFF
                device::List<device::VideoOutputPort> videoPorts =  device::Host::getInstance().getVideoOutputPorts();
                for (size_t i = 0; i < videoPorts.size(); i++)
                {
                    videoPorts.at(i).disable();
                }
            }

            device::List<device::AudioOutputPort> audioPorts =  device::Host::getInstance().getAudioOutputPorts();

            for (size_t i = 0; i < audioPorts.size(); i++)
            {
                audioPorts.at(i).disable();
            }
            /*LOG("Disabling   the AV Ports [_SetAVPortsPowerState]\r\n");*/
        }
        else
        {

            device::List<device::VideoOutputPort> videoPorts =  device::Host::getInstance().getVideoOutputPorts();
            for (size_t i = 0; i < videoPorts.size(); i++)
            {
                videoPorts.at(i).enable();
            }
            device::List<device::AudioOutputPort> audioPorts =  device::Host::getInstance().getAudioOutputPorts();
            for (size_t i = 0; i < audioPorts.size(); i++)
            {
                audioPorts.at(i).enable();
            }

            for (size_t i = 0; i < videoPorts.size(); i++)
            {
                device::VideoOutputPort &vPort = videoPorts.at(i);
                if (vPort.isDisplayConnected())
                {
                    device::AudioOutputPort &aPort = videoPorts.at(i).getAudioOutputPort();
                    LOG("Setting Audio Mode-(STBY- ACTIVE) by persistence to [%s]\r\n",aPort.getStereoMode(true).getName().c_str());
                    if(isEASInProgress == IARM_BUS_SYS_MODE_EAS)
                    {
                        /* Force Stereo in EAS mode. */
                        LOG("Force Stereo in EAS mode \r\n");
                        aPort.setStereoMode(device::AudioStereoMode::kStereo,false);
                    }
                    else
                    {
                        aPort.setStereoMode(aPort.getStereoMode(true).getName(),false);
                    } 
                }
            } 

            /*LOG("Enabling   the AV Ports [_SetAVPortsPowerState]\r\n");*/
        }

    }
    catch (...) {
        LOG("Exception Caught during [_SetAVPortsPowerState]\r\n");
        return 0;
    }
    return 0;
}



/**
 * @fn static IARM_Result_t _SysModeChange(void *arg){
 * @brief This function is a event handler which returns current system
 *  mode using IARM. It returns mode as  "NORMAL", "WAREHOUSE","EAS" or "UNKNOWN".
 *
 * @param[in] void pointer to void, containing IARM_Bus_CommonAPI_SysModeChange_Param_t data.
 *
 * @return variable of IARM_Result_t type.
 * @retval IARM_RESULT_SUCCESS On function completion.
 */
static IARM_Result_t _SysModeChange(void *arg)
{
    IARM_Bus_CommonAPI_SysModeChange_Param_t *param = (IARM_Bus_CommonAPI_SysModeChange_Param_t *)arg;

    __TIMESTAMP();printf("[PwrMgr] Recvd Sysmode Change::New mode --> %d,Old mode --> %d",param->newMode,param->oldMode);
         
    if (param->newMode == IARM_BUS_SYS_MODE_EAS) {
        isEASInProgress = IARM_BUS_SYS_MODE_EAS;
    }
    else if (param->newMode == IARM_BUS_SYS_MODE_NORMAL) {
        isEASInProgress = IARM_BUS_SYS_MODE_NORMAL; 
    }
    return IARM_RESULT_SUCCESS;
}


#ifdef _ENABLE_FP_KEY_SENSITIVITY_IMPROVEMENT
static void *_AsyncPowerTransition(void *) 
{
    PWRMgr_Settings_t *pSettings = &m_settings;

    while(1) {
        pthread_mutex_lock(&powerStateMutex);
        if (pSettings->powerState != targetState) {

            IARM_Bus_PWRMgr_SetPowerState_Param_t param;
            param.newState = targetState;
            _SetPowerState(&param);
        }
        else {
            /* goto sleep */
            pthread_cond_wait(&powerStateCond, &powerStateMutex);
        }
        pthread_mutex_unlock(&powerStateMutex);
    }
}

static void _SetPowerStateAsync(IARM_Bus_PWRMgr_PowerState_t curState, IARM_Bus_PWRMgr_PowerState_t newState)
{
    PWRMgr_Settings_t *pSettings = &m_settings;

    IARM_Result_t retCode = IARM_RESULT_SUCCESS;

    if (asyncPowerThreadId == NULL)
    {
        pthread_create(&asyncPowerThreadId, NULL, _AsyncPowerTransition, NULL);
    }
   
    /* Awake the thread to check status */
    if (curState != newState) {
    
        LOG("PowerState Fast transitioning from %d to %d, while still in [%d]\r\n", curState, newState, pSettings->powerState);

        _SetLEDStatus(newState);
        transitionState = newState;
        pthread_mutex_lock(&powerStateMutex);
        if (targetState != newState) {
            targetState = newState; 
            pthread_cond_signal(&powerStateCond);
        }
        pthread_mutex_unlock(&powerStateMutex);
    }
    else {
        LOG("Warning:PowerState is same as requested\r\n");
    }

    return;
}
#endif

#ifndef NO_RF4CE
#ifdef USE_UNIFIED_CONTROL_MGR_API_1
static void _ctrlmMessageHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
    if (eventId == CTRLM_RCU_IARM_EVENT_KEY_GHOST)
    {
        ctrlm_rcu_iarm_event_key_ghost_t* ghostEvent = (ctrlm_rcu_iarm_event_key_ghost_t*)data;
        if (ghostEvent != NULL)
        {
            if (ghostEvent->api_revision != CTRLM_RCU_IARM_BUS_API_REVISION)
            {
                LOG("CTRLM ghost key event: ERROR - Wrong CTRLM API revision - expected %d, event is %d!!",
                     CTRLM_RCU_IARM_BUS_API_REVISION, ghostEvent->api_revision);
                return;
            }
            LOG("CTRLM ghost code event: network_id: %d, network_type: %d, controller_id: %d, ghost_code: %d.\n",
                ghostEvent->network_id, ghostEvent->network_type, ghostEvent->controller_id, ghostEvent->ghost_code);
            // Decide whether or not to set the power state - either POWER ghost code sets the state to ON.
            if ((ghostEvent->ghost_code == CTRLM_RCU_GHOST_CODE_POWER_OFF) ||
                (ghostEvent->ghost_code == CTRLM_RCU_GHOST_CODE_POWER_ON))
            {
                IARM_Bus_PWRMgr_SetPowerState_Param_t param;
                param.newState = IARM_BUS_PWRMGR_POWERSTATE_ON;
                LOG("CTRLM Setting Powerstate to ON.\n");
                _SetPowerState((void *)&param);
            }
        }
        else
        {
            LOG("CTRLM ghost code event: ERROR: NULL event data!!\n");
        }
    }
    else
    {
        LOG("CTRLM event handler: ERROR: bad event type %d!!\n", eventId);
    }
}
#else
#ifdef RF4CE_GENMSO_API
static void _rfMessageHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{

                MSOBusAPI_Packet_t* busMsg;
                int statusCode;
                IARM_Bus_PWRMgr_SetPowerState_Param_t param;

                busMsg = (MSOBusAPI_Packet_t*)data;

                if(len != (busMsg->length + sizeof(MSOBusAPI_Packet_t) - sizeof(MSOBusAPI_Msg_t))) //Message size + header of packet
                {
                        LOG("BusTestApplication: %i MsgIndication with wrong length rec:%d exp:%d\n",eventId,
                                len, (busMsg->length + sizeof(MSOBusAPI_Packet_t) - sizeof(MSOBusAPI_Msg_t)));
                        return;
                }
                //LOG("BusTestApplication: Message received: id:%d\n", busMsg->msgId);
                switch(busMsg->msgId)
                {
                        case MSOBusAPI_MsgId_GhostCommand:
                        {
                                LOG("BusTestApplication: command code : id:%d\n", busMsg->msg.UserCommand.commandCode);
                                if (busMsg->msg.UserCommand.commandCode == 1 || busMsg->msg.UserCommand.commandCode == 2)
                {
                    param.newState = IARM_BUS_PWRMGR_POWERSTATE_ON;
                                        //LOG("Setting Powerstate to new=%d\r\n", param.newState);
                                        _SetPowerState((void *)&param);
                                }
                                break;
                        }
                         default:
                        {
                                //LOG("BusTestApplication: Message received: id:%d\n", busMsg->msgId);
                    break;
                        }
                }
        }

#elif defined(RF4CE_API)
static void _rf4ceMessageHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{

	rf4ce_Packet_t* busMsg;
	int statusCode;
	IARM_Bus_PWRMgr_SetPowerState_Param_t param; 
	
	busMsg = (rf4ce_Packet_t*)data;

	__TIMESTAMP();LOG("pwrMgr: RF4CE Msg indication event handler, msgId: 0x%X\n", (unsigned)busMsg->msgId);
	if (busMsg->msgId == rf4ce_MsgId_GhostCommand) {
	    __TIMESTAMP();LOG("pwrMgr: RF4CE Ghost Command, msgId: 0x%X\n", (unsigned)busMsg->msg.UserCommand.commandCode);
	}

	if(len != (busMsg->length + sizeof(rf4ce_Packet_t) - sizeof(rf4ce_Msg_t))) //Message size + header of packet
	{
		LOG("BusTestApplication: %i MsgIndication with wrong length rec:%d exp:%d\n",eventId,
			len, (busMsg->length + sizeof(rf4ce_Packet_t) - sizeof(rf4ce_Msg_t)));
		return;
	}
	//LOG("BusTestApplication: Message received: id:%d\n", busMsg->msgId);
	switch(busMsg->msgId)
	{
		case rf4ce_MsgId_GhostCommand:
		{
			LOG("BusTestApplication: command code : id:%d\n", busMsg->msg.UserCommand.commandCode);
			if (busMsg->msg.UserCommand.commandCode == 1 || busMsg->msg.UserCommand.commandCode == 2)
            {
                param.newState = IARM_BUS_PWRMGR_POWERSTATE_ON;
				//LOG("Setting Powerstate to new=%d\r\n", param.newState);
				_SetPowerState((void *)&param);
			}
			break;
		}
		 default:
		{
			//LOG("BusTestApplication: Message received: id:%d\n", busMsg->msgId);
            break;
		}
	}
}
#elif defined(RF4CE_GPMSO_API)
static void _gpMessageHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{

		gpMSOBusAPI_Packet_t* busMsg;
		int statusCode;
		IARM_Bus_PWRMgr_SetPowerState_Param_t param; 
		
		busMsg = (gpMSOBusAPI_Packet_t*)data;

		if(len != (busMsg->length + sizeof(gpMSOBusAPI_Packet_t) - sizeof(gpMSOBusAPI_Msg_t))) //Message size + header of packet
		{
			LOG("BusTestApplication: %i MsgIndication with wrong length rec:%d exp:%d\n",eventId,
				len, (busMsg->length + sizeof(gpMSOBusAPI_Packet_t) - sizeof(gpMSOBusAPI_Msg_t)));
			return;
		}
		//LOG("BusTestApplication: Message received: id:%d\n", busMsg->msgId);
		switch(busMsg->msgId)
		{
			case gpMSOBusAPI_MsgId_GhostCommand:
			{
				LOG("BusTestApplication: command code : id:%d\n", busMsg->msg.UserCommand.commandCode);
				if (busMsg->msg.UserCommand.commandCode == 1 || busMsg->msg.UserCommand.commandCode == 2)
                {
                    param.newState = IARM_BUS_PWRMGR_POWERSTATE_ON;
					//LOG("Setting Powerstate to new=%d\r\n", param.newState);
					_SetPowerState((void *)&param);
				}
				break;
			}
			 default:
			{
				//LOG("BusTestApplication: Message received: id:%d\n", busMsg->msgId);
	            break;
			}
		}
	}
#else
#warning "No RF4CE API defined"
#endif
#endif  /* USE_UNIFIED_CONTROL_MGR_API_1 */
#endif  /* NO_RF4CE */

static void _controlEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
    IARM_Bus_IRMgr_EventData_t *irEventData = (IARM_Bus_IRMgr_EventData_t*)data;
    IARM_Bus_PWRMgr_GetPowerState_Param_t powerState;
    int powerUp = 0;

    __TIMESTAMP();LOG("pwrMgr: Control event handler, key: 0x%X, src: 0x%X\n",
                      (unsigned)irEventData->data.irkey.keyCode, (unsigned)irEventData->data.irkey.keySrc);

    _GetPowerState((void *)&powerState);
    if (powerState.curState != IARM_BUS_PWRMGR_POWERSTATE_ON) {
        int keySrc = irEventData->data.irkey.keySrc;
        switch(irEventData->data.irkey.keyCode) {
        case KED_VOLUMEUP:
        case KED_VOLUMEDOWN:
        case KED_MUTE:
        case KED_INPUTKEY:
        case KED_TVPOWER:
            if ((keySrc == IARM_BUS_IRMGR_KEYSRC_RF) ||
                (keySrc == IARM_BUS_IRMGR_KEYSRC_IR)) {
                powerUp = 1;
            }
            break;

        case KED_PUSH_TO_TALK:
        case KED_VOLUME_OPTIMIZE:
            if (keySrc == IARM_BUS_IRMGR_KEYSRC_IR) {
                powerUp = 1;
            }
            break;
        }
        if (powerUp) {
            irEventData->data.irkey.keyCode = KED_POWER;
            irEventData->data.irkey.keyType = KET_KEYUP;
            _irEventHandler(owner,eventId,(void*)irEventData,len);
        }
    }
}

#ifndef NO_RF4CE
static void _speechEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
    IARM_Bus_PWRMgr_GetPowerState_Param_t powerState;
    _GetPowerState((void *)&powerState);
    __TIMESTAMP();LOG("pwrMgr: Voice event handler, eventId: %d.\n", (int)eventId);
#ifdef USE_UNIFIED_CONTROL_MGR_API_1
    if(powerState.curState != IARM_BUS_PWRMGR_POWERSTATE_ON &&
            (eventId == CTRLM_VOICE_IARM_EVENT_SESSION_BEGIN ||
             eventId == CTRLM_VOICE_IARM_EVENT_SESSION_ABORT ||
             eventId == CTRLM_VOICE_IARM_EVENT_SESSION_SHORT) )
#else
#ifdef USE_UNIFIED_RF4CE_MGR_API_4
    if(powerState.curState != IARM_BUS_PWRMGR_POWERSTATE_ON &&
            (eventId == VREX_MGR_IARM_EVENT_VOICE_BEGIN ||
             eventId == VREX_MGR_IARM_EVENT_VOICE_SESSION_ABORT ||
             eventId == VREX_MGR_IARM_EVENT_VOICE_SESSION_SHORT) )
#else
    _SPEECH_EVENT *sEvent;
    IARM_Bus_VREXMgr_EventData_t *vrexEventData = (IARM_Bus_VREXMgr_EventData_t*)data;
    sEvent = (_SPEECH_EVENT *)&vrexEventData->data.speechEvent;
    if(eventId == IARM_BUS_VREXMGR_EVENT_SPEECH && sEvent->type == IARM_BUS_VREXMGR_SPEECH_BEGIN && powerState.curState != IARM_BUS_PWRMGR_POWERSTATE_ON)
#endif /* USE_UNIFIED_RF4CE_MGR_API_4 */
#endif /* USE_UNIFIED_CONTROL_MGR_API_1 */
    {
        IARM_Bus_IRMgr_EventData_t irEventData;
        irEventData.data.irkey.keyCode = KED_POWER;
        irEventData.data.irkey.keyType = KET_KEYUP;
        _irEventHandler(owner,eventId,(void*)&irEventData,len);
    }
}
#endif /* NO_RF4CE */

#ifdef ENABLE_DEEP_SLEEP  
static IARM_Result_t _SetDeepSleepTimeOut(void *arg)
{
    IARM_Bus_PWRMgr_SetDeepSleepTimeOut_Param_t *param = (IARM_Bus_PWRMgr_SetDeepSleepTimeOut_Param_t *)arg;

    if(param != NULL)
    {  
        __TIMESTAMP();LOG("Deep sleep timeout set to : %d\r\n", param->timeout);
         deep_sleep_wakeup_timeout_sec = param->timeout;
         m_settings.deep_sleep_timeout = param->timeout;
         IsWakeupTimerSet = 1;
         _WriteSettings(m_settingsFile);  
        return IARM_RESULT_SUCCESS; 
    }
    return IARM_RESULT_IPCCORE_FAIL; 
}


/*  Wakeup the box after wakeup timeout for maintenance activities
    Reboot the box and entered to  light sleep.
*/
static gboolean deep_sleep_wakeup_fn(gpointer data)
{
    uint32_t timeout = (uint32_t) difftime(time(NULL),timeAtDeepSleep);

    __TIMESTAMP();LOG("Sec Elapsed Since Deep Sleep : %d \r\n",timeout);
        
    if(timeout >= deep_sleep_wakeup_timeout_sec)
    {
        IARM_BUS_PWRMgr_DeepSleepTimeout_EventData_t param;
        param.timeout = deep_sleep_wakeup_timeout_sec;
        IARM_Bus_BroadcastEvent( IARM_BUS_PWRMGR_NAME,
        IARM_BUS_PWRMGR_EVENT_DEEPSLEEP_TIMEOUT, (void *)&param, sizeof(param));
#if !defined (_DISABLE_SCHD_REBOOT_AT_DEEPSLEEP)
        __TIMESTAMP();LOG("Reboot the box due to Deep Sleep Timer Expiry : %d \r\n", param.timeout);
#else
        /*Scheduled maintanace reboot is disabled for XiOne/Llama/Platco. Instead state will change to LIGHT_SLEEP*/
        IARM_Bus_PWRMgr_SetPowerState_Param_t paramSetPwr;
        PWRMgr_Settings_t *pSettings = &m_settings;
        __TIMESTAMP();LOG("deep_sleep_wakeup_fn: Set Device to light sleep on Deep Sleep timer expiry..\r\n");
        pSettings->powerState = IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP;
        paramSetPwr.newState = IARM_BUS_PWRMGR_POWERSTATE_STANDBY_LIGHT_SLEEP;
        _SetPowerState((void *)&paramSetPwr);
#endif /*End of _DISABLE_SCHD_REBOOT_AT_DEEPSLEEP*/
        return FALSE;
    }
return TRUE;
}


/*  Put the box to Deep Sleep after timeout (configured for now + 1 hour)
    if box had restarted in deep sleep mode 
*/
static gboolean invoke_deep_sleep_on_bootup_timeout(gpointer data)
{
    IARM_Bus_PWRMgr_SetPowerState_Param_t param;
    PWRMgr_Settings_t *pSettings = &m_settings;

    __TIMESTAMP();LOG("deep_sleep_thread : Set Device to Deep Sleep on Bootip Timer Expiry.. \r\n");       

    /* Change the current state to standby and new state to Deep Sleep */
    pSettings->powerState = IARM_BUS_PWRMGR_POWERSTATE_STANDBY;
    param.newState = IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP;
    _SetPowerState((void *)&param);
    return FALSE;
}


/*  Get TZ diff
    Added For wakeup Time calculation based on TZ
*/
// enum with Time Zone hours 
typedef enum _tzValue {
            tzHST11=11,
            tzHST11HDT=10,
            tzAKST=9,
            tzAKST09AKDT=8,
            tzPST08=8,
            tzPST08PDT=8,
            tzMST07=7,
            tzMST07MDT=6,
            tzCST06=6,
            tzCST06CDT=5,
            tzEST05=5,
            tzEST05EDT=4
}tzValue;
// Map to associate the Time Zone strings with TZ hours
#include <map>
static std::map<std::string, tzValue> _maptzValues;
static void InitializeTimeZone();
static uint32_t getTZDiffInSec()
{
    uint32_t _TZDiffTime = 6*3600;
    IARM_Result_t iResult = IARM_RESULT_SUCCESS;
    tzValue value=tzCST06;

    /* Initialize the Time Zone */
    InitializeTimeZone();    

    /* Get the Time Zone Pay Load from SysMgr */
    IARM_Bus_SYSMgr_GetSystemStates_Param_t param;
    iResult = IARM_Bus_Call(IARM_BUS_SYSMGR_NAME,IARM_BUS_SYSMGR_API_GetSystemStates,(void *)&param,sizeof(param));
    if(iResult == IARM_RESULT_SUCCESS)
    {
        if (param.time_zone_available.error)
        {
            __TIMESTAMP();LOG("Failed to get the Time Zone Information from SysMgr \r\n");
        }
        else if (param.time_zone_available.state == 2)
        {
          if (strlen(param.time_zone_available.payload) > 1)
            {
                __TIMESTAMP();LOG("TZ Payload - %s",param.time_zone_available.payload);
                value  = _maptzValues[param.time_zone_available.payload];
                _TZDiffTime = value * 3600;   

                 __TIMESTAMP();LOG("TZ value = %d\r\n",value);
                __TIMESTAMP();LOG("Time Zone in Sec = %d \r\n",_TZDiffTime);
            }
        }
    }
    return _TZDiffTime;
}

/*  Get TZ diff
    Have Record of All US TZ as of now.
*/
static void InitializeTimeZone()
{
  _maptzValues["HST11"] = tzHST11;
  _maptzValues["HST11HDT,M3.2.0,M11.1.0"] = tzHST11HDT;
  _maptzValues["AKST"] = tzAKST;
  _maptzValues["AKST09AKDT,M3.2.0,M11.1.0"] = tzAKST09AKDT;
  _maptzValues["PST08"] = tzPST08;
  _maptzValues["PST08PDT,M3.2.0,M11.1.0"] = tzPST08PDT;
  _maptzValues["MST07"] = tzMST07;
  _maptzValues["MST07MDT,M3.2.0,M11.1.0"] = tzMST07MDT;
  _maptzValues["CST06"] = tzCST06;
  _maptzValues["CST06CDT,M3.2.0,M11.1.0"] = tzCST06CDT;
  _maptzValues["EST05"] = tzEST05;
  _maptzValues["EST05EDT,M3.2.0,M11.1.0"] = tzEST05EDT;
  //printf("_maptzValues contains %ld items \r\n",_maptzValues.size());
}

/*  Get Wakeup timeout. 
    Wakeup the box to do Maintenance related activities.
*/

static uint32_t getWakeupTime()
{
    time_t now,wakeup;
    struct tm wakeupTime;
    uint32_t wakeupTimeInSec = 0,getTZDiffTime = 0;
    uint32_t wakeupTimeInMin = 5;
    FILE *fpWT = NULL;

     /* Read the wakeup Time in Seconds from /tmp override  
        else calculate the Wakeup time til 2AM */
    fpWT = fopen("/tmp/deepSleepWakeupTimer","r");
    if (NULL != fpWT)
    {
        if(0 > fscanf(fpWT,"%d",&wakeupTimeInMin))
        {
            __TIMESTAMP();LOG("Error: fscanf on wakeupTimeInSec failed");
        }
        else
        {
            wakeupTimeInSec = wakeupTimeInMin * 60 ;
            fclose (fpWT);
            __TIMESTAMP();LOG(" /tmp/ override Deep Sleep Wakeup Time is %d \r\n",wakeupTimeInSec);
            return wakeupTimeInSec;
        }
        fclose (fpWT);
    }
   
    /* curr time */
    time (&now);

    /* wakeup time */
    time (&wakeup);
    wakeupTime = *localtime (&wakeup);
    
    if (wakeupTime.tm_hour >=0 && wakeupTime.tm_hour < 2)
    {
        /*Calculate the wakeup time till 2 AM..*/
        wakeupTime.tm_hour = 2;
        wakeupTime.tm_min = 0;
        wakeupTime.tm_sec = 0;
        wakeupTimeInSec = difftime(mktime(&wakeupTime),now);
    
    }   
    else
    {
        /*Calculate the wakeup time till midnight + 2 hours for 2 AM..*/
        wakeupTime.tm_hour = 23;
        wakeupTime.tm_min = 59;
        wakeupTime.tm_sec = 60;
        wakeupTimeInSec = difftime(mktime(&wakeupTime),now);
        wakeupTimeInSec = wakeupTimeInSec + 7200; // 7200sec for 2 hours
    }   
        
        /* Add randomness to calculated value i.e between 2AM - 3AM 
            for 1 hour window 
        */    
        srand(time(NULL)); 
        uint32_t randTimeInSec = (uint32_t)rand()%(3600) + 0; // for 1 hour window
        wakeupTimeInSec  = wakeupTimeInSec + randTimeInSec;
        //printf ("randTimeInSec is  : %d sec \r\n", randTimeInSec);

    __TIMESTAMP();LOG("Calculated Deep Sleep Wakeup Time Before TZ setting is %d Sec \r\n", wakeupTimeInSec);    
        getTZDiffTime = getTZDiffInSec();
        wakeupTimeInSec = wakeupTimeInSec + getTZDiffTime;
    __TIMESTAMP();LOG("Calculated Deep Sleep Wakeup Time After TZ setting is %d Sec \r\n", wakeupTimeInSec);

  return wakeupTimeInSec;
}

#endif
static IARM_Result_t _SetNetworkStandbyMode(void *arg)
{
    IARM_Bus_PWRMgr_NetworkStandbyMode_Param_t *param = (IARM_Bus_PWRMgr_NetworkStandbyMode_Param_t *)arg;
    uint32_t uiTimeout = 3; /*Timeout in seconds*/
    if(param != NULL)
    {
#ifdef ENABLE_LLAMA_PLATCO_SKY_XIONE
        LOG("Setting network standbyMode: %s \r\n", param->bStandbyMode?("Enabled"):("Disabled"));
        nwStandbyMode_gs = param->bStandbyMode;
        m_settings.nwStandbyMode = param->bStandbyMode;
        _WriteSettings(m_settingsFile);
#else
        LOG ("\nError _SetNetworkStandbyMode not implemented. standbyMode: %s", param->bStandbyMode?("Enabled"):("Disabled"));
#endif
        return IARM_RESULT_SUCCESS;
    }
    return IARM_RESULT_IPCCORE_FAIL;
}

static IARM_Result_t _GetNetworkStandbyMode(void *arg)
{
    IARM_Bus_PWRMgr_NetworkStandbyMode_Param_t *param = (IARM_Bus_PWRMgr_NetworkStandbyMode_Param_t *)arg;

    if(param != NULL)
    {
#ifdef ENABLE_LLAMA_PLATCO_SKY_XIONE
        param->bStandbyMode = m_settings.nwStandbyMode;
        LOG("Network standbyMode is: %s \r\n", param->bStandbyMode?("Enabled"):("Disabled"));
#else
        LOG ("\nError _GetNetworkStandbyMode not implemented.");
#endif
        return IARM_RESULT_SUCCESS;
    }
    return IARM_RESULT_IPCCORE_FAIL;
}


/** @} */
/** @} */
