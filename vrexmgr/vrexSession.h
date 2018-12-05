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


#if !defined(VREX_SESSION_H)
#define VREX_SESSION_H

#include <map>
#include <string>
#include <utility>

#include <curl/curl.h>

#include <sys/socket.h>
#include <linux/un.h>
#include <unistd.h>

#include <pthread.h>

#include "jsonParser.h"
//#include "gpMgr.h"
#include "rf4ceMgr.h"



// Time in seconds before curl request times out
#define DEFAULT_VREX_TIMEOUT    30

using namespace std;

typedef struct {
    string  mimeType;
    string  subType;
    string  language;
} AudioInfo;

typedef struct {
    double x;
    double y;
    double z;
} MotionInfo;

// Note that these *must* match IARM_Bus_VREXMgr_SpeechEndReason_t enum in vrexMgr.h
typedef enum {
  AudioDone = 0,  // Normal EOS
  AudioAbort,     // Audio was normally interrupted/cancelled
  AudioError,     // An abnormal error caused end
} AudioStreamEndReason;

// The VREX SendEvent types
typedef enum {
    BeginRecording,
    FinishedRecording,
    FinishedRecordingWithResults,
    FinishedRecordingWithErrors
} SendState;

class VREXSession
{
public:
    VREXSession();

#ifdef RF4CE_GENMSO_API
    VREXSession(unsigned char remoteId, string receiverId, int defaultConversationLength, string route, string aspect_ratio, string language,
                MSOBusAPI_RfStatus_t *rfStatus, int bindRemotesIndex, string stbName, string appId="xr11v2-789ec072-d47e-4eac-9b5d-5f8f7a46e143");
#elif defined(RF4CE_API)  
    VREXSession(unsigned char remoteId, string receiverId, int defaultConversationLength, string route, string aspect_ratio, string language,
                rf4ce_RfStatus_t *rfStatus, int bindRemotesIndex, string stbName, string appId="xr11v2-789ec072-d47e-4eac-9b5d-5f8f7a46e143");
#elif defined(RF4CE_GPMSO_API)
    VREXSession(unsigned char remoteId, string receiverId, int defaultConversationLength, string route, string aspect_ratio, string language,
    gpMSOBusAPI_RfStatus_t *rfStatus, int bindRemotesIndex, string stbName, string appId="xr11v2-789ec072-d47e-4eac-9b5d-5f8f7a46e143");
#else
#warning "No RF4CE API defined"
#endif

    virtual ~VREXSession();

    bool onMotion(MotionInfo);
    bool onStreamStart(AudioInfo audioInfo);
    void onStreamData(void *src, size_t size);
    void onStreamEnd(AudioStreamEndReason);
    bool sendState(SendState state);
    void updateExpiration(int expirationWindow);

    // Internal methods
    void startTransferServer();
    size_t serverSocketCallback(void *buffer, size_t size, size_t nmemb);
    void addKKResponse(string responseData);
    void addSpeechResponse(string responseData);
    void addSendStateResponse(string responseData);
    void changeServerDetails(string route, string aspect_ratio, string language);

private:
    char * getRequestMetrics(CURL *curl,char *);
    void notifySuccess();
    void notifyError(long httpErrorCode , long curlErrorCode, long vrexErrorCode ,long rfErrorCode, const char *message, IARM_Bus_VREXMgr_VoiceCallType_t from);
    void safe_copy(unsigned char *dst, const char *src, size_t len);
    void getDynamicStbAndRemoteinfo();

#ifdef RF4CE_GENMSO_API
    void getStaticStbAndRemoteinfo(MSOBusAPI_RfStatus_t *rfStatus, int bindRemotesIndex);
#elif defined(RF4CE_API) 
    void getStaticStbAndRemoteinfo(rf4ce_RfStatus_t *rfStatus, int bindRemotesIndex);
#elif defined(RF4CE_GPMSO_API)
    void getStaticStbAndRemoteinfo(gpMSOBusAPI_RfStatus_t *rfStatus, int bindRemotesIndex);
#else
#warning "No RF4CE API defined"
#endif      
    string getPostFieldString();
    string getUserAgentString();


private:
    // These are in the query string
    string m_userId;
    string m_appId;

    // These will be part of the post data
    string m_receiverId;
    string m_rf4ceMacAddress;
    string m_remoteBatteryVoltage;
    string m_iArmStatusKillCount;

    // These will be part of the useragent
    string m_rdkImage;
    string m_remoteType;
    string m_remoteSoftwareVersion;
    string m_remoteHardwareVersion;
    string m_stbName;

    // This is data coming back from initial call
    string m_conversationId;

    // This is how I make that initial call
    string m_baseRoute;
	string m_aspect_ratio;
	string m_guideLanguage;

    unsigned char m_remoteId;
    int m_defaultConversationLength; // In seconds
    time_t m_conversationExpires;
    AudioInfo m_audioInfo;

    bool m_curlInitialized;
    bool m_logMetrics;

    string m_kkResponse;
    string m_sendStateResponse;
    string m_speechResponse;

    string m_serverSocketName;
    int m_serverSocket;
    int m_clientSocket; 

    pthread_t   m_dataReadThread;

    pthread_cond_t m_cond;
    pthread_mutex_t m_mutex;

    // The parameters from kk
    map<string, JSONParser::varVal *> m_parameters;
    map<string, JSONParser::varVal *> m_speechResults;
    map<SendState, string> m_sendStates;
};

#endif


/** @} */
/** @} */
