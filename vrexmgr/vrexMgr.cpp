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


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string>
#include <sstream>

#include <map>
#include <utility>

#include <curl/curl.h>

#include "libIBus.h"
#include "iarmUtil.h"
#include "vrexMgr.h"
#include "jsonParser.h"
#include "vrexSession.h"
#include "vrexMgrInternal.h"
//#include "iarmStatus.h"
#include "pwrMgr.h"
#include "safec_lib.h"
//#include "gpMgr.h"
#include "rf4ceMgr.h"

#include "comcastIrKeyCodes.h"

#include <assert.h>

#define VERSION_TXT_FILE        "/version.txt"
#define VERSION_TXT_IMAGENAME   "imagename:"

static pthread_mutex_t tMutexLock;
static void _vrexEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
static void _pairEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
static void _keyEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
static bool _sessionExists(unsigned char remoteId);
static void _updateSessionTimeout(unsigned char remoteId);
static bool _loadReceiverId();
static bool _getSTBName();
static void _configureRemotesToWatch();
static void _loadConfiguration();
static int currentPowerMode = 0;
static bool is_powered_on(void);
static IARM_Result_t _PowerPreChange(void *arg);


#ifdef RF4CE_GENMSO_API
static VREXSession& _findOrNewSession(unsigned char remoteId, MSOBusAPI_RfStatus_t *rfStatus = NULL, int bindRemotesIndex = 0);
#elif defined(RF4CE_API)
static VREXSession& _findOrNewSession(unsigned char remoteId, rf4ce_RfStatus_t *rfStatus = NULL, int bindRemotesIndex = 0);
#elif defined(RF4CE_GPMSO_API)
static VREXSession& _findOrNewSession(unsigned char remoteId, gpMSOBusAPI_RfStatus_t *rfStatus = NULL, int bindRemotesIndex = 0);
#else
#warning "No RF4CE API defined"
#endif 

static volatile int initialized = 0;
static volatile bool bVoiceControl=true;
string guideLanguage="";
string aspect_ratio="";

using namespace std;

map<unsigned char, VREXSession> remotes;

static string receiverId;
static string stbName;
static string defaultRemoteName = "XR11-20";
static string defaultRoute = "https://vrex.g.comcast.net/vsp/v1/";
static int defaultConversationLength = 180;
static int defaultExpirationWindow = 60;
static pthread_mutex_t power_save_mutex;

