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
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sys/time.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <secure_wrapper.h>
#include <thread>

#include "comcastIrKeyCodes.h"
#include "libIBus.h"
#include "pwrMgr.h"
#include "plat_power.h"
#include "libIBusDaemon.h"
#include "resetModes.h"
#include "frontPanelIndicator.hpp"
#include "exception.hpp"
#include "manager.hpp"
#include "iarmUtil.h"
#include "pwrlogger.h"
#include "frontPanelConfig.hpp"
#include "pwrMgrInternal.h"
  
static pthread_mutex_t tMutexLock;
static pthread_cond_t  tMutexCond;
static long gKeyPressedTime = 0;
static long gKeyReleasedTime = 0;
static long gCurrentTime = 0;
static int gTimeOutValue = 0;
static bool inResetMode = false;
static bool gResetMonitoredOnIBus = false;
static unsigned int gLastResetEvent = 0;
static bool gInitialized = false;

static bool isRIFocus = false;
static bool isXREFocus = false;
static bool grabedFocus = false;
static int modeIndexStatus = 0;


/*Global variable which shows the wait state. Waiting for LED blink. Should not enter any key */
static bool gWaitState = false; 
/*Global variable to inform the timer that the wait state got interrupted by key press*/
static bool gResetTimer = true;
/*A single key entry has to be checked for all reset modes. This global variable stores the 
information whether the key is pressed only once but used for different reset modes or a new key*/
static bool gIsSameKey = false;
static bool gKeyAllowed[] = {true, true, true, true};

/* Reset key sequence is taken cared by a statemachine.
The Key sequence can vary between different boxes.*/

/*Structure to represent the values defining the state
of the state machine*/
typedef struct keyType
{
   int Key;          // Key Value for the specific state
   int keyMode;      // Key Type (Up/Down) for the specific state
   int waitDuration; // Time to wait for after a particular state
   int validTime;    // Duration with in which the next set of keys need to be entered
}keyType_t;

/*Structure to pass the time values to the timer thread*/
typedef struct timerStruct
{
    int waitFor;     // Wait till leds blink before entering next key
    int validFor;    // Enter the next keys in the sequence within this valid time after the leds blink.
}timerStruct_t;

/*A mapping to the Key Values available in comcastIrKeyCodes.h*/
typedef enum keyValue
{
   KEY_NULL=0,
   KEY_POWER=KED_POWER,
   KEY_OK=KED_OK,
   KEY_RIGHT=KED_ARROWRIGHT,
   KEY_LEFT=KED_ARROWLEFT,
   KEY_MENU=KED_MENU,
   KEY_UP=KED_ARROWUP,
   KEY_DOWN=KED_ARROWDOWN
}keyValue_t;

/* Key Variants mapping.
 * In different states remote can send different key codes for the same keys.
 * this structure for variants definition to to translate variants to defined in reset sequence keys  */
typedef struct keyVariant
{
   int Key;       // Key Value for the specific state
   int Variant;   // Other key code can be generated
}keyVariant_t;

/*State of the reset statemachine*/
typedef enum state
{
    INIT = 0,
    PROCESS = 1,
    ENDED = 2
}state_t;

/*Structure which stores the current state of the statemachine */
typedef struct stateMachine
{
   state_t state;              /*Currently not used*/
   int expectedKeyIndex;       /*Points to the expected key set for the current state*/
   int (*resetFuction) (void); /*Function pointer to point to the reset function for the specific reset mode*/
}stateMachine_t;

/*We have 4 statemachines each for a specific reset mode.
All statemachines are initialized to index 0 and ENDED state*/
static stateMachine_t coldFactory = {ENDED, 0, processColdFactoryReset};
static stateMachine_t factory = {ENDED, 0, processFactoryReset};
static stateMachine_t wareHouse = {ENDED, 0, processWareHouseReset};
static stateMachine_t customer = {ENDED, 0, processCustomerReset};
static stateMachine_t personality = {ENDED, 0, processPersonalityReset};

/*An array of the statemachines currently supported. The array is introduced to enable
traversing through the specific statemachine from a single function using different
array index - resetmode*/
stateMachine_t * stateMachineList[] = {&coldFactory, &factory, &wareHouse, &customer, &personality};

/*Enumeration to map a resetmode to a number */
typedef enum resetType
{
    COLD_FACTORY = 0,
    FACTORY = 1,
    WARE_HOUSE = 2,
    CUSTOMER = 3,
    PERSONALITY = 4
}resetType_t;

