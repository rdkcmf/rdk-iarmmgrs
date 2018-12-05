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


#include <stdlib.h>
#include "vrexMgrInternal.h"
#include "vrexMgr.h"
#include "iarmUtil.h"
#include "libIBus.h"
//#include "iarmStatus.h"

#include <stdio.h>
#include <string.h>
#include <sstream>
#include <exception>


#include <ctime>

#include "vrexSession.h"  //This needs to go below sstream for G8 stable to build

//using namespace std;
//static list<char *> vrexData;

#if !defined(MIN)
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#define MAX_IARMSTATUS_COUNT_LENGTH 10
#define TEMP_BUFFER_SIZE            20

static void* speechServerThread(void *session)
{
    VREXSession *pVREX = (VREXSession *)session;
    pVREX->startTransferServer();
    __TIMESTAMP(); LOG("Exiting speech thread\n");
    return NULL;
}

static size_t voice_writeData(void *buffer, size_t size, size_t nmemb, void *userData)
{
    VREXSession *vrex = (VREXSession *)userData;
    string str;
    str.append((const char *)buffer, size*nmemb);
    __TIMESTAMP(); LOG("voice<%i,%i>: %s", size, nmemb, str.c_str());
    
    vrex->addSpeechResponse(str);
    
    return size*nmemb;
}

static size_t kk_writeData(void *buffer, size_t size, size_t nmemb, void *userData)
{
    VREXSession *vrex = (VREXSession *)userData;
    string str;
    str.append((const char *)buffer, size*nmemb);
    __TIMESTAMP(); LOG("kk_writeData<%i,%i>: %s", size, nmemb, str.c_str());
    vrex->addKKResponse(str); 

    return size*nmemb;
}

static size_t sendState_writeData(void *buffer, size_t size, size_t nmemb, void *userData)
{
    VREXSession *vrex = (VREXSession *)userData;
    string str;
    str.append((const char *)buffer, size*nmemb);
    //__TIMESTAMP(); LOG("sendState_writeData<%i,%i>: %s", size, nmemb, str.c_str());
    vrex->addSendStateResponse(str); 

    return size*nmemb;
}

static size_t read_chunked_socket_callback(void *buffer, size_t size, size_t nmemb, void *userData)
{
    VREXSession *vrex = (VREXSession *)userData;
    return vrex->serverSocketCallback(buffer, size, nmemb);
}


VREXSession::VREXSession() :
    m_curlInitialized(false),
    m_baseRoute("uninit")
{
    //__TIMESTAMP(); LOG("Warning... Empty VREXSession constructor called\n");
}

#ifdef RF4CE_GENMSO_API          
VREXSession::VREXSession(unsigned char remoteId, string receiverId, int defaultConversationLength, string route, string aspect_ratio, string language, MSOBusAPI_RfStatus_t *rfStatus, int bindRemotesIndex, string stbName, string appId) :
#elif defined(RF4CE_API)
VREXSession::VREXSession(unsigned char remoteId, string receiverId, int defaultConversationLength, string route, string aspect_ratio, string language, rf4ce_RfStatus_t *rfStatus, int bindRemotesIndex, string stbName, string appId) :
#elif defined(RF4CE_GPMSO_API)
VREXSession::VREXSession(unsigned char remoteId, string receiverId, int defaultConversationLength, string route, string aspect_ratio, string language, gpMSOBusAPI_RfStatus_t *rfStatus, int bindRemotesIndex, string stbName, string appId) :
#else
#error "No RF4CE API defined"
#endif     
    m_remoteId(remoteId),
    m_receiverId(receiverId),
    m_appId(appId),
    m_stbName(stbName),
    m_defaultConversationLength(defaultConversationLength),
    m_conversationExpires(std::time(0)-200),
    m_curlInitialized(false),
    m_logMetrics(true),
    m_baseRoute(route),
    m_serverSocket(0),
    m_dataReadThread(NULL),
	m_aspect_ratio(aspect_ratio),
	m_guideLanguage(language)
{
    CURLcode res;
    __TIMESTAMP(); LOG("VREXSession constructor for remote %i, %s, %d, %s\n", (int)m_remoteId, m_receiverId.c_str(), m_defaultConversationLength, m_appId.c_str());
    res = curl_global_init(CURL_GLOBAL_DEFAULT);
    /* Check for errors */
    if(res != CURLE_OK) {
        __TIMESTAMP();LOG("Error: Can't init cURL: %s\n", curl_easy_strerror(res));
    }
    else {
        m_curlInitialized = true;
    }

    pthread_cond_init(&m_cond, NULL);
    pthread_mutex_init(&m_mutex, NULL);

    m_clientSocket = -1;

    m_sendStates[BeginRecording] = "BEGIN_RECORDING";
    m_sendStates[FinishedRecording] = "FINISHED_RECORDING";
    m_sendStates[FinishedRecordingWithResults] = "FINISHED_RECORDING_WITH_RESULTS";
    m_sendStates[FinishedRecordingWithErrors] = "FINISHED_RECORDING_WITH_ERRORS";

    if(NULL != rfStatus)
    {
        // Get the static stb and remote info.
        getStaticStbAndRemoteinfo(rfStatus, bindRemotesIndex);
    }
}