IARM_Result_t VREXMgr_Start()
{
    errno_t rc = -1;	
    char *list[] = { "XR2-", "XR5-", "XR11-", "XR13-" };
    int i;

    IARM_Result_t status = IARM_RESULT_SUCCESS;

	LOG("Entering [%s] - [%s] - disabling io redirect buf\r\n", __FUNCTION__, IARM_BUS_VREXMGR_NAME);
	setvbuf(stdout, NULL, _IOLBF, 0);

    if (!initialized) {
        LOG("I-ARM VREX Mgr: %d\r\n", __LINE__);

        pthread_mutex_init (&tMutexLock, NULL);
        pthread_mutex_lock(&tMutexLock);

        // Unfortunately it doesn't appear that this actually returns a reasonable error on lower level
        // IARM_Init but on the off chance that gets fixed we can deal with errors reasonably
        status = IARM_Bus_Init(IARM_BUS_VREXMGR_NAME);
        if (status == IARM_RESULT_SUCCESS)
            status = IARM_Bus_Connect();

        if (status == IARM_RESULT_SUCCESS) {
            IARM_Bus_RegisterEvent(IARM_BUS_VREXMGR_EVENT_MAX);
            IARM_Bus_RegisterEventHandler(IARM_BUS_VREXMGR_NAME, IARM_BUS_VREXMGR_EVENT_MOTION, _vrexEventHandler);
            IARM_Bus_RegisterEventHandler(IARM_BUS_VREXMGR_NAME, IARM_BUS_VREXMGR_EVENT_SPEECH, _vrexEventHandler);
            IARM_Bus_RegisterEventHandler(IARM_BUS_VREXMGR_NAME, IARM_BUS_VREXMGR_EVENT_SETTINGS, _vrexEventHandler);

            // We need to "snoop" on pairing events to update our list in case a voice remote
            // pairs

#ifdef RF4CE_GENMSO_API
            IARM_Bus_RegisterEventHandler(IARM_BUS_RFMGR_NAME, IARM_BUS_RFMGR_EVENT_MSG_IND, _pairEventHandler);
            IARM_Bus_RegisterEventHandler(IARM_BUS_RFMGR_NAME, IARM_BUS_RFMGR_EVENT_KEY, _keyEventHandler);
#elif defined(RF4CE_API)
            IARM_Bus_RegisterEventHandler(IARM_BUS_RF4CEMGR_NAME, IARM_BUS_RF4CEMGR_EVENT_MSG_IND, _pairEventHandler);
            IARM_Bus_RegisterEventHandler(IARM_BUS_RF4CEMGR_NAME, IARM_BUS_RF4CEMGR_EVENT_KEY, _keyEventHandler);
#elif defined(RF4CE_GPMSO_API) 
            IARM_Bus_RegisterEventHandler(IARM_BUS_GPMGR_NAME, IARM_BUS_GPMGR_EVENT_MSG_IND, _pairEventHandler);
            IARM_Bus_RegisterEventHandler(IARM_BUS_GPMGR_NAME, IARM_BUS_GPMGR_EVENT_KEY, _keyEventHandler);		
#else
#warning "No RF4CE API defined"
#endif            
			//
			// RF4CEPATCH BEGIN: This is temporary code to correctly configure NVM in the RF4CE driver.  Incorrectly moving
			// some platforms to a the new ranking algorithm has introduced the potential for the remote weighting to
			// be incorrect in NVM.  This belongs in generic RF4CE startup code, but that does not exist.  When it is added
			// in in the RF4CE rearchitecture, this code or the new implementation should be moved to that section.
            //
            // This code sets the XRs it expects and their weights.  A weight of 6 is the weight given to a particular remote if
            // it matches up to the STB it is meant for. This information is stored in NVM by GP in new builds. This code
            // ensures the nvm is correct.  A segment of comcast boxes were upgraded to a new weighting algorithm and then
            // downgraded.  This code moves those boxes NVM back to the correct values. 
			//
			{

#ifdef RF4CE_GENMSO_API
				MSOBusAPI_Packet_t expectedRemotesPacket;
				expectedRemotesPacket.msgId = MSOBusAPI_MsgId_ExpectedRemotes;
                expectedRemotesPacket.index  = 0;
                expectedRemotesPacket.length = sizeof(MSOBusAPI_ExpectedRemoteList_t);
#elif defined(RF4CE_API)
 				rf4ce_Packet_t expectedRemotesPacket;
				expectedRemotesPacket.msgId = rf4ce_MsgId_ExpectedRemotes;
                expectedRemotesPacket.index  = 0;
                expectedRemotesPacket.length = sizeof(rf4ce_ExpectedRemoteList_t);
#elif defined(RF4CE_GPMSO_API)
				gpMSOBusAPI_Packet_t expectedRemotesPacket;
				expectedRemotesPacket.msgId = gpMSOBusAPI_MsgId_ExpectedRemotes;
                expectedRemotesPacket.index  = 0;
                expectedRemotesPacket.length = sizeof(gpMSOBusAPI_ExpectedRemoteList_t);
#else
#warning "No RF4CE API defined"
#endif                  
				expectedRemotesPacket.msg.ExpectedRemoteList.numOfRemotes = 4;
				
				for (i = 0 ; i < 4 ; ++i)
                                {
                                expectedRemotesPacket.msg.ExpectedRemoteList.remotes[i].weight = 6;
                                        rc = strcpy_s(expectedRemotesPacket.msg.ExpectedRemoteList.remotes[i].expectedString,sizeof(expectedRemotesPacket.msg.ExpectedRemoteList.remotes[i].expectedString), list[i]);
                                        if(rc!=EOK)
                                        {
                                                ERR_CHK(rc);
                                        }
                                }
				
								

#ifdef RF4CE_GENMSO_API
                IARM_Bus_Call(IARM_BUS_RFMGR_NAME, IARM_BUS_RFMGR_MsgRequest, (void *)&expectedRemotesPacket, sizeof(expectedRemotesPacket));
#elif defined(RF4CE_API)
                IARM_Bus_Call(IARM_BUS_RF4CEMGR_NAME, IARM_BUS_RF4CEMGR_MsgRequest, (void *)&expectedRemotesPacket, sizeof(expectedRemotesPacket));
#elif defined(RF4CE_GPMSO_API)
				IARM_Bus_Call(IARM_BUS_GPMGR_NAME, IARM_BUS_GPMGR_API_MsgRequest, (void *)&expectedRemotesPacket, sizeof(expectedRemotesPacket));
#else
#warning "No RF4CE API defined"
#endif                
			}
			//
			// RF4CEPATCH END
			//
			IARM_Bus_RegisterCall(IARM_BUS_COMMON_API_PowerPreChange, _PowerPreChange);

			IARM_Bus_PWRMgr_GetPowerState_Param_t param;
			IARM_Result_t status=IARM_Bus_Call(IARM_BUS_PWRMGR_NAME, "GetPowerState", (void *) &param, sizeof(param));

			if(status==IARM_RESULT_SUCCESS){
				currentPowerMode=param.curState;
			}
			else{
				currentPowerMode=-1; // we dont know yet,  check again
			}

			__TIMESTAMP();			LOG("_vrexEventHandler startup in power mode %d\n", currentPowerMode);

            // Load our ReceiverId
            while (!_loadReceiverId()) {
                LOG("I-ARM VREX Mgr:BAD DEVICE ID:Waiting 1 second to try again! \n");
            	sleep(1);
            }
            // to be safe in case the file is in process of being written at initial boot
            // grab file again once concidered valid so we are reasonably sure we do not catch
            // file during a write and only get half the data
            if(!_loadReceiverId()){
            	LOG("I-ARM VREX Mgr:DEVICE ID READ FAILED - ABORTING");
            	// this should never happen unless file is deleted.  If it does exit so we can start again.
            	exit(1);
            }

            if (_getSTBName()) {
                LOG("I-ARM VREX Mgr: stbName is: %s\n", stbName.c_str());
            } else {
                LOG("I-ARM VREX Mgr: FAILED in getSTBName()!\n");
            }

            // Load and parse config file
            _loadConfiguration();

            // Check to see if we have any voice remotes that we need to listen
            // to key events for to generate knockknock events
            _configureRemotesToWatch();

            initialized = 1;
        }

        pthread_mutex_unlock(&tMutexLock);
        LOG("I-ARM VREX Mgr: %d\r\n", __LINE__);
    }
    else {
      LOG("I-ARM VREX Mgr Error case: %d\r\n", __LINE__);
      status = IARM_RESULT_INVALID_STATE;
    }
    return status;
}

