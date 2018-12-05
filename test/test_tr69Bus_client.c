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
#define TEST_PARAM_DATA "hello world"

int main()
{

	IARM_Result_t ret ;
	char *pAgentName = NULL;
	char *pParamName = NULL;
	char *pParamValue = NULL;

	printf("Enter tr69 client test app..\n");

	do
	{
		IARM_Bus_TR69_BUS_AgentRequestInfo_Param_t getParam;
		IARM_Bus_TR69_BUS_AgentRequestInfo_Param_t setParam;

		ret = IARM_Bus_Init("tr69TestClient");
		if(ret != IARM_RESULT_SUCCESS)
		{
			printf("Error initilaizing IARM Bus for test app (client) ..\n");
			break;
		}
		ret = IARM_Bus_Connect();
		if(ret != IARM_RESULT_SUCCESS)
		{
			printf("Error connecting to  IARM Bus for test app..\n");
			break;
		}
		ret = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, strlen("tr69TestAgent") + 1, (void **)&pAgentName);
		if(ret != IARM_RESULT_SUCCESS)
		{
			printf("Error allocating shared mem for test agent name..\n");
			break;
		}
		strcpy(pAgentName,"tr69TestAgent");

		ret = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, strlen(TEST_PARAM_NAME) + 1, (void **)&pParamName);
		if(ret != IARM_RESULT_SUCCESS)
		{
			printf("Error allocating shared mem for test parameter name..\n");
			break;
		}

		strcpy(pParamName,TEST_PARAM_NAME);

		strcpy(getParam.agentName, pAgentName);
		strcpy(getParam.paramName, pParamName);
		getParam.mode = IARM_Bus_TR69_BUS_MODE_Get;
		getParam.paramType = IARM_Bus_TR69_BUS_TYPE_String;

		ret = IARM_Bus_Call(IARM_BUS_TR69_BUS_MGR_NAME, IARM_BUS_TR69_BUS_MGR_API_ParameterHandler,&getParam,sizeof(getParam));

		if(ret != IARM_RESULT_SUCCESS)
		{
			printf("Error executing Get parameter call from test client, error code: %d\n",getParam.err_no);
			break;
		}

		printf("Test agent(%s) returned param (%s) as -> %s(%d bytes)",pAgentName,pParamName,(char *)getParam.paramValue,getParam.paramLen);

		ret = IARM_Malloc(IARM_MEMTYPE_PROCESSLOCAL, strlen(TEST_PARAM_DATA) + 1, (void **)&pParamValue);
		if(ret != IARM_RESULT_SUCCESS)
		{
			printf("Error allocating shared mem for data..\n");
			break;
		}

		strcpy(pParamValue, TEST_PARAM_DATA);

		strcpy(setParam.agentName, pAgentName);
		strcpy(setParam.paramName, pParamName);
		strcpy(setParam.paramValue,pParamValue);
		setParam.paramLen = strlen(TEST_PARAM_DATA) + 1;
		setParam.mode = IARM_Bus_TR69_BUS_MODE_Set;
		setParam.paramType = IARM_Bus_TR69_BUS_TYPE_String;

		printf("Setting the parameter %s on the agent %s as %s\n", pParamName, pAgentName, pParamValue);

		ret = IARM_Bus_Call(IARM_BUS_TR69_BUS_MGR_NAME, IARM_BUS_TR69_BUS_MGR_API_ParameterHandler, &setParam,sizeof(setParam));

		if(ret != IARM_RESULT_SUCCESS)
		{
			printf("Error executing Set parameter call from test client, error code: %d\n",getParam.err_no);
			break;
		}

		IARM_Free(IARM_MEMTYPE_PROCESSLOCAL, pParamValue);

		printf("Trying to get parameter %s from agent %s again..\n",pParamName,pAgentName);

		ret = IARM_Bus_Call(IARM_BUS_TR69_BUS_MGR_NAME, IARM_BUS_TR69_BUS_MGR_API_ParameterHandler,&getParam,sizeof(getParam));

		if(ret != IARM_RESULT_SUCCESS)
		{
			printf("Error executing Get parameter call from test client, error code: %d\n",getParam.err_no);
			break;
		}
		printf("Test agent(%s) returned param (%s) as -> %s(%d bytes)",pAgentName,pParamName,(char *)getParam.paramValue,getParam.paramLen);

	}while(0);

	IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,pParamName);
	IARM_Free(IARM_MEMTYPE_PROCESSLOCAL,pAgentName);

	IARM_Bus_Disconnect();
	IARM_Bus_Term();
	printf("Exiting test client.. press any key to continue..\n");

	getchar();
} 


/** @} */
/** @} */