static const char *resetString [5] = {"CLDINIT", "FR RST", "WH RST", "CUST", "PERS"};

/*Structure to store the expected sequence for a specific reset type*/
typedef struct KeyMap
{
    resetType_t reset;
    keyType_t *pData;
}KeyMap_t;

/*Time values in milliseconds*/
#define INITIAL_TIME_OUT_VALUE 10000
#define MAX_RESET_MODE_COUNT 5
  
#define KEY_COMBINATION_TIMEOUT 2000
#define POWER_KEY_HOLD_TIME_FOR_REBOOT 10000
#define POWER_KEY_HOLD_TIME_FOR_RESET 2000
#define POWER_KEY_HOLD_TIME_FOR_FACTORY_RESET 30000
  
  
/* Refer  https://www.teamccp.com/confluence/display/CE/Resets.
  Use Same Reset Sequence for all devices. 
*/
static keyType_t coldFactorySeq[] = {{KEY_POWER, KET_KEYUP, 0, 0}, {KEY_OK, KET_KEYUP, 0, 2000}, \
                              {KEY_RIGHT, KET_KEYUP, 0, 2000}, {KEY_MENU, KET_KEYUP, 0, 2000}, \
                              {KEY_RIGHT, KET_KEYUP, 0, 2000}, {KEY_POWER, KET_KEYUP, 0, 0}, \
                              {KEY_NULL, 0, 0, 0}};

static keyType_t factorySeq[] = {{KEY_POWER, KET_KEYUP, 0, 0}, {KEY_OK, KET_KEYUP, 0, 2000}, \
                          {KEY_RIGHT, KET_KEYUP, 0, 2000}, {KEY_MENU, KET_KEYUP, 0, 2000}, \
                          {KEY_LEFT, KET_KEYUP, 0, 2000}, {KEY_POWER, KET_KEYUP, 0, 0}, \
                          {KEY_NULL, 0, 0, 0}};

static keyType_t wareHouseSeq[] = {{KEY_POWER, KET_KEYUP, 0, 0}, {KEY_OK, KET_KEYUP, 0, 2000}, \
                            {KEY_RIGHT, KET_KEYUP, 0, 2000}, {KEY_DOWN, KET_KEYUP, 0, 2000}, \
                            {KEY_POWER, KET_KEYUP, 0, 0}, {KEY_NULL, 0, 0, 0}};

/*customer reset sequence unknown*/
static keyType_t customerSeq[] = {{KEY_POWER, KET_KEYUP, 0, 0}, {KEY_OK, KET_KEYUP, 0, 2000},  \
                           {KED_UNDEFINEDKEY, KET_KEYUP, 0, 0}, {KEY_NULL, 0, 0, 0}}; 

keyType_t personalityResetSeq[] = {{KEY_POWER, KET_KEYUP, 0, 0}, {KEY_OK, KET_KEYUP, 0, 2000}, \
                            {KEY_LEFT, KET_KEYUP, 0, 2000}, {KEY_DOWN, KET_KEYUP, 0, 2000}, \
                            {KEY_POWER, KET_KEYUP, 0, 0}, {KEY_NULL, 0, 0, 0}};

/* handling variants */
static const keyVariant_t keyVariants[] = {{ KED_POWER, KED_RF_POWER}, {KED_OK, KED_SELECT}};

/*Reset Rules Array - which points to the Key sequences for each reset modes */
static KeyMap_t Reset_Rules[MAX_RESET_MODE_COUNT] = {{COLD_FACTORY, &coldFactorySeq[0]},{FACTORY, &factorySeq[0]},{WARE_HOUSE, &wareHouseSeq[0]},{CUSTOMER, &customerSeq[0]}, \
    {PERSONALITY, &personalityResetSeq[0]}};


static IARM_Bus_PWRMgr_PowerState_t curPwrState = IARM_BUS_PWRMGR_POWERSTATE_STANDBY;

static void storeFocus()
{
    isXREFocus = true;
}

static void getSystemTime(long *syst)
{
    struct timespec Stime;
    clock_gettime(CLOCK_MONOTONIC, &Stime);
    *syst = ((Stime.tv_sec * 1000) + (Stime.tv_nsec / 1000000)); //gets in milliseconds	
  
}

static void releaseFocus()
{
    __TIMESTAMP();LOG("Reset: IARM-Release MEDIA Focus for RI Process\n");
    IARM_BusDaemon_ReleaseOwnership(IARM_BUS_RESOURCE_FOCUS);     
    grabedFocus = false;
    isXREFocus = false;
    isRIFocus = false;
  
}