IARM_Result_t VREXMgr_Loop()
{
    time_t curr = 0;
	int i=0;
    while(1)
    {
        time(&curr);
	i++;
	if(i>500){
        LOG("I-ARM VREX Mgr: HeartBeat at %s\r\n", ctime(&curr));
	i=0;
	}
	fflush(stdout);
        sleep(1);
    }
    return IARM_RESULT_SUCCESS;
}


IARM_Result_t VREXMgr_Stop(void)
{
    if (initialized) {
        pthread_mutex_lock(&tMutexLock);
        IARM_Bus_UnRegisterEventHandler(IARM_BUS_VREXMGR_NAME, IARM_BUS_VREXMGR_EVENT_MOTION);
        IARM_Bus_UnRegisterEventHandler(IARM_BUS_VREXMGR_NAME, IARM_BUS_VREXMGR_EVENT_SPEECH);

#ifdef RF4CE_GENMSO_API
        IARM_Bus_UnRegisterEventHandler(IARM_BUS_RFMGR_NAME, IARM_BUS_RFMGR_EVENT_MSG_IND);
        IARM_Bus_UnRegisterEventHandler(IARM_BUS_RFMGR_NAME, IARM_BUS_RFMGR_EVENT_KEY);
#elif defined(RF4CE_API)
        IARM_Bus_UnRegisterEventHandler(IARM_BUS_RF4CEMGR_NAME, IARM_BUS_RF4CEMGR_EVENT_MSG_IND);
        IARM_Bus_UnRegisterEventHandler(IARM_BUS_RF4CEMGR_NAME, IARM_BUS_RF4CEMGR_EVENT_KEY);
#elif defined(RF4CE_GPMSO_API)
        IARM_Bus_UnRegisterEventHandler(IARM_BUS_GPMGR_NAME, IARM_BUS_GPMGR_EVENT_MSG_IND);
        IARM_Bus_UnRegisterEventHandler(IARM_BUS_GPMGR_NAME, IARM_BUS_GPMGR_EVENT_KEY);
#else
#warning "No RF4CE API defined"
#endif 
        IARM_Bus_Disconnect();
        IARM_Bus_Term();
        pthread_mutex_unlock(&tMutexLock);
        pthread_mutex_destroy (&tMutexLock);
		pthread_mutex_destroy(&power_save_mutex);

        initialized = 0;
        return IARM_RESULT_SUCCESS;
    }
    else {
        return IARM_RESULT_INVALID_STATE;
    }
}

static bool _fileExists(string fileName)
{
  struct stat buffer;
  return (stat(fileName.c_str(), &buffer) == 0);
}


void _keyEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
   // IARMSTATUS_keyEventHandler(owner, eventId, data, len);

#ifdef RF4CE_GENMSO_API
#if defined(DEBUG)
	//LOG("I-ARM VREX Mgr: remoteId <%d> keyHandler (status, code) 0x%lx 0x%lx\n",((MSOBusAPI_UserCommand_t*)data)->remoteId,
        ((MSOBusAPI_UserCommand_t*)data)->keyStatus,((MSOBusAPI_UserCommand_t*)data)->commandCode);
#endif
    if (((MSOBusAPI_UserCommand_t*)data)->keyStatus == KET_KEYUP && _sessionExists(((MSOBusAPI_UserCommand_t*)data)->remoteId)) {
	    //LOG("I-ARM VREX Mgr: remoteId <%d> keyHandler calling updatesessiontimeout\n",((MSOBusAPI_UserCommand_t*)data)->remoteId);
        _updateSessionTimeout(((MSOBusAPI_UserCommand_t*)data)->remoteId);
    }
#elif defined(RF4CE_API)
#if defined(DEBUG)
	//LOG("I-ARM VREX Mgr: remoteId <%d> keyHandler (status, code) 0x%lx 0x%lx\n",((rf4ce_UserCommand_t*)data)->remoteId,
        ((rf4ce_UserCommand_t*)data)->keyStatus,((rf4ce_UserCommand_t*)data)->commandCode);
#endif
    if (((rf4ce_UserCommand_t*)data)->keyStatus == KET_KEYUP && _sessionExists(((rf4ce_UserCommand_t*)data)->remoteId)) {
	    //LOG("I-ARM VREX Mgr: remoteId <%d> keyHandler calling updatesessiontimeout\n",((rf4ce_UserCommand_t*)data)->remoteId);
        _updateSessionTimeout(((rf4ce_UserCommand_t*)data)->remoteId);
    }
#elif defined(RF4CE_GPMSO_API)
#if defined(DEBUG)
	//LOG("I-ARM VREX Mgr: remoteId <%d> keyHandler (status, code) 0x%lx 0x%lx\n",((gpMSOBusAPI_UserCommand_t*)data)->remoteId,
	((gpMSOBusAPI_UserCommand_t*)data)->keyStatus,((gpMSOBusAPI_UserCommand_t*)data)->commandCode);
#endif
    if (((gpMSOBusAPI_UserCommand_t*)data)->keyStatus == KET_KEYUP && _sessionExists(((gpMSOBusAPI_UserCommand_t*)data)->remoteId)) {
	    //LOG("I-ARM VREX Mgr: remoteId <%d> keyHandler calling updatesessiontimeout\n",((gpMSOBusAPI_UserCommand_t*)data)->remoteId);
        _updateSessionTimeout(((gpMSOBusAPI_UserCommand_t*)data)->remoteId);
    }
#else
#warning "No RF4CE API defined"
#endif    
}


