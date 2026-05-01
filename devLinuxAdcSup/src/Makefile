# SPDX-FileCopyrightText: 2003 Argonne National Laboratory
#
# SPDX-License-Identifier: EPICS

TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC_Linux += devLinuxAdc

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install devLinuxAdc.dbd into <top>/dbd
DBD += devLinuxAdc.dbd

# specify all source files to be compiled and added to the library
#devLinuxAdc_SRCS += xxx

devLinuxAdc_SRCS += devTiAm335XAdc.c

devLinuxAdc_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