static void grabFocus()
{
  
    __TIMESTAMP();LOG("Reset: IARM-Request MEDIA Focus for Reset Process\n");
    IARM_BusDaemon_RequestOwnership(IARM_BUS_RESOURCE_FOCUS);    
    return;
  
}
static void ResetResetState(void)
{
    int i;
    for( i = 0; i < MAX_RESET_MODE_COUNT; i++ )
    {
        stateMachineList[i]->expectedKeyIndex = 0;
    }
  
    inResetMode = false;
    gKeyPressedTime = 0;
    gKeyReleasedTime = 0;
    gCurrentTime = 0;
    gTimeOutValue = 0;
    if (grabedFocus)
    {
        releaseFocus();
    }
  
    /*There could be other entities (like ledmgr) monitoring progress of reset sequence.
     * Let them know that the sequence failed.*/
    if(gResetMonitoredOnIBus)
    {
        IARM_Bus_PWRMgr_EventData_t eventData;
        eventData.data.reset_sequence_progress = -1; 
        IARM_Bus_BroadcastEvent(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_EVENT_RESET_SEQUENCE, 
                (void *)&eventData, sizeof(eventData));
        gResetMonitoredOnIBus = false;
		gLastResetEvent = 0;
    }
}

void setResetPowerState(IARM_Bus_PWRMgr_PowerState_t newPwrState)
{
   /* LOG("Reset: Saving current state %d\r\n", newPwrState);*/
    curPwrState = newPwrState;
}

void blinkLEDs(void)
{
    /*Blink functionality is not supported in boxes.. 
      Simulate blink by setting the LED stae to ON/OFF */
  
    __TIMESTAMP();LOG("Reset: Blinking LEDs\n");
    fflush(stdout);
    
    try{
      
        device::List <device::FrontPanelIndicator> fpIndicators = device::FrontPanelConfig::getInstance().getIndicators();
        
        for (uint i = 0; i < fpIndicators.size(); i++)
        {
            //__TIMESTAMP();LOG("BLINK %s LED : ",fpIndicators.at(i).getName().c_str());
            device::FrontPanelIndicator::getInstance(fpIndicators.at(i).getName()).setState(false);
        }
       
        sleep(1);
      
        for (uint i = 0; i < fpIndicators.size(); i++)
        {
            device::FrontPanelIndicator::getInstance(fpIndicators.at(i).getName()).setState(true);
        }
      
        sleep(1);
      
        for (uint i = 0; i < fpIndicators.size(); i++)
        {
            device::FrontPanelIndicator::getInstance(fpIndicators.at(i).getName()).setState(false);
        }
        
    }
    catch(device::Exception &e)
    {
        __TIMESTAMP();LOG("An exception caught during FP LED Blink Operation: %s",e.getMessage().c_str());
    }
    return;
}