VREXSession::~VREXSession()
{
    curl_global_cleanup();
}

void VREXSession::changeServerDetails(string route, string aspect_ratio, string language){
    m_baseRoute=route;

    // Make sure the URL from the vrex server has a / on the end
    if(m_baseRoute[m_baseRoute.length()-1] != '/')
        m_baseRoute += "/";

    m_aspect_ratio=aspect_ratio;
    m_guideLanguage=language;
}

bool VREXSession::onMotion(MotionInfo motion)
{
    bool retval = false;

    if(m_conversationExpires==0)
    {
    	// this is an invalid time which means a previous KK attempt failed.  we do
    	// not want to try again from anyone but a sendstate which will fix this before calling
    	return false;
    }
    // clear the conversation ID so no other KK's happen till a valid session is created and expires
    m_conversationExpires=0;
    __TIMESTAMP(); LOG("KnockKnock for remote %i, %s, %s\n", (int)m_remoteId, m_receiverId.c_str(), m_appId.c_str());
    if (!m_curlInitialized) return retval;

    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();

    m_kkResponse.clear();

    __TIMESTAMP(); LOG("KnockKnock URL: %s",  (m_baseRoute + "knockknock").c_str());
    curl_easy_setopt(curl, CURLOPT_URL, (m_baseRoute + "knockknock").c_str());

    // Post Data
    string postData = "receiverId=" + m_receiverId + "&appId=" + m_appId;

    if (!m_guideLanguage.empty())
     {
    	postData += "&language=" + m_guideLanguage;
     }

     if (!m_aspect_ratio.empty())
     {
    	 postData += "&aspectRatio=" + m_aspect_ratio;
     }



    __TIMESTAMP(); LOG("KnockKnock POSTFIELDS: %s", postData.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, kk_writeData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, this); 
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, DEFAULT_VREX_TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    //Add the user agent field
    string userAgentData = getUserAgentString();
    __TIMESTAMP(); LOG("KnockKnock USERAGENT: %s\n", userAgentData.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgentData.c_str());
 
    res = curl_easy_perform(curl);

    // Error?
    if(res != CURLE_OK) {
        std::stringstream sstm;
        sstm << "cURL call failed KnockKnock:(" << res << "): " << curl_easy_strerror(res);
        string errorMessage = sstm.str();
        //errorMessage = errorMessage + curl_easy_strerror(res);
        __TIMESTAMP(); LOG("onMotion(): %s\n", errorMessage.c_str());
       	//long httpErrorCode , long curlErrorCode, long vrexErrorCode ,long rfErrorCode
            notifyError(0, res,0,0, errorMessage.c_str(),KNOCKKNOCK);
    }
    else {
        JSONParser parser;
        try{
        	retval = true;
        m_parameters = parser.parse((const unsigned char *)m_kkResponse.c_str());

        if (m_parameters["code"]->str == "0") {
            time_t currentTime = std::time(0);
            __TIMESTAMP(); LOG("Time now: %s", asctime(localtime(&currentTime)));
            m_conversationExpires = std::time(0) + m_defaultConversationLength;
            __TIMESTAMP(); LOG("Conversation expires: %s\n", asctime(localtime(&m_conversationExpires)));

            LOG("onMotion CID: <%s>\n", m_parameters["cid"]->str.c_str());
        }
        else {
            long responseCode;
            long returnCode;
            // Error in the service itself
            LOG("onMotion response error!\n");
            LOG("onMotion code: <%s>\n", m_parameters["code"]->str.c_str());
            LOG("onMotion message: <%s>\n", m_parameters["message"]->str.c_str());
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
            returnCode = strtol(m_parameters["code"]->str.c_str(), NULL, 10);
           	//long httpErrorCode , long curlErrorCode, long vrexErrorCode ,long rfErrorCode
                notifyError(responseCode, 0,returnCode,0, m_parameters["message"]->str.c_str(),KNOCKKNOCK);
        }
        }catch (const std::exception err){
        	__TIMESTAMP(); LOG("exception in OnMotion response: %s",err.what());
           	__TIMESTAMP(); LOG("OnMotion response string for exception: %s",m_kkResponse.c_str());
           	std::stringstream sstm;
           	sstm << "Unknown exception:" << err.what() << ": response:" << m_kkResponse.c_str();
            string errorMessage = sstm.str();
            notifyError(0, 0,800,0, errorMessage.c_str(),KNOCKKNOCK);
            retval = false;
        }

    }

    if (m_logMetrics){
    	char buffer[700];
    	getRequestMetrics(curl,buffer);
    	LOG("%s",buffer);
    }

    curl_easy_cleanup(curl);

    return retval;
}

