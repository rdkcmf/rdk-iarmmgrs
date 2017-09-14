##########################################################################
# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
#
# Copyright 2016 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################
#

RM          := rm -rf
CFLAGS      := -g -fPIC -D_REENTRANT -Wall
LIBNAME     := MAF
LIBNAMEFULL := lib$(LIBNAME).so
CPPLAGS     := -g -fPIC -D_REENTRANT -Wall
EXECUTABLE  := mfrMgrMain

#Plase make setting NEXUS_APP_INCLUDE_PATHS configuratble
ifeq ($(RDK_PLATFORM_SOC), broadcom)
ifeq ($(BUILD_13_2_OR_LATER),y)
include $(NEXUS_BIN_DIR)/include/platform_app.inc
else
include $(NEXUS_TOP)/bin/include/platform_app.inc
endif
#CFLAGS +=-DRDK_MFRLIB_NAME='"libPaceMfrApi.so"'
    CFLAGS +=-DRDK_MFRLIB_NAME=$(RDK_MFRLIB_NAME)
    LDFLAGS += -lssl
    ifeq ($(RDK_USE_NXCLIENT),y)
	LDFLAGS += -lnxclient 
    endif 
LDFLAGS+=-Wl,-rpath,$(SDK_FSROOT)/lib
endif
#Plase make setting NEXUS_ENDIAN configuratble
NEXUS_ENDIAN=BSTD_ENDIAN_LITTLE
#Plase make setting LINUXKDIR configuratble
LINUXKDIR=BCM_NEXUS_LINUX_74XX


INCLUDE     =	-I.\
                -I./include \
		-I$(MFR_PATH)/include	\
		-I$(IARM_PATH)/core/   \
		-I$(IARM_PATH)/core/include \
		-I$(DFB_ROOT)/usr/local/include/directfb \

IARM_NEXUS_INCLUDE     =-I$(LINUXKDIR)/include \
			$(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES))

NEXUS_BUILD_CFLAGS+=-DBSTD_CPU_ENDIAN=${NEXUS_ENDIAN}

ifeq ($(RDK_LOGGER_ENABLED),y)
INCLUDE     += -I$(COMBINED_ROOT)/rdklogger/include
LDFLAGS     += -L$(COMBINED_ROOT)/rdklogger/build/lib -lrdkloggers
CFLAGS      += -DRDK_LOGGER_ENABLED
CPPLAGS     += -DRDK_LOGGER_ENABLED
endif

CFLAGS      += $(INCLUDE) 


CPPLAGS      += ${NEXUS_BUILD_CFLAGS}
CPPLAGS      += ${IARM_NEXUS_INCLUDE}


CROSS_COMPILE=mipsel-linux-
CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++

LDFLAGS +=-Wl,-rpath,$(SDK_FSROOT)/usr/local/lib
LDFLAGS += -L$(SDK_FSROOT)/usr/local/lib -lIARMBus -lnexus

LDFLAGS += -L${WORK_DIR}/${MFR_LIB_PATH} $(MFR_LIB_NAME)
LDFLAGS += -L$(OPENSSL_PATH)/lib -lcrypto -lrt 

ifeq ($(COMCAST_PLATFORM), XI3)
	LDFLAGS += -L$(BUILDS_DIR)/workXI3/rootfs/usr/local/lib
else
       LDFLAGS += -L$(BUILDS_DIR)/workXG1/rootfs/usr/local/lib
endif

all:clean executable
	@echo "Build Finished...."

library: mfrMgr.o 
	@echo "Building $(LIBNAMEFULL) ...."
ifeq ($(RDK_PLATFORM_SOC), broadcom)
	$(CC) $(CFLAGS) $(LDFLAGS) -shared mfrMgr.o -o $(LIBNAMEFULL) 
endif
ifeq ($(RDK_PLATFORM_SOC), rpi)
	$(CC) $(CFLAGS) $(LDFLAGS) -shared mfrMgr.o -o $(LIBNAMEFULL) 
endif
ifeq ($(COMCAST_PLATFORM), XI4)
	$(CC) $(CFLAGS) $(LDFLAGS) -shared mfrMgr.o -o $(LIBNAMEFULL) 
endif

executable: mfrMgrMain.o mfrMgr.o
	@echo "Building $(EXECUTABLE) ...."
ifeq ($(RDK_PLATFORM_SOC), broadcom)
	$(CC) $(CFLAGS) $(LDFLAGS) mfrMgr.o mfrMgrMain.o -o $(EXECUTABLE) 
endif	
ifeq ($(RDK_PLATFORM_SOC), rpi)
	$(CC) $(CFLAGS) $(LDFLAGS) mfrMgr.o mfrMgrMain.o -o $(EXECUTABLE) 
endif	
ifeq ($(COMCAST_PLATFORM), XI4)
	$(CC) $(CFLAGS) $(LDFLAGS) mfrMgr.o mfrMgrMain.o -o $(EXECUTABLE) 
endif	

%.o: %.c
	@echo "Building $@ ...."
ifeq ($(RDK_PLATFORM_SOC), broadcom)
	$(CC) -c $<  $(CFLAGS) -o $@
endif
ifeq ($(RDK_PLATFORM_SOC), rpi)
	$(CC) -c $<  $(CFLAGS) -o $@
endif
ifeq ($(COMCAST_PLATFORM), XI4)
	$(CC) -c $<  $(CFLAGS) -o $@
endif

clean:
	@echo "Cleaning the directory..."
	@$(RM) *.o $(LIBNAMEFULL) 