void _pairEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{

#ifdef RF4CE_GENMSO_API
    MSOBusAPI_Packet_t* busMsg;
    busMsg = (MSOBusAPI_Packet_t*)data;

    if(len != (busMsg->length + sizeof(MSOBusAPI_Packet_t) - sizeof(MSOBusAPI_Msg_t))) //Message size + header of packet
    {
        LOG("I-ARM VREX Mgr: %i pairEventHandler with wrong length rec:%d exp:%d\r\n",eventId, len, (busMsg->length + sizeof(MSOBusAPI_Packet_t) - sizeof(MSOBusAPI_Msg_t)));
        return;
    }

    switch(busMsg->msgId)
    {
        case MSOBusAPI_MsgId_ValidationComplete:
        {
            switch(busMsg->msg.ValidationStatus.status)
            {
                case MSOBusAPI_ValidationResult_Success:
                {
                    LOG("I-ARM VREX Mgr: New remote paired successfully <%d>", busMsg->msg.UserCommand.remoteId);
                    // Run this again so we pick up the newly paired remote
                    sleep(1);
                    _configureRemotesToWatch();
                    break;
                }
                default:
                    break;
            }
            break;
        }
        default:
            break;
    }
#elif defined(RF4CE_API)
    rf4ce_Packet_t* busMsg;
    busMsg = (rf4ce_Packet_t*)data;

    if(len != (busMsg->length + sizeof(rf4ce_Packet_t) - sizeof(rf4ce_Msg_t))) //Message size + header of packet
    {
        LOG("I-ARM VREX Mgr: %i pairEventHandler with wrong length rec:%d exp:%d\r\n",eventId, len, (busMsg->length + sizeof(rf4ce_Packet_t) - sizeof(rf4ce_Msg_t)));
        return;
    }

    switch(busMsg->msgId)
    {
        case rf4ce_MsgId_ValidationComplete:
        {
            switch(busMsg->msg.ValidationStatus.status)
            {
                case rf4ce_ValidationResult_Success:
                {
                    LOG("I-ARM VREX Mgr: New remote paired successfully <%d>", busMsg->msg.UserCommand.remoteId);
                    // Run this again so we pick up the newly paired remote
                    sleep(1);
                    _configureRemotesToWatch();
                    break;
                }
                default:
                    break;
            }
            break;
        }
        default:
            break;
    }
#elif defined(RF4CE_GPMSO_API)
    gpMSOBusAPI_Packet_t* busMsg;
    busMsg = (gpMSOBusAPI_Packet_t*)data;

    if(len != (busMsg->length + sizeof(gpMSOBusAPI_Packet_t) - sizeof(gpMSOBusAPI_Msg_t))) //Message size + header of packet
    {
        LOG("I-ARM VREX Mgr: %i pairEventHandler with wrong length rec:%d exp:%d\r\n",eventId, len, (busMsg->length + sizeof(gpMSOBusAPI_Packet_t) - sizeof(gpMSOBusAPI_Msg_t)));
        return;
    }

    switch(busMsg->msgId)
    {
        case gpMSOBusAPI_MsgId_ValidationComplete:
        {
            switch(busMsg->msg.ValidationStatus.status)
            {
                case gpMSOBusAPI_ValidationResult_Success:
                {
                    LOG("I-ARM VREX Mgr: New remote paired successfully <%d>", busMsg->msg.UserCommand.remoteId);
                    // Run this again so we pick up the newly paired remote
                    sleep(1);
                    _configureRemotesToWatch();
                    break;
                }
                default:
                    break;
            }
            break;
        }
        default:
            break;
    }
#else
#warning "No RF4CE API defined"
#endif     
}


static void _updateSessionTimeout(unsigned char remoteId)
{
    VREXSession &vrSession = _findOrNewSession(remoteId);
    vrSession.updateExpiration(defaultExpirationWindow);
}


#ifdef RF4CE_GENMSO_API
static bool _isVoiceRemote(MSOBusAPI_BindRemote_t boundRemote)
{
    // Can do something fancier later
    if (defaultRemoteName == boundRemote.Type)
        return true;
    else
        return false;
}
#elif defined(RF4CE_API)
static bool _isVoiceRemote(rf4ce_BindRemote_t boundRemote)
{
    // Can do something fancier later
    if (defaultRemoteName == boundRemote.Type)
        return true;
    else
        return false;
}
#elif defined(RF4CE_GPMSO_API)
static bool _isVoiceRemote(gpMSOBusAPI_BindRemote_t boundRemote)
{
    // Can do something fancier later
    if (defaultRemoteName == boundRemote.Type)
        return true;
    else
        return false;
}
#else
#warning "No RF4CE API defined"
#endif 