void VREXSession::notifySuccess()
{
    IARM_Bus_VREXMgr_EventData_t eventData;
    memset(&eventData, 0, sizeof(eventData));

    eventData.remoteId = m_remoteId;

    IARM_Result_t retval = IARM_Bus_BroadcastEvent(IARM_BUS_VREXMGR_NAME, (IARM_EventId_t)IARM_BUS_VREXMGR_EVENT_SUCCESS,(void *)&eventData,sizeof(eventData));
    if (retval == IARM_RESULT_SUCCESS) {
        __TIMESTAMP(); LOG("Success Event sent successfully");
    }
    else {
        __TIMESTAMP(); LOG("Success Event problem, %i", retval);
    }
}


void VREXSession::notifyError(long httpErrorCode , long curlErrorCode, long vrexErrorCode ,long rfErrorCode, const char *message, IARM_Bus_VREXMgr_VoiceCallType_t from)
{
    IARM_Bus_VREXMgr_EventData_t eventData;
    memset(&eventData, 0, sizeof(eventData));
    eventData.remoteId = m_remoteId;


//    if(curl!=NULL){
//    	char buffer[700];
//
//    	sendMsg+=":curl Details:";
//    	sendMsg+=getRequestMetrics(curl,buffer);
//    }
//

//
//    switch (curlError){
//    case 6: //CURLE_COULDNT_RESOLVE_HOST
//    	break;
//    case 60: //CURLE_SSL_CACERT
//    case 77: //CURLE_SSL_CACERT_BADFILE
//    {
//    	struct curl_slist *sslEngines;
//        curl_easy_getinfo(curl, CURLINFO_SSL_ENGINES, &sslEngines);
//        std::stringstream sstm;
//        sstm << "::cURL sslEngines:" << sslEngines;
//        sendMsg+=sstm.str();
//        curl_slist_free_all(sslEngines);
//
//    }
//        break;
//
//    case 52: //CURLE_GOT_NOTHING
//    	break;
//    case 28: //CURLE_OPERATION_TIMEDOUT
//    	break;
//    }

    bool needComma=false;
    std::stringstream jsonString;

    jsonString<<"{";

    jsonString<<"\"vrexCallType\":\"";
    		jsonString<<from;
    jsonString<<"\",";
    jsonString<<"\"remoteId\":\"";
    jsonString<<(int)m_remoteId;
    jsonString<<"\", \"errors\":{";

    if(httpErrorCode>0){
		jsonString<<"\"httpErrorCode\":\"";
		jsonString<<(int)httpErrorCode;
		needComma=true;
	    jsonString<<"\"";
    }

    if(curlErrorCode>0){
    	if(needComma){
    	    jsonString<<",";
    	}
		needComma=true;
		jsonString<<"\"curlErrorCode\":\"";
		jsonString<<(int)curlErrorCode;
	    jsonString<<"\"";
    }
    if(vrexErrorCode>0){
    	if(needComma){
    	    jsonString<<",";
    	}
		needComma=true;
		jsonString<<"\"vrexErrorCode\":\"";
		jsonString<<(int)vrexErrorCode;
	    jsonString<<"\"";
    }

    if(rfErrorCode>0){
    	if(needComma){
    	    jsonString<<",";
    	}
		needComma=true;
		jsonString<<"\"remoteErrorCode\":\"";
		jsonString<<(int)rfErrorCode;
	    jsonString<<"\"";
    }
	if(needComma){
	    jsonString<<",";
	}
	needComma=true;
	jsonString<<"\"message\":\"";
	jsonString<<message;

    jsonString<<"\"}}";

    string erroStr(jsonString.str());
    LOG("sending event %s",erroStr.c_str());
    safe_copy(eventData.data.jsonEvent.jsonData, erroStr.c_str(), IARM_BUS_VREXMGR_ERROR_MESSAGE_LENGTH);
    IARM_Result_t retval = IARM_Bus_BroadcastEvent(IARM_BUS_VREXMGR_NAME, (IARM_EventId_t)IARM_BUS_VREXMGR_EVENT_ERROR,(void *)&eventData,sizeof(eventData));	
    if (retval == IARM_RESULT_SUCCESS) {
        __TIMESTAMP(); LOG("Error Event sent successfully");
    }
    else {
        __TIMESTAMP(); LOG("Error Event problem, %i", retval);
    }
}


