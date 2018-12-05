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
#include "libIBusDaemon.h"
#include "pwrMgr.h"
#include "irMgr.h"


static IARM_Result_t _ReleaseOwnership(void *arg)
{
	printf("############### Bus Client _ReleaseOwnership, CLIENT releasing stuff\r\n");

    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    return retCode;
}

static void _eventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
	if (strcmp(owner, IARM_BUS_PWRMGR_NAME)  == 0) {
		switch (eventId) {
		case IARM_BUS_PWRMGR_EVENT_MODECHANGED:
		{
			IARM_Bus_PWRMgr_EventData_t *param = (IARM_Bus_PWRMgr_EventData_t *)data;
			printf("Event IARM_BUS_PWRMGR_EVENT_MODECHANGED: State Changed %d -- > %d\r\n",
					param->data.state.curState, param->data.state.newState);
		}
			break;
		default:
			break;
		}
	}
	else if (strcmp(owner, IARM_BUS_IRMGR_NAME)  == 0) {
		switch (eventId) {
		case IARM_BUS_IRMGR_EVENT_IRKEY:
		{
			IARM_Bus_IRMgr_EventData_t *irEventData = (IARM_Bus_IRMgr_EventData_t*)data;
			int keyCode = irEventData->data.irkey.keyCode;
			int keyType = irEventData->data.irkey.keyType;
			printf("Test Bus Client Get IR Key (%x, %x) From IR Manager\r\n", keyCode, keyType);
		}
			break;
		default:
			break;
		}

	}
}

int main()
{
	IARM_Result_t retCode = IARM_RESULT_SUCCESS;
	printf("Client Entering %d\r\n", getpid());
	IARM_Bus_Init("Bus Client");
	IARM_Bus_Connect();

	IARM_Bus_RegisterCall(IARM_BUS_COMMON_API_ReleaseOwnership, _ReleaseOwnership);
    IARM_Bus_RegisterEventHandler(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_EVENT_MODECHANGED, _eventHandler);
	retCode = IARM_BusDaemon_RequestOwnership(IARM_BUS_RESOURCE_FOCUS);

	int i = 0;
	while(i < 5) {
		i++;
		printf("HeartBeat of Bus Client\r\n");
        if (i % 2 == 1) {
            printf("Register IR for Bus Client\r\n");
            IARM_Bus_RegisterEventHandler(IARM_BUS_IRMGR_NAME, IARM_BUS_IRMGR_EVENT_IRKEY, _eventHandler);
        }
        else {
            printf("Unregister IR for Bus Client\r\n");
            IARM_Bus_UnRegisterEventHandler(IARM_BUS_IRMGR_NAME, IARM_BUS_IRMGR_EVENT_IRKEY);
        }
		sleep(5);
	}
	retCode = IARM_BusDaemon_ReleaseOwnership(IARM_BUS_RESOURCE_FOCUS);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_EVENT_MODECHANGED);
	IARM_Bus_Disconnect();
	IARM_Bus_Term();
	printf("Bus Client Exiting\r\n");
}


/** @} */
/** @} */