static void _configureRemotesToWatch()
{

#ifdef RF4CE_GENMSO_API
    MSOBusAPI_Packet_t getRequest;
    getRequest.msgId  = MSOBusAPI_MsgId_GetRfStatus;
    getRequest.length = sizeof(MSOBusAPI_RfStatus_t);
    getRequest.index  = 0;

    IARM_Bus_Call(IARM_BUS_RFMGR_NAME, IARM_BUS_RFMGR_MsgRequest, (void *)&getRequest, sizeof(getRequest));

    MSOBusAPI_Packet_t* dbusMsg = &getRequest;
    MSOBusAPI_Msg_t* av = &(dbusMsg->msg);
#elif defined(RF4CE_API)
    rf4ce_Packet_t getRequest;
    getRequest.msgId  = rf4ce_MsgId_GetRfStatus;
    getRequest.length = sizeof(rf4ce_RfStatus_t);
    getRequest.index  = 0;

    IARM_Bus_Call(IARM_BUS_RF4CEMGR_NAME, IARM_BUS_RF4CEMGR_MsgRequest, (void *)&getRequest, sizeof(getRequest));

    rf4ce_Packet_t* dbusMsg = &getRequest;
    rf4ce_Msg_t* av = &(dbusMsg->msg);
#elif defined(RF4CE_GPMSO_API)
    gpMSOBusAPI_Packet_t getRequest;
    getRequest.msgId  = gpMSOBusAPI_MsgId_GetRfStatus;
    getRequest.length = sizeof(gpMSOBusAPI_RfStatus_t);
    getRequest.index  = 0;

    IARM_Bus_Call(IARM_BUS_GPMGR_NAME, IARM_BUS_GPMGR_API_MsgRequest, (void *)&getRequest, sizeof(getRequest));

    gpMSOBusAPI_Packet_t* dbusMsg = &getRequest;
    gpMSOBusAPI_Msg_t* av = &(dbusMsg->msg);
#else
#warning "No RF4CE API defined"
#endif 
    switch(dbusMsg->msgId)
    {

#ifdef RF4CE_GENMSO_API
	    case MSOBusAPI_MsgId_GetRfStatus:            
#elif defined(RF4CE_API)
        case rf4ce_MsgId_GetRfStatus:
#elif defined(RF4CE_GPMSO_API)
	    case gpMSOBusAPI_MsgId_GetRfStatus:
#else
#warning "No RF4CE API defined"
#endif           
	    {
	        int i;

                //Clear the remotes since we're going to repopulate
                if(!remotes.empty())
                {
                    remotes.clear();
                }

	        //Remotes are ordered 'Most Recently Used' remote first

#ifdef RF4CE_GENMSO_API
	        for(i=0; i < MSO_BUS_API_MAX_BINDED_REMOTES-1; i++)
#elif defined(RF4CE_API)
	        for(i=0; i < RF4CE_MAX_BINDED_REMOTES-1; i++)
#elif defined(RF4CE_GPMSO_API)
	        for(i=0; i < GP_MSO_BUS_API_MAX_BINDED_REMOTES-1; i++)
#else
#warning "No RF4CE API defined"
#endif                  
	        {
                // Only for valid entries!!!
	            if(av->RfStatus.bindRemotes[i].ShortAddress != 0xFFFF) {

                    if (_isVoiceRemote(av->RfStatus.bindRemotes[i])) {
                        // We will populate the remote here and test for existence in the keypress event
                        VREXSession &vrSession = _findOrNewSession(av->RfStatus.bindRemotes[i].remoteId, &(av->RfStatus), i);
                        LOG("Remote %d, remoteId <%d> remote_controller_type %s is voice enabled!\n", i, (int)av->RfStatus.bindRemotes[i].remoteId, av->RfStatus.bindRemotes[i].Type);
                    }
                    else {
                        LOG("Remote %d, remoteId <%d> remote_controller_type %s is *not* voice enabled!\n", i, (int)av->RfStatus.bindRemotes[i].remoteId, av->RfStatus.bindRemotes[i].Type);
                    }
                }

#if defined(DEBUG)
	            //Only print valid entries
	            if(av->RfStatus.bindRemotes[i].ShortAddress != 0xFFFF)
	            {
                    LOG("Remote %d remote_fw_version %d.%d.%d.%d\n",i ,av->RfStatus.bindRemotes[i].VersionInfoSw[0], \
                                                                       av->RfStatus.bindRemotes[i].VersionInfoSw[1], \
                                                                       av->RfStatus.bindRemotes[i].VersionInfoSw[2], \
                                                                       av->RfStatus.bindRemotes[i].VersionInfoSw[3]);
                    LOG("Remote %d remote_hw_revision %d.%d.%d.%d\n",i \
                                           ,av->RfStatus.bindRemotes[i].VersionInfoHw.manufacturer, \
                                                                       av->RfStatus.bindRemotes[i].VersionInfoHw.model, \
                                                                       av->RfStatus.bindRemotes[i].VersionInfoHw.hwRevision, \
                                                                       av->RfStatus.bindRemotes[i].VersionInfoHw.lotCode);

                    LOG("Remote %d remote_battery_voltage %i.%i V\n", i, (UInt16)((av->RfStatus.bindRemotes[i].BatteryLevelLoaded >> 6) & 0x03), (UInt16)(((av->RfStatus.bindRemotes[i].BatteryLevelLoaded & 0x3F)*100) >> 6));
                    LOG("Remote %d remoteId <%d> remote_controller_type %s\n", i, av->RfStatus.bindRemotes[i].remoteId, av->RfStatus.bindRemotes[i].Type);

	            }
#endif
	        }
	        break;
	    }
        default:
            break;
    }
}