void VREXSession::safe_copy(unsigned char *dst, const char *src, size_t len)
{
    if (len < 0) return;

    len = MIN(strlen(src), (len - 1));

    // Because glibc maintainers don't like strlcpy....
    *((char *) mempcpy (dst, src, len)) = '\0';
}

void VREXSession::addKKResponse(string responseData)
{
    m_kkResponse += responseData;
}

void VREXSession::addSpeechResponse(string responseData)
{
    m_speechResponse += responseData;
}

void VREXSession::addSendStateResponse(string responseData)
{
    m_sendStateResponse += responseData;
}

bool VREXSession::onStreamStart(AudioInfo audioInfo)
{
    string errorMessage;
    m_audioInfo = audioInfo;

    // special case that when we get a StreamStart we want to try the conversation regardless
    // of past failures, so make the timeout valid if needed so KK will go through.
    if(m_conversationExpires==0){
    	// invalid conversation time so fix
    	m_conversationExpires=std::time(0)-200;
    }

    pthread_mutex_lock(&m_mutex);
//    vrexData.clear();
    time_t currentTime = std::time(0);
    if (currentTime > m_conversationExpires) {
        MotionInfo mi = { 0, 0, 0 };
        __TIMESTAMP(); LOG("Conversation id has expired! attempting to acquire another one via knock knock\n");

        if (!onMotion(mi)) {
            pthread_mutex_unlock(&m_mutex);
            errorMessage = "Conversation id expired and could not reacquire. Aborting speech call";
            goto handleError;
        }
    }

    m_speechResponse.clear();

    if (pthread_create(&m_dataReadThread, NULL, speechServerThread, (void *)this) != 0)
    {
        pthread_mutex_unlock(&m_mutex);
        errorMessage = "Could not create thread to send speech data";
        goto handleError;
    }
    __TIMESTAMP(); LOG("Thread blocked waiting for initialization");
    pthread_cond_wait(&m_cond, &m_mutex);
    pthread_mutex_unlock(&m_mutex);

    // Now start the client side of the code.
    struct sockaddr_un address;
    int  nbytes;
    char buffer[256];

    m_clientSocket = socket(PF_UNIX, SOCK_STREAM, 0);
    if(m_clientSocket < 0) {
        errorMessage = "Speech socket call failed";
        goto handleError;
    }

    __TIMESTAMP(); LOG("Server socket name is <%s>", m_serverSocketName.c_str());

    memset(&address, 0, sizeof(struct sockaddr_un));
    address.sun_family = AF_UNIX;
    snprintf(address.sun_path, UNIX_PATH_MAX, m_serverSocketName.c_str());

    if(connect(m_clientSocket, (struct sockaddr *) &address, sizeof(struct sockaddr_un)) != 0)
    {
        errorMessage = "Speech connect call failed";
        goto handleError;
    }

    return true;

handleError:
	__TIMESTAMP(); LOG("Error Message:%s",errorMessage.c_str());
	//long httpErrorCode , long curlErrorCode, long vrexErrorCode ,long rfErrorCode
	//TODO need to figure out better messaging
    notifyError(0, 0,0,0, errorMessage.c_str(),SPEECH);
    return false;
}

void VREXSession::onStreamData(void *src, size_t size)
{
    pthread_mutex_lock(&m_mutex);
    __TIMESTAMP(); LOG("onStreamData: numbytes sent: %i", (int)size);
    if (m_clientSocket >= 0)
        write(m_clientSocket, src, size);
    pthread_mutex_unlock(&m_mutex);
}

void VREXSession::onStreamEnd(AudioStreamEndReason reason)
{
    if (m_clientSocket >= 0) {
        pthread_mutex_lock(&m_mutex);
        __TIMESTAMP(); LOG("Deleting client socket from onStreamEnd\n");
        close(m_clientSocket);
        m_clientSocket = -1;
        pthread_mutex_unlock(&m_mutex);

        __TIMESTAMP(); LOG("Waiting for server thread to exit\n");
        pthread_join(m_dataReadThread, NULL);
        __TIMESTAMP(); LOG("Server thread exited!\n");
    }

    if (reason != AudioDone) {
        string message = "Audio stream ";
        message = message + ((reason == AudioAbort) ? "canceled" : "aborted due to error") + " from remote";
    	//long httpErrorCode , long curlErrorCode, long vrexErrorCode ,long rfErrorCode
    	//TODO need to figure out better messaging
        notifyError(0, 0,0,(long)reason, message.c_str(),SPEECH);
    }

}

