/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2017 RDK Management
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
/*****************************************************************************
 *
 *   Plat-ir.c
 *
 *   Description: RDK emulator ir implementation
 *
 *   Author: Ridish Rema Aravindan <ridish.ra@lnttechservices.com>
 *
 *   Date:   06/17/2014
 *
 *****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "comcastIrKeyCodes.h"
#include "plat_ir.h"

#ifdef DEBUG_PLAT
#define DEBUG_MSG(x,y...) printf(x,##y)
#else
#define DEBUG_MSG(x,y...) {;}
#endif

#define BUFSIZE 1024

unsigned long int btn[]={
        KED_TVVCR,
        KED_POWER,
        KED_VOLUMEUP,
        KED_VOLUMEDOWN,
        KED_MUTE,
        KET_KEYREPEAT,
        KED_CHANNELUP,
        KED_CHANNELDOWN,
        KED_REWIND,
        KED_PLAY,/*Play or Pause*/
        KED_FASTFORWARD,
        KED_EXIT,
        KED_MENU,
        KED_RECORD,
        KED_GUIDE,
        KED_PAGEUP,
        KED_ARROWLEFT,
        KED_ARROWUP,
        KED_SELECT,
        KED_ARROWDOWN,
        KED_ARROWRIGHT,
        KED_LAST,
        KED_INFO,
        KED_PAGEDOWN,
        KED_KEYA,
        KED_KEYB,
        KED_KEYC,
        KED_KEYD,
        KED_DIGIT1,
        KED_DIGIT2,
        KED_DIGIT3,
        KED_DIGIT4,
        KED_DIGIT5,
        KED_DIGIT6,
        KED_DIGIT7,
        KED_DIGIT8,
        KED_DIGIT9,
        KED_DISPLAY_SWAP,
        KED_DIGIT0
};

//Platform specific control block.
typedef struct _PlatformData
{
   int                   repeats;   //repeat counter.
   int                   last_key;  //Last key cache.
   PLAT_IrKeyCallback_t  key_cb;    //Call back function when key is received.
} PlatformData;

static PlatformData  Platform_Data;

void error(char *msg)
{
	perror(msg);
	exit(1);
}


/**
* @brief Register callback function to which IR Key events should be posted.
*
* This function registers the calling applications callback function.  The application
* will then be notified of IR Key events via this callback function.
*
* @param [in]  func    Function reference of the callback function to be registered.
* @return None.
*/
void PLAT_API_RegisterIRKeyCallback(PLAT_IrKeyCallback_t func)
{
   Platform_Data.key_cb = func;
}

/**
 * @brief Initialize the underlying IR module.
 *
 * This function must initialize all the IR specific user input device modules.
 *
 * @param     None.
 * @return    Return Code.
 * @retval    0 if successful.
 */
int  PLAT_API_INIT(void)
{
   DEBUG_MSG("<%s, %d>** DEBUG iarmMgr/ir -- %s()\n",__FILE__,__LINE__,__FUNCTION__);
   return 0;
}

/**
 * @brief Close the IR device module.
 *
 * This function must terminate all the IR specific user input device modules. It must
 * reset any data structures used within IR module and release any IR specific handles
 * and resources.
 *
 * @param None.
 * @return None.
 */
void PLAT_API_TERM(void)
{
    DEBUG_MSG("<%s, %d>** DEBUG iarmMgr/ir -- %s()\n",__FILE__,__LINE__,__FUNCTION__);
}

/**
 * @brief Execute key event loop.
 *
 * This function executes the platform-specific key event loop. This will generally
 * translate between platform-specific key codes and Comcast standard keycode definitions.
 *
 * @param None.
 * @return None.
 */
void PLAT_API_LOOP()
{
	int IARM_fd;
	int IARM_portno = 3434;
	int IARM_client_len; 
	struct sockaddr_in EMU_addr;
	struct sockaddr_in remote_addr; 
	struct hostent *remote_machine;
	char KeyEvents[BUFSIZE];
	int Flags; 
	int n;
	unsigned long int key;

	IARM_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (IARM_fd < 0)
		error("IARM ERROR opening socket");

	Flags = 1;
	setsockopt(IARM_fd, SOL_SOCKET, SO_REUSEADDR,
			(const void *)&Flags , sizeof(int));
	bzero((char *) &EMU_addr, sizeof(EMU_addr));
	EMU_addr.sin_family = AF_INET;
	EMU_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	EMU_addr.sin_port = htons((unsigned short)IARM_portno);

	if (bind(IARM_fd, (struct sockaddr *) &EMU_addr,
				sizeof(EMU_addr)) < 0)
		error("IARM ERROR on binding");

	IARM_client_len = sizeof(remote_addr);
	while (1) {

		bzero(KeyEvents, BUFSIZE);
		n = recvfrom(IARM_fd, KeyEvents, BUFSIZE, 0,
				(struct sockaddr *) &remote_addr, (socklen_t*)&IARM_client_len);
		if (n < 0)
			error("ERROR in recvfrom");
		printf(" EMUserver received %d/%d bytes: %d\n", strlen(KeyEvents), n, atoi(KeyEvents));
		key = atoi(KeyEvents);
		printf("COMCAST Key code is %ld\n", btn[key]);
		
		Platform_Data.key_cb(KET_KEYDOWN, btn[key]);
		// Calling this function yet again to simulate 
		// KEY_RELEASE
		Platform_Data.key_cb(KET_KEYUP, btn[key]);
	}
}
