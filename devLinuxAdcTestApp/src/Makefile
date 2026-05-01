# SPDX-FileCopyrightText: 2003 Argonne National Laboratory
#
# SPDX-License-Identifier: EPICS

TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application

PROD_IOC_Linux = devLinuxAdcTest

# devLinuxAdcTest.dbd will be created and installed
DBD += devLinuxAdcTest.dbd

# devLinuxAdcTest.dbd will be made up from these files:
devLinuxAdcTest_DBD += base.dbd

# Include dbd files from all support applications:
devLinuxAdcTest_DBD += devLinuxAdc.dbd
devLinuxAdcTest_DBD += PVAServerRegister.dbd
devLinuxAdcTest_DBD += qsrv.dbd

# Add all the support libraries needed by this IOC
#devLinuxAdcTest_LIBS += xxx

# devLinuxAdcTest_registerRecordDeviceDriver.cpp derives from devLinuxAdcTest.dbd
devLinuxAdcTest_SRCS += devLinuxAdcTest_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
devLinuxAdcTest_SRCS_DEFAULT += devLinuxAdcTestMain.cpp
devLinuxAdcTest_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#devLinuxAdcTest_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

devLinuxAdcTest_LIBS += devLinuxAdc
devLinuxAdcTest_LIBS += qsrv

# Finally link to the EPICS Base libraries
devLinuxAdcTest_LIBS += $(EPICS_BASE_PVA_CORE_LIBS)
devLinuxAdcTest_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