void VREXSession::updateExpiration(int expirationWindow)
{
    time_t currentTime = std::time(0);

    // we only update a valid expire time that is not expired
    if (m_conversationExpires>0 && (currentTime + expirationWindow > m_conversationExpires)) {
        MotionInfo mi = { 0, 0, 0 };
        __TIMESTAMP(); LOG("VREXSession: updateExpiration, conversation about to expire. Attempting to extend via knock knock\n");

        // Try to extend
        if (!onMotion(mi)) {
            string errorMessage;
            errorMessage = "Conversation id about to expire and could not reacquire.";
            __TIMESTAMP(); LOG("Error: %s",errorMessage.c_str());
            // Do we really want to notify an error here? It's being generated synthetically and
            // feedback could throw off UI or remote
            //notifyError(0, 0, errorMessage.c_str());
        }
    }
}

bool VREXSession::sendState(SendState state)
{
    bool retval = false;
redoSendState:
    __TIMESTAMP(); LOG("sendState for remote %i, %i\n", (int)m_remoteId, (int)state);

    if (!m_curlInitialized) return retval;

    CURL *curl;
    CURLcode res;

    time_t currentTime = std::time(0);

    curl = curl_easy_init();

    m_sendStateResponse.clear();

    if(m_parameters["cid"]!=NULL){

    string url = m_baseRoute + "xapi/sendstate?cid=" + m_parameters["cid"]->str + "&state=" + m_sendStates[state];
    __TIMESTAMP(); LOG("sendState URL: %s\n",  url.c_str());
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    /* Now specify the POST data */ 
    string postData = getPostFieldString();
    __TIMESTAMP(); LOG("sendState POSTFIELDS: %s\n", postData.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, sendState_writeData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, this); 
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, DEFAULT_VREX_TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    //Add the user agent field
    string userAgentData = getUserAgentString();
    __TIMESTAMP(); LOG("sendState USERAGENT: %s\n", userAgentData.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgentData.c_str());
 
    /* Perform the request, res will get the return code */ 
    res = curl_easy_perform(curl);

    /* Check for errors */ 
    if(res != CURLE_OK) {
        std::stringstream sstm;
        sstm << "cURL call failed (" << res << "): " << curl_easy_strerror(res);
        string errorMessage = sstm.str();
        //errorMessage = errorMessage + curl_easy_strerror(res);
        __TIMESTAMP(); LOG("sendstate: %s\n", errorMessage.c_str());
     //   notifyError(0, 0, errorMessage.c_str());
    }
    else {
    	try{
			time_t currentTime = std::time(0);
			__TIMESTAMP(); LOG("Updating expiration time, time now: %s\n", asctime(localtime(&currentTime)));
			m_conversationExpires = std::time(0) + m_defaultConversationLength;
			__TIMESTAMP(); LOG("Conversation expires: %s\n", asctime(localtime(&m_conversationExpires)));
			JSONParser parser;
			__TIMESTAMP(); LOG("Result = %s", m_sendStateResponse.c_str());

			__TIMESTAMP(); LOG("MYRESULT = %s", m_sendStateResponse.c_str());
			map<string, JSONParser::varVal *> result;
			result = parser.parse((const unsigned char *)m_sendStateResponse.c_str());
			__TIMESTAMP(); LOG("AFTER MYRESULT = %s", m_sendStateResponse.c_str());


			if (result["code"]!=NULL && result["code"]->str != "0") {
				if (result["code"]->str == "213") {
					MotionInfo mi = { 0, 0, 0 };
					__TIMESTAMP(); LOG("Conversation id has expired! attempting to acquire another one via knock knock\n");

					if (!onMotion(mi)) {
						__TIMESTAMP(); LOG("could not handle the 213 error\n");
				   }
					else{
						goto redoSendState;
					}
				}else{
					__TIMESTAMP(); LOG("not a 213 error\n");


				}


				long responseCode;
				long returnCode;
				// Error in the service itself
				LOG("sendstate CODE: <%s>", result["code"]->str.c_str());
				LOG("sendstate MESSAGE: <%s>", result["message"]->str.c_str());

				curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
				returnCode = strtol(result["code"]->str.c_str(), NULL, 10);
	//            notifyError(responseCode, returnCode, result["message"].c_str());
			}

			retval = true;
		}catch (const std::exception err){
			__TIMESTAMP(); LOG("exception in SENDSTATE response: %s", err.what());
				__TIMESTAMP(); LOG("SENDSTATE response string for exception: %s",m_kkResponse.c_str());
				std::stringstream sstm;
				sstm << "Unknown exception:" << err.what() << ":SENDSTATE response:" << m_sendStateResponse.c_str();
			 string errorMessage = sstm.str();
			 notifyError(0, 0,800,0, errorMessage.c_str(),SENDSTATE);
			 retval=false;
		 }

    }

    if (m_logMetrics){
    	char buffer[700];
    	getRequestMetrics(curl,buffer);
    	LOG("%s",buffer);

    }

    curl_easy_cleanup(curl);
    }

    return retval;
}