int checkResetModeSequence(int keyType, int keyCode, resetType_t resetMode)
{
    keyType_t *pCurrentState;
    keyType_t *pNextState;
    int ret = 0;
    static timerStruct_t timerVariable;
  
   //__TIMESTAMP(); LOG("Reset: Enter checkResetModeSequence");
    
    /*Get the current state's expectation for the reset mode */
    pCurrentState = Reset_Rules[resetMode].pData + stateMachineList[resetMode]->expectedKeyIndex;
  
    if ( (true == gWaitState) && (true != gIsSameKey))
    {
       /* Received a key during wait state - so reset the timer */
       gResetTimer = true;
       /* Signal the thread to unblock */
       pthread_mutex_lock(&tMutexLock);
       pthread_cond_signal(&tMutexCond);
       pthread_mutex_unlock(&tMutexLock);
      
       ResetResetState();
       return 0;
    }
  
    //__TIMESTAMP(); LOG("Reset: checkResetModeSequence %d - %d",pCurrentState->keyMode,pCurrentState->Key);
   // __TIMESTAMP(); LOG("Reset: checkResetModeSequence passed %d - %d",keyType,keyCode);
    /* Check Key Code for the specific state machine */
    if( (pCurrentState->keyMode == keyType) && (pCurrentState->Key == keyCode)/* && (true == gKeyAllowed[resetMode]) */)
    {
        if(0 <  stateMachineList[resetMode]->expectedKeyIndex)
        {
            /*Power and OK keys detected. From here on, all keypresses generate appropriate IARM events*/
            if(gLastResetEvent != stateMachineList[resetMode]->expectedKeyIndex)
            {
                gResetMonitoredOnIBus = true;
                IARM_Bus_PWRMgr_EventData_t eventData;
                eventData.data.reset_sequence_progress = stateMachineList[resetMode]->expectedKeyIndex;
                IARM_Bus_BroadcastEvent(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_EVENT_RESET_SEQUENCE,
                        (void *)&eventData, sizeof(eventData));
                __TIMESTAMP();LOG("\n Reset: sending IARM event %d \n", stateMachineList[resetMode]->expectedKeyIndex);
                gLastResetEvent = stateMachineList[resetMode]->expectedKeyIndex;
            }
        }
       /* Set the next expected KeyCode and KeyType for the state machine*/
       stateMachineList[resetMode]->expectedKeyIndex++;
       //__TIMESTAMP();LOG("Reset: Set the next expected KeyCode and KeyType %02x",keyCode);
      
       /*If the next state is KEY_NULL, we have traversed through the correct key sequence. Can call the 
       appropriate reset function*/
       pNextState = pCurrentState + 1;
       if ( KEY_NULL == (pNextState)->Key)
       {
           gResetTimer = true;
           stateMachineList[resetMode]->expectedKeyIndex = 0;
         __TIMESTAMP();LOG("\n Reset: Action took place for: %d \n",resetMode);
           
           fflush(stdout);
           try {
               device::FrontPanelTextDisplay::Scroll scroll(40,10,0);
               device::FrontPanelConfig::getInstance().getTextDisplay("Text").setText(resetString[resetMode]);
               device::FrontPanelConfig::getInstance().getTextDisplay("Text").setScroll(scroll);
           }
           catch(device::Exception &e)
           {
               printf("An exception caught while setting fp text : %s",e.getMessage().c_str());
           }
           
           ret = stateMachineList[resetMode]->resetFuction();
           ResetResetState();
       }
              
       if(0 != pCurrentState->validTime)
       {
            /*We need to  wait for 2 more seconds to get the
            next key. If the user fail to provide the key within the expected
            time period, all the reset statemachines are set to beginning*/
            gCurrentTime=0;
            /*Capture the current time to check for that timeout*/
            getSystemTime(&gCurrentTime);
            gTimeOutValue = pCurrentState->validTime;
            __TIMESTAMP();LOG("Reset: gTimeOutValue: %d- %d ",gTimeOutValue,gCurrentTime);
       } 
       
      
    }
    else
    {  
         /*Reset statemachine to the initial state*/
          stateMachineList[resetMode]->expectedKeyIndex = 0;
        //__TIMESTAMP();LOG("\n Reset: Reset statemachine to the initial state for Reset Mode %d: \n",resetMode);
        
    }
    return 0;
}

