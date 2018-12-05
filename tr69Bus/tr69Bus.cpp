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
* @defgroup tr69Bus
* @{
**/


#include "stdio.h"
#include "memory.h"
#include "tr69BusMgr.h"
#include "libIARMCore.h"
#include "tr69BusInternal.h"
#include <time.h>
#include <unistd.h>
#include <list>
#include <string>
#include <algorithm>
#include <pthread.h>

using namespace std;

static int is_connected = 0;

static list <string> registered_agents;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

IARM_Result_t registerAgent(void *arg)
{
    IARM_Result_t ret = IARM_RESULT_SUCCESS;

    IARM_Bus_TR69_BUS_RegisterAgent_Param_t *param = (IARM_Bus_TR69_BUS_RegisterAgent_Param_t *) arg;

    pthread_mutex_lock(&mutex);
    param->status = 0;

    string *agent_name = new string(param->agentName);

    if(find(registered_agents.begin(), registered_agents.end(), *agent_name) != registered_agents.end())
    {
        LOG("%s Already registered\n",__FUNCTION__,agent_name->c_str());
        param->status = 1;
    }
    else
    {
        registered_agents.push_back(*agent_name);
        LOG("[%s]  Registered agent: %s  \n",__FUNCTION__,agent_name->c_str());
        param->status = 1;
    }

    LOG("%s: Successfully registered agent %s\n",__FUNCTION__,agent_name->c_str());

    pthread_mutex_unlock(&mutex);

    delete agent_name;
    return ret;
}

IARM_Result_t unRegisterAgent(void *arg)
{
    IARM_Result_t ret = IARM_RESULT_SUCCESS;

    IARM_Bus_TR69_BUS_UnRegisterAgent_Param_t *param = (IARM_Bus_TR69_BUS_UnRegisterAgent_Param_t *) arg;
    list <string>::iterator ii;

    pthread_mutex_lock(&mutex);

    string *agent_name = new string(param->agentName);

    ii = find(registered_agents.begin(), registered_agents.end(), *agent_name);

    if(ii == registered_agents.end())
    {
        LOG("%s failed: Agent %s is not registered\n",__FUNCTION__,agent_name->c_str());
        ret = IARM_RESULT_IPCCORE_FAIL;
    }
    else
    {
        registered_agents.erase(ii);
        LOG("[%s]  Unregistered agent: %s \n",__FUNCTION__,agent_name->c_str());
    }

    LOG("%s: Successfully unregistered agent %s\n",__FUNCTION__,agent_name->c_str());
    pthread_mutex_unlock(&mutex);

    delete agent_name;
    return ret;
}

IARM_Result_t parameterHandler(void *arg)
{
    IARM_Result_t ret = IARM_RESULT_SUCCESS;

    IARM_Bus_TR69_BUS_AgentRequestInfo_Param_t *param = (IARM_Bus_TR69_BUS_AgentRequestInfo_Param_t*) arg;

    pthread_mutex_lock(&mutex);

    string *agent_name = new string(param->agentName);

    if(find(registered_agents.begin(), registered_agents.end(), *agent_name) == registered_agents.end())
    {
        pthread_mutex_unlock(&mutex);

        LOG("%s failed: Agent %s not registered\n",__FUNCTION__,agent_name->c_str());

        param->err_no = IARM_Bus_TR69_BUS_ERROR_NO_SUCH_AGENT;

        ret = IARM_RESULT_IPCCORE_FAIL;
    }
    else
    {
        int is_registered = 0;

        pthread_mutex_unlock(&mutex);

        ret = IARM_IsCallRegistered(agent_name->c_str(), IARM_BUS_TR69_COMMON_API_AgentParameterHandler, &is_registered);

        if(IARM_RESULT_SUCCESS == ret && is_registered)
        {
            IARM_Bus_TR69_BUS_AgentRequestInfo_Param_t agent_param;

            strcpy(agent_param.agentName, param->agentName);  // Unused
            strcpy(agent_param.paramName, param->paramName);
            agent_param.paramType = param->paramType;
            agent_param.mode = param->mode;
            agent_param.instanceNumber = param->instanceNumber;

            if (agent_param.mode == IARM_Bus_TR69_BUS_MODE_Get)
            {
                //LOG("%s: Get operation requested... \n", __FUNCTION__);
                memset(agent_param.paramValue, '\0', BUF_MAX);
                agent_param.paramLen = 0;
            }
            else if (agent_param.mode == IARM_Bus_TR69_BUS_MODE_Set)
            {
                //LOG("%s: Set operation requested... \n", __FUNCTION__);
                //strcpy(agent_param.paramValue, param->paramValue);
                memcpy(agent_param.paramValue, param->paramValue, BUF_MAX);
                agent_param.paramLen = param->paramLen;
            }
            else
            {
                LOG("[%s:%s:%d] Invalid operation mode = %d\n",__FILE__,  __FUNCTION__, __LINE__, agent_param.mode);
                agent_param.err_no = IARM_Bus_TR69_BUS_ERROR_UNSUPPORTED_MODE;
                return IARM_RESULT_IPCCORE_FAIL;
            }

            if(IARM_RESULT_SUCCESS == ret)
            {
                agent_param.err_no = IARM_Bus_TR69_BUS_SUCCESS;

                ret = IARM_Bus_Call(agent_name->c_str(), IARM_BUS_TR69_COMMON_API_AgentParameterHandler,&agent_param,sizeof(agent_param));

                if(ret != IARM_RESULT_SUCCESS)
                {
                    switch(agent_param.err_no)
                    {
                    case IARM_Bus_TR69_BUS_SUCCESS:  /*IARM call to subagent failed here */
                        param->err_no = IARM_Bus_TR69_BUS_ERROR_GENERAL_ERROR;
                        break;

                        /*handle other cases like default actions (if not handled by sub-agent) here*/
                    case IARM_Bus_TR69_BUS_ERROR_NO_SUCH_PARAM:
                    default:
                        param->err_no = agent_param.err_no;
                    }
                    LOG("[%s:%d] Failed IARM_Bus_Call for RPC: \'%s\' of request mode (%d) with return: [%d] \n", 
                                      __FUNCTION__, __LINE__, IARM_BUS_TR69_COMMON_API_AgentParameterHandler, agent_param.mode,  ret);
                }
                else
                {
                    //strncpy(param->paramValue, agent_param.paramValue, BUF_MAX);
                    memcpy(param->paramValue, agent_param.paramValue, BUF_MAX);
                    param->paramLen = agent_param.paramLen;
                }
            }
        }
        else
        {
            LOG("[%s:%s:%d] Failed: Call %s is not registered on %s \n", __FILE__, __FUNCTION__, __LINE__, IARM_BUS_TR69_COMMON_API_AgentParameterHandler, agent_name->c_str());

            ret = IARM_RESULT_IPCCORE_FAIL;

            param->err_no = IARM_Bus_TR69_BUS_ERROR_NO_SUCH_METHOD;
        }
    }

    delete agent_name;
    return ret;
}

IARM_Result_t TR69Bus_Start(void)
{
    IARM_Result_t err = IARM_RESULT_IPCCORE_FAIL;

    LOG("Entering [%s] - [%s] - disabling io redirect buf\r\n", __FUNCTION__, IARM_BUS_TR69_BUS_MGR_NAME);
    setvbuf(stdout, NULL, _IOLBF, 0);


    do {
        /* Please do any platform init here*/
        err = IARM_Bus_Init(IARM_BUS_TR69_BUS_MGR_NAME);

        if(IARM_RESULT_SUCCESS != err)
        {
            LOG("Error initializing TR69 Bus.. error code : %d\n",err);
            break;
        }

        err = IARM_Bus_Connect();

        if(IARM_RESULT_SUCCESS != err)
        {
            LOG("Error connecting to IARM.. error code : %d\n",err);
            break;
        }

        is_connected = 1;

        err = IARM_Bus_RegisterCall(IARM_BUS_TR69_BUS_MGR_API_RegisterAgent,registerAgent);

        if(IARM_RESULT_SUCCESS != err)
        {
            LOG("Error registering call(registerAgent) in IARM.. error code : %d\n",err);
            break;
        }

        err = IARM_Bus_RegisterCall(IARM_BUS_TR69_BUS_MGR_API_UnRegisterAgent,unRegisterAgent);

        if(IARM_RESULT_SUCCESS != err)
        {
            LOG("Error registering call(unRegisterAgent) in IARM.. error code : %d\n",err);
            break;
        }

        err = IARM_Bus_RegisterCall(IARM_BUS_TR69_BUS_MGR_API_ParameterHandler, parameterHandler);

        if(IARM_RESULT_SUCCESS != err)
        {
            LOG("Error registering call (parameterHandler) in IARM.. error code : %d\n",err);
            break;
        }

    } while(0);

    if(err != IARM_RESULT_SUCCESS)
    {
        if(is_connected)
        {
            IARM_Bus_Disconnect();
        }
        IARM_Bus_Term();
    }

    return err;
}

IARM_Result_t TR69Bus_Stop(void)
{
    if(is_connected)
    {
        LOG("I-ARM TR69 Bus: Stopped \n");
        IARM_Bus_Disconnect();
        IARM_Bus_Term();
    }
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t TR69Bus_Loop()
{
    time_t curr = 0;
    while(1)
    {
        time(&curr);
        LOG("I-ARM TR69 Bus: HeartBeat at %s\r\n", ctime(&curr));
        sleep(600);
    }
    return IARM_RESULT_SUCCESS;
}


/** @} */
/** @} */