size_t VREXSession::serverSocketCallback(void *buffer, size_t size, size_t nmemb)
{
    size_t numBytes;

    //__TIMESTAMP(); LOG("serverSocketCallback: fill up to bytes: <%i>:", size*nmemb);

    if(size*nmemb < 1)
        return 0;

    numBytes = read(m_serverSocket, buffer, size*nmemb);
    __TIMESTAMP(); LOG("serverSocketCallback: got bytes: <%i>:", numBytes);

//    char* mybuff=new char[(numBytes)+2];
//    *(short *)mybuff=numBytes;
//    memcpy(mybuff+2,buffer,numBytes);
//    vrexData.push_back(mybuff);


    return numBytes;
}

void VREXSession::startTransferServer()
{
    int nbytes;
    char buffer[256];
    int socket_fd;
    struct sockaddr_un address;
    socklen_t address_length;

    __TIMESTAMP(); LOG("startTransferServer\n");

    socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if(socket_fd < 0)
    {
        __TIMESTAMP(); LOG("socket() failed\n");
        return;
    } 

    // Don't want to use itoa
    ostringstream stream;
    int remoteId = (int)m_remoteId;
    __TIMESTAMP(); LOG("remoteId= %i", remoteId);
    __TIMESTAMP(); LOG("receiverId= %s", m_receiverId.c_str());
    __TIMESTAMP(); LOG("appId= %s", m_appId.c_str());

    stream << "/tmp/.vrex" << remoteId << "_socket";
    string socketName(stream.str());

    m_serverSocketName = socketName;

    __TIMESTAMP(); LOG("sts m_serverSocketName = %s\n", m_serverSocketName.c_str());
    unlink(socketName.c_str());

    memset(&address, 0, sizeof(struct sockaddr_un));
    address.sun_family = AF_UNIX;
    snprintf(address.sun_path, UNIX_PATH_MAX, socketName.c_str());

    if(bind(socket_fd, (struct sockaddr *) &address, 
                sizeof(struct sockaddr_un)) != 0)
    {
        close(socket_fd);
        __TIMESTAMP(); LOG("sts bind failed");
        return;
    }

    if(listen(socket_fd, 5) != 0)
    {
        close(socket_fd);
        __TIMESTAMP(); LOG("sts listen failed");
        return;
    }

    usleep(5 * 1000);  // simple wait to make sure streamStart thread is waiting before broadcast;
    __TIMESTAMP(); LOG("Unblocking waiting thread(s)");
    // Unblock parent thread
    pthread_cond_broadcast(&m_cond);
    __TIMESTAMP(); LOG("Thread(s) unblocked(s)");
    address_length = sizeof(sockaddr_un);
    if ((m_serverSocket = accept(socket_fd, (struct sockaddr *) &address, &address_length)) > -1)
    {
        CURL *curl;
        CURLcode res;
        struct curl_slist *chunk = NULL;

        curl = curl_easy_init();

        if(curl) {

             string url;

            url = m_baseRoute + "speech?cid=" + m_parameters["cid"]->str + "&filters=SR,NLP,X1&codec=" + m_audioInfo.subType + "&receiverId=" + m_receiverId + "&appId=" + m_appId;

            if (!m_guideLanguage.empty())
            {
                url += "&language=" + m_guideLanguage;
            }

            if (!m_aspect_ratio.empty())
            {
                url += "&aspectRatio=" + m_aspect_ratio;
            }

                //+ "&mimeType=" + m_audioInfo.mimeType + "&lang=" + m_audioInfo.language;
            __TIMESTAMP(); LOG("Speech connection to: <%s>", url.c_str());

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_chunked_socket_callback);

            /* pointer to pass to our read function */
            curl_easy_setopt(curl, CURLOPT_READDATA, this);
            //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, voice_writeData);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, this); 
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, DEFAULT_VREX_TIMEOUT);
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

            chunk = curl_slist_append(chunk, "Transfer-Encoding: chunked");
            chunk = curl_slist_append(chunk, "Content-Type:application/octet-stream");
            chunk = curl_slist_append(chunk, "Expect:");
            res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

            //Add the user agent field
            string userAgentData = getUserAgentString();
            __TIMESTAMP(); LOG("Speech USERAGENT: %s\n", userAgentData.c_str());
            curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgentData.c_str());

            /* Perform the request, res will get the return code */
            res = curl_easy_perform(curl);

            pthread_mutex_lock(&m_mutex);
            if (m_clientSocket >= 0) {
                __TIMESTAMP(); LOG("Deleting client socket from after curl_easy_perform\n");
                close(m_clientSocket);
            }
            m_clientSocket = -1;
            pthread_mutex_unlock(&m_mutex);

            curl_slist_free_all(chunk);