static void _loadConfiguration()
{
    FILE *fp = NULL;

    string confName = "vrexPrefs.json";
    string filePath;

    // Check if file exists in /opt
    filePath = "/opt/" + confName;

    if (!_fileExists(filePath)) {
        filePath = "/mnt/nfs/env/" + confName;
    }

    if (!_fileExists(filePath)) {
        LOG("I-ARM VREX Mgr: Configuration error. Configuration file(s) missing, using defaults\n");
    }
    else if ((fp = fopen(filePath.c_str(), "r")) != NULL) {
        LOG("I-ARM VREX Mgr: Reading configuration from <%s>\n", filePath.c_str());

        string confData;
        char buf[2048];

        while (!feof(fp) && confData.length() < 65535) {
            memset(buf, 0, sizeof(buf));
            if(0 >= fread(buf, 1, sizeof(buf)-1, fp))
            {
                LOG("Error: fread returned Negative or Zero value");
            }
            else
            {
                confData.append(buf);
            }
        }

        if (fp)
            fclose(fp);

        LOG("I-ARM VREX Mgr: Configuration Read <%s>\n", confData.c_str());

        JSONParser parser;
        map<string, JSONParser::varVal *> configuration;
        
        configuration = parser.parse((const unsigned char *)confData.c_str());

        if (configuration["remoteName"]!=NULL) {
            LOG("I-ARM VREX Mgr: Found remote name from config\n");
            defaultRemoteName = configuration["remoteName"]->str;
        }
        else {
            LOG("I-ARM VREX Mgr: No valid remoteName from config using coded default\n");
        }
        if (configuration["route"]!=NULL) {
            LOG("I-ARM VREX Mgr: Found ROUTE from config\n");
            defaultRoute = configuration["route"]->str;
        }
        else {
            LOG("I-ARM VREX Mgr: No valid ROUTE from config using coded default\n");
        }
        if (configuration["defaultConversationLength"]!=NULL) {
            LOG("I-ARM VREX Mgr: Found defaultConversationLength from config\n");
            defaultConversationLength = atoi(configuration["defaultConversationLength"]->str.c_str());
        }
        else {
            LOG("I-ARM VREX Mgr: No valid defaultConversationLength from config using coded default\n");
        }
        if (configuration["defaultExpirationWindow"]!=NULL) {
            LOG("I-ARM VREX Mgr: Found defaultExpirationWindow from config\n");
            defaultExpirationWindow = atoi(configuration["defaultExpirationWindow"]->str.c_str());
        }
        else {
            LOG("I-ARM VREX Mgr: No valid defaultExpirationWindow from config using coded default\n");
        }
    }
    LOG("I-ARM VREX Mgr: default route is: <%s>\n", defaultRoute.c_str());
    LOG("I-ARM VREX Mgr: default conversation length is: <%d> seconds\n", defaultConversationLength);
    LOG("I-ARM VREX Mgr: default expiration window is: <%d> seconds\n", defaultExpirationWindow);
    LOG("I-ARM VREX Mgr: default remote name is: <%s>\n", defaultRemoteName.c_str());
}

static bool _loadReceiverId()
{
    // Try to use authService first
    FILE *fp;
    bool found = true;
    char buffer[128];
    int bytesRead = 0;

    if ((fp = fopen("/opt/www/authService/deviceid.dat", "r")) != NULL || 
        (fp = fopen("/opt/www/whitebox/wbdevice.dat", "r")) != NULL) {
        bytesRead = fread(&buffer[0], 1, sizeof(buffer)-1, fp);
        receiverId.assign(buffer, bytesRead);
        LOG("I-ARM VREX Mgr:Device ID %s\n", receiverId.c_str());
        found=true;
    }
    else
    {
        LOG("I-ARM VREX Mgr: Could not find device id file! \n");
        found = false;
    }

    if (fp)
        fclose(fp);

    return found;
}

static bool _getSTBName()
{
    bool bOk = false;
    // Use the root "version.txt"
    FILE*   fp = fopen(VERSION_TXT_FILE, "r");
    if (fp != NULL) {
        long filesize = 0;
        unsigned char* buffer = NULL;
        size_t imageNameLength = strlen(VERSION_TXT_IMAGENAME);

        fseek(fp, 0L, SEEK_END);
        filesize = ftell(fp);
        if (filesize > (long)imageNameLength) {
            fseek(fp, 0L, SEEK_SET);
            buffer = new unsigned char[filesize + 1];
            if (buffer != NULL) {
                memset((void*)buffer, 0, filesize + 1);
                if (fread(buffer, filesize, 1, fp) > 0) {
                    buffer[filesize] = 0;
                    std::string contents((const char*)buffer);
                    size_t startpos = contents.find(VERSION_TXT_IMAGENAME);
                    if (startpos != std::string::npos) {
                        size_t endpos = 0;
                        contents = contents.substr(startpos + imageNameLength);
                        endpos = contents.find_first_of("_\r\n");
                        if (endpos != std::string::npos) {
                            stbName = contents.substr(0, endpos);
                            bOk = true;
                        } else {
                            LOG("I-ARM VREX Mgr: getSTBName: Could not find value of %s!\n", VERSION_TXT_IMAGENAME);
                        }
                    } else {
                        LOG("I-ARM VREX Mgr: getSTBName: Could not find %s in file!\n", VERSION_TXT_IMAGENAME);
                    }
                } else {
                    delete[] buffer;
                    LOG("I-ARM VREX Mgr: getSTBName: Could not read from file!\n");
                }
            } else {
                LOG("I-ARM VREX Mgr: getSTBName: Could not allocate buffer of size %ld!\n", filesize);
            }
        } else {
            LOG("I-ARM VREX Mgr: getSTBName: File size of %ld is too small!\n", filesize);
        }

        fclose(fp);
    } else {
        LOG("I-ARM VREX Mgr: getSTBName: Could not open file %s!\n", VERSION_TXT_FILE);
    }

    if (!bOk) {
        stbName = "unknown";
    }

    return bOk;
}

static bool _sessionExists(unsigned char remoteId)
{
    return remotes.count(remoteId) == 0 ? false : true;
}


