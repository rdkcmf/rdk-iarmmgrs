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
* @defgroup test
* @{
**/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "libIBus.h"
#include "vrexMgr.h"
#include <pthread.h>

#if !defined(MIN)
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#define DEFAULT_REMOTE_ID   12
#define DEFAULT_PCM_FILENAME "voice-cmd.raw"

static unsigned char remoteId = DEFAULT_REMOTE_ID;
static char currentCodec[] = "PCM_16_16K";
static pthread_mutex_t tMutexLock;

static void safe_copy(unsigned char *dst, const char *src, size_t len)
{
    if (len < 0) return;

    len = MIN(strlen(src), (len - 1));

    // Because glibc maintainers don't like strlcpy....
    *((char *) mempcpy (dst, src, len)) = '\0';

    printf("len=%i, dst=<%s>, src=<%s>\n", len, (char *)dst, src);
}

void send_file(const char *filename)
{
    IARM_Bus_VREXMgr_EventData_t eventData;

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) return;

    _SPEECH_EVENT *se = &eventData.data.speechEvent;
    int fragmentSizeRead = 0;
    int readBytes = sizeof(se->data.fragment.fragment);

    eventData.remoteId = remoteId;

    eventData.data.speechEvent.type = IARM_BUS_VREXMGR_SPEECH_FRAGMENT;

    while (!feof(fp)) {
        fragmentSizeRead = fread(&se->data.fragment.fragment[0], 1, readBytes, fp);
        se->data.fragment.length = fragmentSizeRead;

        printf("<<<<<<< Send Speech Fragment Event len: <%i>>>>>>>>>\n", fragmentSizeRead);
        IARM_Bus_BroadcastEvent(IARM_BUS_VREXMGR_NAME, (IARM_EventId_t)IARM_BUS_VREXMGR_EVENT_SPEECH,
            (void *)&eventData,sizeof(eventData));	
    }
    fclose(fp);
}

static void _eventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
	/*Handle only VREX Manager Events */
	if (strcmp(owner, IARM_BUS_VREXMGR_NAME)  == 0) 
	{
        _JSON_EVENT  *eEvent;

		//pthread_mutex_lock(&tMutexLock);

		IARM_Bus_VREXMgr_EventData_t *vrexEventData = (IARM_Bus_VREXMgr_EventData_t*)data;
        unsigned char remoteId = vrexEventData->remoteId;


        switch(eventId) {
            case IARM_BUS_VREXMGR_EVENT_ERROR:
            {
//                eEvent = (_ERROR_EVENT *)&vrexEventData->data.errorEvent;
//		        printf("\n_eventHandler Error Event remote=%d, responseCode=%ld, returnCode=%ld, message=%s\n", (int)vrexEventData->remoteId, eEvent->responseCode, eEvent->returnCode, eEvent->message);
            }
            break;
            default:
		        printf("\n_eventHandler unknown event type \n");
            break;
        }

		//pthread_mutex_unlock(&tMutexLock);
	}
    else {
        printf("_eventHandler event type not meant for me <%s>...\n", owner);
    }
}

int main(int argc, char *argv[])
{

    IARM_Result_t err;
    int input;
    char filename[] = DEFAULT_PCM_FILENAME;

    //pthread_mutex_init(&tMutexLock, NULL);
    //pthread_mutex_lock(&tMutexLock);

    do {
        printf("Tring to initialize IARM..\n");

        err = IARM_Bus_Init("vrexMgrTest");

        if(IARM_RESULT_SUCCESS != err)
        {
            printf("Error initialing IARM bus()... error code : %d\n",err);
            break;
        }

        printf("Trying to connect..\n");

        err = IARM_Bus_Connect();

        if(IARM_RESULT_SUCCESS != err)
        {
            printf("Error connecting to  IARM bus()... error code : %d\n",err);
            break;
        }
        IARM_Bus_RegisterEventHandler(IARM_BUS_VREXMGR_NAME, IARM_BUS_VREXMGR_EVENT_ERROR, _eventHandler);

    } while(0);
    //pthread_mutex_lock(&tMutexLock);

    do {
        printf("Enter command..\n");
        printf("m - send motion event\n");
        printf("v - send voice event(s)\n");
        printf("r - randomize remoteId \n");
        printf("c - switch codec\n");
        printf("x - exit..\n");
    
        input = getchar();

        switch(input)
        {
            case 'c':
            {
                if (currentCodec[0] == 'P')
                    strcpy(currentCodec, "ADPCM");
                else
                    strcpy(currentCodec, "PCM_16_16K");

                printf("Codec is now: %s\n", currentCodec);
            }
            break;
            case 'r':
            {
                remoteId = rand() % 16;
                printf("Remote ID is now: %i\n", (int)remoteId);
            }
            break;
            case 'm':
            {
                IARM_Bus_VREXMgr_EventData_t eventData;
                eventData.remoteId = remoteId;

                eventData.data.motionEvent.x = 128;
                eventData.data.motionEvent.y = 128;
                eventData.data.motionEvent.z = 128;

                printf("<<<<<<< Send Motion Event is >>>>>>>>\n");

                IARM_Bus_BroadcastEvent(IARM_BUS_VREXMGR_NAME, (IARM_EventId_t)IARM_BUS_VREXMGR_EVENT_MOTION,(void *)&eventData,sizeof(eventData));	

            }
            break;
            case 'v':
            {
                IARM_Bus_VREXMgr_EventData_t eventData;
                eventData.remoteId = remoteId;

                eventData.data.speechEvent.type = IARM_BUS_VREXMGR_SPEECH_BEGIN;
                safe_copy(eventData.data.speechEvent.data.begin.mimeType, "\"audio/vnd.wave;codec=1\"", sizeof(eventData.data.speechEvent.data.begin.mimeType));
                //safe_copy(eventData.data.speechEvent.data.begin.subType, "PCM_16_16K", sizeof(eventData.data.speechEvent.data.begin.subType));
                safe_copy(eventData.data.speechEvent.data.begin.subType, currentCodec, sizeof(eventData.data.speechEvent.data.begin.subType));
                safe_copy(eventData.data.speechEvent.data.begin.language, "en", sizeof(eventData.data.speechEvent.data.begin.language));
                printf("<<<<<<< Send Speech Begin Event >>>>>>>>\n");
                IARM_Bus_BroadcastEvent(IARM_BUS_VREXMGR_NAME, (IARM_EventId_t)IARM_BUS_VREXMGR_EVENT_SPEECH,(void *)&eventData,sizeof(eventData));	

                send_file(filename);

                printf("<<<<<<< Send Speech End Event >>>>>>>>\n");
                eventData.data.speechEvent.type = IARM_BUS_VREXMGR_SPEECH_END;
                eventData.data.speechEvent.data.end.reason = IARM_BUS_VREXMGR_SPEECH_DONE;
                //eventData.data.speechEvent.data.end.reason = IARM_BUS_VREXMGR_SPEECH_ABORT;
                IARM_Bus_BroadcastEvent(IARM_BUS_VREXMGR_NAME, (IARM_EventId_t)IARM_BUS_VREXMGR_EVENT_SPEECH,(void *)&eventData,sizeof(eventData));	
            }
            break;
        }
        getchar();
    } while(input != 'x');

    IARM_Bus_Disconnect();
    IARM_Bus_Term();
}


/** @} */
/** @} */