int checkResetSequence(int keyType, int keyCode)
{
    int ret = 0;
    int i = 0;
    long tCurTime = 0;
  
    //__TIMESTAMP();LOG("Reset: Processing Key %02x",keyCode);
  
    /* Check Key Code variant for the specific state machine */
    for (const keyVariant_t* v = keyVariants; v < keyVariants + sizeof(keyVariants)/sizeof(keyVariant_t); ++v)
    {
    	if (keyCode == v->Variant)
    	{
            keyCode = v->Key;
            //__TIMESTAMP();LOG("Reset: Key %02x variant was redefined as %02x for further processing " ,v->Variant, v->Key);
            break;
    	}
    }
    //__TIMESTAMP();LOG("checkResetSequence : keyCode = %d keyType = %d \n",keyCode,keyType);
    if (0 != gCurrentTime)
    {
        gKeyPressedTime=0;
        getSystemTime(&gKeyPressedTime);
        //__TIMESTAMP(); LOG("\n Reset: gKeyPressedTime  - 1 %ld - %ld \n", gKeyPressedTime,gCurrentTime);
        if((gKeyPressedTime - gCurrentTime) > gTimeOutValue)
        {
            __TIMESTAMP(); LOG("\n Reset: Sorry you crossed the time out value %d \n", gTimeOutValue);
            fflush(stdout);
            ResetResetState();
        }
    }
  
    switch (keyType)
    {
        case KET_KEYDOWN:
        {
            
              //__TIMESTAMP();LOG("Reset : KEY PRESSED keyCode = %d -%d\n",keyCode,KET_KEYDOWN );
                fflush(stdout);
                if (!inResetMode && ((keyCode == KED_POWER) || (keyCode == KED_RF_POWER) || (keyCode == KED_FACTORY)))
                {
                    gKeyPressedTime=0;
                    getSystemTime(&gKeyPressedTime);
                }
          
        }                             
        break;
        case KET_KEYUP:
        {
            //__TIMESTAMP();LOG("Reset : KEY RELEASED keyCode = %d -%d\n",keyCode,KET_KEYUP);
            fflush(stdout);
            if (!inResetMode && ((keyCode == KED_POWER) || (keyCode == KED_RF_POWER) || (keyCode == KED_FACTORY)))
            {
                gKeyReleasedTime=0;
                getSystemTime(&gKeyReleasedTime);
                if(gKeyPressedTime == 0)
                {
                    ResetResetState();//if start time is zero make end also zero. Found a issue with (Downkey event not comes and starttime =0 and endtime >0 TODO
                }
                if ((keyCode == KED_POWER) || (keyCode == KED_RF_POWER))
                {
#ifdef ENABLE_FACTORY_RESET_VIA_FP_POWER_BUTTON
                    if((gKeyReleasedTime - gKeyPressedTime) >= POWER_KEY_HOLD_TIME_FOR_FACTORY_RESET)
                    {
                        __TIMESTAMP();LOG("\n Reset: User Factory reset through long-pressing power key\n");
                        fflush(stdout);
                        processUserFactoryReset();
                        ret = 1;
                        return ret;
                    }
#else
                    if ((gKeyReleasedTime - gKeyPressedTime) > POWER_KEY_HOLD_TIME_FOR_REBOOT)
                    {
                        __TIMESTAMP();LOG("\n Reset: Inside reset mode 1\n");
                        fflush(stdout);
                        PwrMgr_Reset(curPwrState, true);

                        ret = 1; //mark the return value as triggered for reset.
                        return ret;
                    }
#endif
                    else if ((gKeyReleasedTime - gKeyPressedTime) >= POWER_KEY_HOLD_TIME_FOR_RESET)
                    {
                        __TIMESTAMP();LOG("\n Reset: Inside reset mode 2\n");
                        fflush(stdout);
                        ResetResetState();
                        inResetMode = true;
                    
                        gCurrentTime=0;
                        /*Capture the current time to check for that timeout*/
                        getSystemTime(&gCurrentTime);
                        gTimeOutValue = INITIAL_TIME_OUT_VALUE;
                        if (!grabedFocus)
                        {
                            /*storeFocus()*/;
                            grabFocus();
                            grabedFocus = true;
                        }
                        for( i = 0; i < MAX_RESET_MODE_COUNT; i++ )
                        {
                            stateMachineList[i]->expectedKeyIndex++;
                        }
                        __TIMESTAMP();LOG("\n Reset: FP Power key is pressed for 2 secs");
                        fflush(stdout);
                        ret = 1; //mark the return value as triggered for reset.
                        return ret;
                    }
                    else
                    {
                        ResetResetState();
                    }
                }
                else
                {
                    ResetResetState();
                }
                
                
            }
            else
            {
                if(inResetMode)
                {
                    __TIMESTAMP();LOG("\n Reset: Already in reset mode  \n");
					fflush(stdout);                    
					resetType_t resetMode = COLD_FACTORY;
                    checkResetModeSequence(keyType, keyCode, resetMode);
                    gIsSameKey = true;
                    resetMode = FACTORY;
                    checkResetModeSequence(keyType, keyCode, resetMode);
#ifndef _DISABLE_RESET_SEQUENCE
                    resetMode = WARE_HOUSE;
                    checkResetModeSequence(keyType, keyCode, resetMode);
#else
                    static bool indicatedDisable = false;
                    if (!indicatedDisable) {
                        indicatedDisable = true;
                        LOG("Reset sequence is disabled for Warehouse reset");
                    }
#endif
                    resetMode = CUSTOMER;
                    checkResetModeSequence(keyType, keyCode, resetMode);
                    resetMode = PERSONALITY;
                    checkResetModeSequence(keyType, keyCode, resetMode);
                    gIsSameKey = false;
                }
            }
        }
        default:
            /*Ignore Repeat Key type*/
        break;
    }
    if( inResetMode )
    {
        if(( 0 == stateMachineList[0]->expectedKeyIndex) && (0 == stateMachineList[1]->expectedKeyIndex) && (0 == stateMachineList[2]->expectedKeyIndex) && 
            (0 == stateMachineList[3]->expectedKeyIndex) && (0 == stateMachineList[4]->expectedKeyIndex))
        {
            __TIMESTAMP();LOG("\n Reset: All statemachines in initial state. So reset the state machines\n");
            ResetResetState();
        }
    }
    return 0;
}