#ifdef RF4CE_GENMSO_API
static VREXSession& _findOrNewSession(unsigned char remoteId, MSOBusAPI_RfStatus_t *rfStatus, int bindRemotesIndex)
{
    if (remotes.count(remoteId) == 0) {
        // Can't find it? Put it in
        VREXSession newSession(remoteId, receiverId, defaultConversationLength, defaultRoute,aspect_ratio, guideLanguage, rfStatus, bindRemotesIndex, stbName);
        remotes[remoteId] = newSession;
    }
    return remotes[remoteId];
}
#elif defined(RF4CE_API)
static VREXSession& _findOrNewSession(unsigned char remoteId, rf4ce_RfStatus_t *rfStatus, int bindRemotesIndex)
{
    if (remotes.count(remoteId) == 0) {
        // Can't find it? Put it in
        VREXSession newSession(remoteId, receiverId, defaultConversationLength, defaultRoute, aspect_ratio, guideLanguage, rfStatus, bindRemotesIndex, stbName);
        remotes[remoteId] = newSession;
    }
    return remotes[remoteId];
}
#elif defined(RF4CE_GPMSO_API)
static VREXSession& _findOrNewSession(unsigned char remoteId, gpMSOBusAPI_RfStatus_t *rfStatus, int bindRemotesIndex)
{
    if (remotes.count(remoteId) == 0) {
        // Can't find it? Put it in
        VREXSession newSession(remoteId, receiverId, defaultConversationLength, defaultRoute, aspect_ratio, guideLanguage, rfStatus, bindRemotesIndex, stbName);
        remotes[remoteId] = newSession;
    }
    return remotes[remoteId];
}
#else
#warning "No RF4CE API defined"
#endif  

static void handleSettingsFromServer(const char *jsonData){
	JSONParser parser;
	string paramData=jsonData;
	map<string, JSONParser::varVal *> settings;
	settings = parser.parse((const unsigned char *) jsonData);

	if(settings["VoiceControlOptIn"]!=NULL)
	{
		__TIMESTAMP();LOG("got VoiceControlOptIn %i",settings["VoiceControlOptIn"]->boolean);
		bVoiceControl=settings["VoiceControlOptIn"]->boolean;
	}
	if(settings["parameters"]!=NULL){
//		__TIMESTAMP();LOG("got parameters %s",settings["parameters"]->str.c_str());
		settings = parser.parse((const unsigned char *) settings["parameters"]->str.c_str());
	}else{
		int start=paramData.find("{",2);
		int end=paramData.find("}",2);
		paramData=paramData.substr(start,end);
//		__TIMESTAMP();LOG("got parameter string %s",paramData.c_str());
		settings = parser.parse((const unsigned char *) paramData.c_str());
	}


	if(settings["vrexURL"]!=NULL)
	{
		__TIMESTAMP();LOG("got vrexURL %s",settings["vrexURL"]->str.c_str());
		defaultRoute=settings["vrexURL"]->str;
	}

	if(settings["GuideLanguage"]!=NULL)
	{
		__TIMESTAMP();LOG("got GuideLanguage %s",settings["GuideLanguage"]->str.c_str());
		guideLanguage=settings["GuideLanguage"]->str;
	}

	if(settings["aspect_ratio"]!=NULL)
	{
		__TIMESTAMP();LOG("got aspect_ratio %s",settings["aspect_ratio"]->str.c_str());
		aspect_ratio=settings["aspect_ratio"]->str;
	}

	typedef std::map<unsigned char, VREXSession>::iterator it_type;
	for(it_type iterator = remotes.begin(); iterator != remotes.end(); iterator++) {
		iterator->second.changeServerDetails(defaultRoute, aspect_ratio,guideLanguage);
	}
}


