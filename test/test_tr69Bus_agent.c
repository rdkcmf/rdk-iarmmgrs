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


#include "tr69BusMgr.h"
#include "libIARMCore.h"
#include <stdio.h>
#include <string.h>

#define TEST_PARAM_NAME "test_param"

static char test_param[20];

IARM_Result_t getSetParams(void *arg)
{
	IARM_Result_t ret =  IARM_RESULT_IPCCORE_FAIL;
	IARM_Bus_TR69_BUS_AgentRequestInfo_Param_t *pParam = (IARM_Bus_TR69_BUS_AgentRequestInfo_Param_t *)arg;

	if(!pParam)
		return ret;

	if (pParam->mode == IARM_Bus_TR69_BUS_MODE_Get)
	{
		printf("%s: Agent in Get mode .. \n", __FUNCTION__);
		if(pParam->paramName)
		{
			printf("%s: Parameter name = %s \n", __FUNCTION__, pParam->paramName);
			if(strcmp(pParam->paramName,TEST_PARAM_NAME) == 0)
			{
				pParam->paramLen = strlen(test_param) + 1;
				strcpy((char *)pParam->paramValue,test_param);
				pParam->paramLen = strlen(test_param);
				printf("Returning param value : %s (%d bytes)\n",(char *)pParam->paramValue, pParam->paramLen);
				ret = IARM_RESULT_SUCCESS;
			}
			else
			{
				pParam->err_no = IARM_Bus_TR69_BUS_ERROR_NO_SUCH_PARAM;
			}
		}
		else
		{
			printf("%s: Parameter name is NULL \n", __FUNCTION__);
		}
	}
	else if (pParam->mode == IARM_Bus_TR69_BUS_MODE_Set)
	{
		printf("%s: Agent in Set mode \n", __FUNCTION__);
		if(pParam->paramName)
		{
			printf("%s: Parameter name = %s \n", __FUNCTION__, pParam->paramName);
			if(strcmp(pParam->paramName,TEST_PARAM_NAME) == 0)
			{
				strcpy(test_param,(const char*)pParam->paramValue); /*In test app, we are assuming safe bounds for data*/
				ret = IARM_RESULT_SUCCESS;
			}
			else
			{
				pParam->err_no = IARM_Bus_TR69_BUS_ERROR_NO_SUCH_PARAM;
			}
		}
		else
		{
			printf("%s: Parameter name is NULL \n", __FUNCTION__);
		}
	}
	return ret;
}

int main()
{

	IARM_Result_t ret ;

	IARM_Bus_TR69_BUS_UnRegisterAgent_Param_t unRegisterAgentParam;

	printf("Enter tr69 Agent test app..\n");

	strcpy(test_param, "test_string");

	do
	{
		IARM_Bus_TR69_BUS_RegisterAgent_Param_t registerAgentParam;

		ret = IARM_Bus_Init("tr69TestAgent");
		if(ret != IARM_RESULT_SUCCESS)
		{
			printf("Error initilaizing IARM Bus for test app..\n");
			break;
		}
		ret = IARM_Bus_Connect();
		if(ret != IARM_RESULT_SUCCESS)
		{
			printf("Error connecting to  IARM Bus for test app..\n");
			break;
		}
		ret = IARM_Bus_RegisterCall(IARM_BUS_TR69_COMMON_API_AgentParameterHandler, getSetParams);
		if(ret != IARM_RESULT_SUCCESS)
		{
			printf("Error registering call getParam for test app..\n");
			break;
		}

		strcpy(registerAgentParam.agentName,"tr69TestAgent");
		ret = IARM_Bus_Call(IARM_BUS_TR69_BUS_MGR_NAME,IARM_BUS_TR69_BUS_MGR_API_RegisterAgent,&registerAgentParam,sizeof(registerAgentParam));

		if(ret != IARM_RESULT_SUCCESS)
		{
			printf("Error registering tr69 test app to tr69 bus..\n");
			break;
		}

	} while(0);

	if(ret == IARM_RESULT_SUCCESS)
	{
		printf("Press 'x' to exit from the agent..\ntr69TestClient could be tested now..\n");
		while(getchar() != 'x');
	}

	ret = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, strlen("tr69TestAgent") + 1, (void **)&unRegisterAgentParam.agentName);
	if(ret != IARM_RESULT_SUCCESS)
	{
		printf("Error allocating shared mem for unregistering test app..\n");
	}

	strcpy(unRegisterAgentParam.agentName,"tr69TestAgent");
	ret = IARM_Bus_Call(IARM_BUS_TR69_BUS_MGR_NAME,IARM_BUS_TR69_BUS_MGR_API_UnRegisterAgent,&unRegisterAgentParam,sizeof(unRegisterAgentParam));

	if(ret != IARM_RESULT_SUCCESS)
	{
		printf("Error unregistering from TR69 Bus..\n");
	}
	else
	{
		printf("Successfully unregistered from TR69 Bus\n");
	}

	IARM_Bus_Disconnect();
	IARM_Bus_Term();

	return 0;

}




/** @} */
/** @} */