IARM_Result_t initReset()
{
    if(!gInitialized)
    {
        gInitialized = true;
        pthread_mutex_init (&tMutexLock, NULL);
        pthread_cond_init (&tMutexCond, NULL);
    }
    return IARM_RESULT_SUCCESS;
}
/*Uninitialize the reset app from power manager Stop.
Currently this interface is not being used*/
IARM_Result_t unInitReset()
{
    if(gInitialized)
    {
        gInitialized = false;
        pthread_mutex_unlock(&tMutexLock);
        pthread_mutex_destroy(&tMutexLock);
        pthread_cond_destroy(&tMutexCond);
    }
    return IARM_RESULT_SUCCESS;
}

void PwrMgr_Reset(IARM_Bus_PWRMgr_PowerState_t newState, bool isFPKeyPress)
{
    LOG("\n PWR_Reset: Rebooting box now .....\r\n");
    if (newState == IARM_BUS_PWRMGR_POWERSTATE_ON) system("sh /togglePower");

    if(!isFPKeyPress)
    {
        performReboot("PowerMgr_Powerreset", nullptr, "Rebooting the box due to ECM connectivity Loss...");
    }
    else
    {
        performReboot("PowerMgr_Powerreset", nullptr, "Rebooting the box due to Frontpanel power key pressed for 10 sec...");
    }
}

inline static void check_payload(const char ** input, const char * default_arg)
{
    if((NULL == *input) || (0 == (*input)[0]))
    {
        *input = default_arg;
    }
    return;
}

void performReboot(const char * requestor, const char * reboot_reason_custom, const char * reboot_reason_other)
{
    LOG("performReboot: Rebooting box now. Requestor: %s. Reboot reason: %s\n", requestor, reboot_reason_custom);
    const char * default_arg = "unknown";

    check_payload(&requestor, default_arg);
    check_payload(&reboot_reason_custom, default_arg);
    check_payload(&reboot_reason_other, default_arg);
    
    IARM_Bus_PWRMgr_RebootParam_t eventData;
    //Note: assumes caller has checked arg reboot_reason_custom and is safe to use.
    strncpy(eventData.reboot_reason_custom, reboot_reason_custom, sizeof(eventData.reboot_reason_custom));
    strncpy(eventData.reboot_reason_other, reboot_reason_other, sizeof(eventData.reboot_reason_other));
    strncpy(eventData.requestor, requestor, sizeof(eventData.requestor));
    eventData.reboot_reason_custom[sizeof(eventData.reboot_reason_custom) - 1] = '\0';
    eventData.reboot_reason_other[sizeof(eventData.reboot_reason_other) - 1] = '\0';
    eventData.requestor[sizeof(eventData.requestor) - 1] = '\0';
    IARM_Bus_BroadcastEvent( IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_EVENT_REBOOTING, (void *)&eventData, sizeof(eventData));

    /*
     * performReboot() can be called from the context of an RPC, and the sleep() call below can trigger RPC timeouts. So the time-consuming operations
     * must be handled asynchronously to let this function return promptly. Make a copy of the necessary members so that they can be accessed
     * safely from the new thread.
     * Note: not using strndup() here as lengths of the incoming string are already sanitized.
    */
    char * requestor_cpy = strdup(requestor); 
    char * reboot_reason_custom_cpy = strdup(reboot_reason_custom);
    char * reboot_reason_other_cpy = strdup(reboot_reason_other);

    std::thread async_reboot_thread([requestor_cpy, reboot_reason_custom_cpy, reboot_reason_other_cpy] () {
        v_secure_system("echo 0 > /opt/.rebootFlag");
        sleep(5);
        if(0 == access("/rebootNow.sh", F_OK))
        {
            v_secure_system("/rebootNow.sh -s '%s' -r '%s' -o '%s'", requestor_cpy, reboot_reason_custom_cpy, reboot_reason_other_cpy);
        }
        else
        {
            v_secure_system("/lib/rdk/rebootNow.sh -s '%s' -r '%s' -o '%s'", requestor_cpy, reboot_reason_custom_cpy, reboot_reason_other_cpy);
        }
        free(requestor_cpy);
        free(reboot_reason_custom_cpy);
        free(reboot_reason_other_cpy);
        });
    async_reboot_thread.detach();
}

/** @} */
/** @} */
