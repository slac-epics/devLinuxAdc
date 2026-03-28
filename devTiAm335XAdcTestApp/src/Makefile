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

PROD_IOC_Linux = devTiAm335XAdcTest
# devTiAm335XAdcTest.dbd will be created and installed
DBD += devTiAm335XAdcTest.dbd

# devTiAm335XAdcTest.dbd will be made up from these files:
devTiAm335XAdcTest_DBD += base.dbd

# Include dbd files from all support applications:
devTiAm335XAdcTest_DBD += devTiAm335XAdc.dbd

# Add all the support libraries needed by this IOC
#devTiAm335XAdcTest_LIBS += xxx

# devTiAm335XAdcTest_registerRecordDeviceDriver.cpp derives from devTiAm335XAdcTest.dbd
devTiAm335XAdcTest_SRCS += devTiAm335XAdcTest_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
devTiAm335XAdcTest_SRCS_DEFAULT += devTiAm335XAdcTestMain.cpp
devTiAm335XAdcTest_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#devTiAm335XAdcTest_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

devTiAm335XAdcTest_LIBS += devTiAm335XAdc

# Finally link to the EPICS Base libraries
devTiAm335XAdcTest_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

