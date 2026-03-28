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

LIBRARY_IOC_Linux += devTiAm335XAdc

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install devTiAm335XAdc.dbd into <top>/dbd
DBD += devTiAm335XAdc.dbd

# specify all source files to be compiled and added to the library
#devTiAm335XAdc_SRCS += xxx

devTiAm335XAdc_SRCS += devTiAm335XAdc.c

devTiAm335XAdc_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

