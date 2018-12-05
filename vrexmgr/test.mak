##########################################################################
# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
#
# Copyright 2018 RDK Management
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
RM          := rm -rf
COMMON_FLAGS := -DSTANDALONE_TEST -g -fPIC -D_REENTRANT -Wall
CFLAGS      := $(COMMON_FLAGS)
CPPFLAGS    := $(COMMON_FLAGS)
EXECUTABLE  := testStatus
OBJS        := iarmStatus.o test/src/iarm_bus.o test/src/test_main.o
QUIET       := @

INCLUDE     =	-I. \
                -I./include \
                -I./test/include \
                -I./../../../rf4ce/include \
                -I./../../../iarmbus/core/include \
                -I./../hal/include

ifeq ($(RDK_LOGGER_ENABLED),y)
INCLUDE     += -I$(COMBINED_ROOT)/rdklogger/include
LDFLAGS     += -L$(COMBINED_ROOT)/rdklogger/build/lib -lrdkloggers
CFLAGS      += -DRDK_LOGGER_ENABLED
CPPFLAGS    += -DRDK_LOGGER_ENABLED
endif

CFLAGS      += $(INCLUDE)
CPPFLAGS    += $(INCLUDE)
FUSION_LIBS  = -pthread
LDFLAGS     += $(FUSION_LIBS)

.PHONY: clean

all: $(EXECUTABLE)
	@echo "Build Finished...."

$(EXECUTABLE): $(OBJS)
	@echo "Creating Final Executable ..$@ "
	$(QUIET)$(CXX) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@

%.o: %.c
	@echo "Building $@ ...."
	$(QUIET)$(CXX) -c $<  $(CFLAGS) -o $@
clean:
	@echo "Cleaning the directory..."
	@$(RM) $(OBJS) $(LIBNAMEFULL) $(EXECUTABLE)