static void _vrexEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
	__TIMESTAMP();			LOG("_vrexEventHandler event - %d", eventId);
    
	/*Handle only VREX Manager Events */
	if (strcmp(owner, IARM_BUS_VREXMGR_NAME)  == 0) 
	{
        _JSON_EVENT  *eEvent;
        _MOTION_EVENT *mEvent;
        _SPEECH_EVENT *sEvent;

		pthread_mutex_lock(&tMutexLock);

		IARM_Bus_VREXMgr_EventData_t *vrexEventData = (IARM_Bus_VREXMgr_EventData_t*)data;
        unsigned char remoteId = vrexEventData->remoteId;



		if (is_powered_on()==false){
			__TIMESTAMP();			LOG("_vrexEventHandler standyby  mode - No Action");
//			IARM_Bus_VREXMgr_EventData_t eventData;
//			memset(&eventData, 0, sizeof(eventData));
//			eventData.remoteId = remoteId;
//			eventData.data.errorEvent.responseCode = 0;
//			eventData.data.errorEvent.returnCode = 0;
//			strcpy((char *) eventData.data.errorEvent.message, "system in Standby Mode");
//			__TIMESTAMP();			LOG("_vrexEventHandler standyby mode - sending error");
//			IARM_Result_t retval = IARM_Bus_BroadcastEvent(IARM_BUS_VREXMGR_NAME,
//					(IARM_EventId_t) IARM_BUS_VREXMGR_EVENT_ERROR, (void *) &eventData, sizeof(eventData));
//			if (retval == IARM_RESULT_SUCCESS)
//			{
//				__TIMESTAMP();				LOG("Error Event sent successfully");
//			}
//			else
//			{
//				__TIMESTAMP();				LOG("Error Event problem, %i ", retval);
//			}
			pthread_mutex_unlock(&tMutexLock);
			return;
		}

        switch(eventId) {
        	case IARM_BUS_VREXMGR_EVENT_SETTINGS:
		        __TIMESTAMP();LOG("got IARM_BUS_VREXMGR_EVENT_SETTINGS:%s",vrexEventData->data.jsonEvent.jsonData);

        		handleSettingsFromServer((const char *)(vrexEventData->data.jsonEvent.jsonData));
        	break;

            case IARM_BUS_VREXMGR_EVENT_MOTION:
            {
        		if (bVoiceControl==false){
        			__TIMESTAMP();			LOG("_vrexEventHandler voice disabled - No Action");
        			pthread_mutex_unlock(&tMutexLock);
        			return;
        		}

                mEvent = (_MOTION_EVENT *)&vrexEventData->data.motionEvent;
		        __TIMESTAMP();LOG("_vrexEventHandler Motion Event x=%lf, y=%lf, z=%lf", mEvent->x, mEvent->y, mEvent->z);
                MotionInfo motion = { mEvent->x, mEvent->y, mEvent->z };
                VREXSession &vrSession = _findOrNewSession(remoteId);
                if(!vrSession.onMotion(motion)) {
		            __TIMESTAMP();LOG("Error attempting Knock Knock request!\n");
                }
            }
            break;
            case IARM_BUS_VREXMGR_EVENT_SPEECH:
            {
        		if (bVoiceControl==false){
        			__TIMESTAMP();			LOG("_vrexEventHandler voice disabled - No Action");
        			pthread_mutex_unlock(&tMutexLock);
        			return;
        		}
                sEvent = (_SPEECH_EVENT *)&vrexEventData->data.speechEvent;
                switch(sEvent->type) {
                    case IARM_BUS_VREXMGR_SPEECH_BEGIN:
                    {
		                __TIMESTAMP();LOG("_vrexEventHandler Speech Begin Event");
                        VREXSession &vrSession = _findOrNewSession(remoteId);

                        AudioInfo audioInfo;
                        audioInfo.mimeType = (char *)sEvent->data.begin.mimeType;
                        audioInfo.subType = (char *)sEvent->data.begin.subType;
                        audioInfo.language = (char *)sEvent->data.begin.language;

                        // If not defined set it to English
                        if (audioInfo.language.empty()) audioInfo.language = "en";

		                __TIMESTAMP();LOG("AudioInfo: mime: <%s>, subType: <%s>, language: <%s> ",
                            audioInfo.mimeType.c_str(), audioInfo.subType.c_str(), audioInfo.language.c_str());
                        if(vrSession.onStreamStart(audioInfo)) {
                            vrSession.sendState(BeginRecording);
                        }
                        else {
		                    __TIMESTAMP();LOG("_vrexEventHandler onStreamStart failed");
                            vrSession.sendState(FinishedRecordingWithErrors);
                        }
                    }
                    break;
                    case IARM_BUS_VREXMGR_SPEECH_FRAGMENT:
                    {
                		if (bVoiceControl==false){
                			__TIMESTAMP();			LOG("_vrexEventHandler voice disabled - No Action");
                			pthread_mutex_unlock(&tMutexLock);
                			return;
                		}
                        VREXSession &vrSession = _findOrNewSession(remoteId);
                        vrSession.onStreamData(&(sEvent->data.fragment.fragment[0]), sEvent->data.fragment.length);
		                __TIMESTAMP();LOG("_vrexEventHandler Speech Fragment Event \n");
                    }
                    break;
                    case IARM_BUS_VREXMGR_SPEECH_END:
                    {
                		if (bVoiceControl==false){
                			__TIMESTAMP();			LOG("_vrexEventHandler voice disabled - No Action");
                			pthread_mutex_unlock(&tMutexLock);
                			return;
                		}
		                __TIMESTAMP();LOG("_vrexEventHandler Speech End Event \n");
                        VREXSession &vrSession = _findOrNewSession(remoteId);
                        vrSession.onStreamEnd((AudioStreamEndReason)sEvent->data.end.reason);
                        vrSession.sendState(FinishedRecording);
                    }
                    break;
                    default:
		                __TIMESTAMP();LOG("_vrexEventHandler Error, Unknown Speech Event \n");
                    break;
                }
            }
            break;
            default:
		        __TIMESTAMP();LOG("_vrexEventHandler unknown event type \n");
            break;
        }

		pthread_mutex_unlock(&tMutexLock);
	}
    else {
        __TIMESTAMP();LOG("_vrexEventHandler event type not meant for me <%s>...\n", owner);
    }
}

static bool is_powered_on(void)
{
	if(currentPowerMode==-1)
	{
		IARM_Bus_PWRMgr_GetPowerState_Param_t param;
		IARM_Result_t status=IARM_Bus_Call(IARM_BUS_PWRMGR_NAME, "GetPowerState", (void *) &param, sizeof(param));

		if(status==IARM_RESULT_SUCCESS){
			currentPowerMode=param.curState;
		}
		else{
			currentPowerMode=-1; // we dont know yet,  check again
		}
	}
	return currentPowerMode;
}

/**
 * Set the state of the logger based on the power state events
 */
static IARM_Result_t _PowerPreChange(void *arg)
{
	IARM_Bus_CommonAPI_PowerPreChange_Param_t *param = (IARM_Bus_CommonAPI_PowerPreChange_Param_t *) arg;
	IARM_Result_t result = IARM_RESULT_SUCCESS;

	__TIMESTAMP();	STATUS_LOG("PowerPreChange state to %d (STANDBY %d ::ON %d)\n", param->newState, IARM_BUS_PWRMGR_POWERSTATE_STANDBY,
			IARM_BUS_PWRMGR_POWERSTATE_ON);

	pthread_mutex_lock(&power_save_mutex);
	currentPowerMode = param->newState;

	pthread_mutex_unlock(&power_save_mutex);
	__TIMESTAMP();	STATUS_LOG("PowerPreChange set state to %d \n", currentPowerMode);

	return result;
}


/** @} */
/** @} */