//            {            
//				time_t     now = time(0);
//				struct tm  tstruct;
//				char       buf[80];
//				tstruct = *localtime(&now);
//				// Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
//				// for more information about date/time format
//				strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);
//				
//			    FILE* pFile;
//			    string filename="vrexData";
//				filename+=buf;
//				filename+=".raw";
//			    pFile = fopen(filename.c_str(), "wb");
//				//Here would be some error handling
//				  for (list<char *>::iterator it=vrexData.begin(); it != vrexData.end(); ++it){
//					//Some calculations to fill a[]
//					short count=*(short *)(*it);
//				  	  char * mybuff=(*it);
//				  	  mybuff+=2;
//				       fwrite(mybuff, 1, count, pFile);
//				    }
//				    fclose(pFile);
//				
//            }
            
            /* Check for errors */
            if(res != CURLE_OK) {
                std::stringstream sstm;
                sstm << "cURL call failed (" << res << "): " << curl_easy_strerror(res);
                string errorMessage = sstm.str();
                //errorMessage = errorMessage + curl_easy_strerror(res);
                __TIMESTAMP(); LOG("speech(): %s\n", errorMessage.c_str());
               	//long httpErrorCode , long curlErrorCode, long vrexErrorCode ,long rfErrorCode
               notifyError(0, res,0,0, errorMessage.c_str(),SPEECH);

            }
            else {
            	try{
					JSONParser parser;
					m_speechResults = parser.parse((const unsigned char *)m_speechResponse.c_str());

					if (m_speechResults["code"] != NULL) {
						long responseCode=0;
						long returnCode=0;
						// Error in the service itself
						LOG("onSpeech response error!\n");
						LOG("onSpeech code: <%s>\n", m_speechResults["code"]->str.c_str());
						if (m_speechResults["message"]!=NULL){
							LOG("onSpeech message: <%s>\n", m_speechResults["message"]->str.c_str());
						}
						curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
                        returnCode = strtol(m_speechResults["code"]->str.c_str(), NULL, 10);
						if ((responseCode != 200) || (returnCode != 0)) {
							notifyError(responseCode, 0,returnCode,0, m_speechResults["message"]->str.c_str(),SPEECH);
						} else {
						    notifySuccess();
						}
					}
					else
						notifySuccess();
        		}catch (const std::exception err){
        			__TIMESTAMP(); LOG("exception in Speech response: %s",err.what());
        				__TIMESTAMP(); LOG("Speech response string for exception: %s",m_kkResponse.c_str());
        				std::stringstream sstm;
        				sstm << "Unknown exception:" << err.what() << ": speech response:" << m_speechResponse.c_str();
        			 string errorMessage = sstm.str();
        			 notifyError(0, 0,800,0, errorMessage.c_str(),SPEECH);
        		 }
            }

			if (m_logMetrics) {
				char buffer[700];
				getRequestMetrics(curl,buffer);
				LOG("%s",buffer);
			}

			/* always cleanup */
			curl_easy_cleanup(curl);

			close(m_serverSocket);
        }
    }

    close(socket_fd);
    unlink(socketName.c_str());

    return;
}

char * VREXSession::getRequestMetrics(CURL *curl,char *buffer)
{
    double total, connect, startTransfer, resolve, appConnect, preTransfer, redirect; 
    long responseCode,sslVerify;
    char *url;
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &url); 
    curl_easy_getinfo(curl, CURLINFO_NAMELOOKUP_TIME, &resolve);
    curl_easy_getinfo(curl, CURLINFO_CONNECT_TIME, &connect);
    curl_easy_getinfo(curl, CURLINFO_APPCONNECT_TIME, &appConnect);
    curl_easy_getinfo(curl, CURLINFO_PRETRANSFER_TIME, &preTransfer);
    curl_easy_getinfo(curl, CURLINFO_STARTTRANSFER_TIME, &startTransfer);
    curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME , &total);
    curl_easy_getinfo(curl, CURLINFO_REDIRECT_TIME, &redirect);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

    curl_easy_getinfo(curl, CURLINFO_SSL_VERIFYRESULT, &sslVerify);

    sprintf(buffer,"\nHttpRequestEnd %s code=%ld times={total=%g, connect=%g startTransfer=%g resolve=%g, appConnect=%g, preTransfer=%g, redirect=%g, sslVerify=%g}\n", url, responseCode, total, connect, startTransfer, resolve, appConnect, preTransfer, redirect,sslVerify);
    return buffer;
}

