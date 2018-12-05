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
#include "resetModes.h"
#include "pwrlogger.h"


int processColdFactoryReset()
{ 
    /*Code copied from X1.. Needs modification*/
    LOG("\n Reset: Processing Cold Factory Reset\n");
    fflush(stdout);
    LOG(" Reset: ...Clearing data from your box before reseting \n");
    fflush(stdout);
    /*Execute the script for Cold Factory Reset*/
    system("sh /lib/rdk/deviceReset.sh coldfactory");

    system("echo 0 > /opt/.rebootFlag");
    system(" echo `/bin/timestamp` ------------- Rebooting due to Cold Factory Reset process --------------- >> /opt/logs/receiver.log");
    system("sleep 5; /rebootNow.sh -s PowerMgr_coldFactoryReset -o 'Rebooting the box due to Cold Factory Reset process ...'");
    return 1;
}


/** @} */
/** @} */