#ifdef RF4CE_GENMSO_API
void VREXSession::getStaticStbAndRemoteinfo(MSOBusAPI_RfStatus_t *rfStatus, int bindRemotesIndex)
{
    MSOBusAPI_BindRemote_t bindRemote;
#elif defined(RF4CE_API)
void VREXSession::getStaticStbAndRemoteinfo(rf4ce_RfStatus_t *rfStatus, int bindRemotesIndex)
{
    rf4ce_BindRemote_t bindRemote;
#elif defined(RF4CE_GPMSO_API)
void VREXSession::getStaticStbAndRemoteinfo(gpMSOBusAPI_RfStatus_t *rfStatus, int bindRemotesIndex)
{
    gpMSOBusAPI_BindRemote_t bindRemote;
#else
#warning "No RF4CE API defined"
#endif     

    char tempBuffer[TEMP_BUFFER_SIZE];

    if(rfStatus->bindRemotes[bindRemotesIndex].ShortAddress != 0xFFFF)
    {
        bindRemote = rfStatus->bindRemotes[bindRemotesIndex];

        //Remote type
        m_remoteType = bindRemote.Type;

        //Remote software version
        sprintf(tempBuffer, "%d.%d.%d.%d", bindRemote.VersionInfoSw[0], \
                                           bindRemote.VersionInfoSw[1], \
                                           bindRemote.VersionInfoSw[2], \
                                           bindRemote.VersionInfoSw[3]);
        m_remoteSoftwareVersion = tempBuffer;

        //Remote hardware version
        sprintf(tempBuffer, "%d.%d.%d.%d", bindRemote.VersionInfoHw.manufacturer, \
                                           bindRemote.VersionInfoHw.model, \
                                           bindRemote.VersionInfoHw.hwRevision, \
                                           bindRemote.VersionInfoHw.lotCode);
        m_remoteHardwareVersion = tempBuffer;

        //RF4CE mac address
        sprintf(tempBuffer, "0x%x%x%x%x%x%x%x%x", rfStatus->macAddress[7],rfStatus->macAddress[6],rfStatus->macAddress[5],rfStatus->macAddress[4],rfStatus->macAddress[3],rfStatus->macAddress[2],rfStatus->macAddress[1],rfStatus->macAddress[0]);
        m_rf4ceMacAddress = tempBuffer;
    }

#if 0 // Temporarily remove as STB_VERSION_STRING can't be retrieved in a Yocto build
    //RDK version
    m_rdkImage = STB_VERSION_STRING;
#endif
}

#if 0 // Temporarily remove this code as the values had been received from iarmStatus check which has been removed
void VREXSession::getDynamicStbAndRemoteinfo()
{
    char tempBuffer[TEMP_BUFFER_SIZE];

    //Remote battery voltage
    unsigned char batteryLevelLoaded = get_battery_level_loaded(m_remoteId);
    sprintf(tempBuffer, "%i.%iV", (UInt16)((batteryLevelLoaded >> 6) & 0x03), (UInt16)(((batteryLevelLoaded & 0x3F)*100) >> 6));
    m_remoteBatteryVoltage = tempBuffer;

    //IARM status kill count
    sprintf(tempBuffer, "%d", get_status_kill_count());
    m_iArmStatusKillCount = tempBuffer;
}
#endif

string VREXSession::getPostFieldString()
{
#if 0 // Temporarily remove this code as the values had been received from iarmStatus check which has been removed
    //Get the dynamic stb and remote info
    getDynamicStbAndRemoteinfo();

    string postData = "receiverId=" + m_receiverId + "&appId=" + m_appId + "&rf4ceMAC=" + m_rf4ceMacAddress + "&batt=" + m_remoteBatteryVoltage + "&iarmKill=" + m_iArmStatusKillCount;
#else
    string postData = "receiverId=" + m_receiverId + "&appId=" + m_appId + "&rf4ceMAC=" + m_rf4ceMacAddress;
#endif

    return postData;
}

string VREXSession::getUserAgentString()
{
#if 0 // Temporarily remove as STB_VERSION_STRING can't be retrieved in a Yocto build
    string userAgentData = "rdk=" + m_rdkImage + "; rmtType=" + m_remoteType + "; rmtSVer=" + m_remoteSoftwareVersion + "; rmtHVer=" + m_remoteHardwareVersion;
#else
    string userAgentData = "rmtType=" + m_remoteType + "; rmtSVer=" + m_remoteSoftwareVersion + "; rmtHVer=" + m_remoteHardwareVersion + "; stbName=" + m_stbName;
#endif

    return userAgentData;

}



/** @} */
/** @} */
