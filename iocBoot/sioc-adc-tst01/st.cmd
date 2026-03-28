#!../../bin/linux-tdx-aarch64/devTiAm335XAdcApp

#- SPDX-FileCopyrightText: 2005 Argonne National Laboratory
#-
#- SPDX-License-Identifier: EPICS

#- You may have to change devTiAm335XAdcSup to something else
#- everywhere it appears in this file

#< envPaths

## Register all support components
dbLoadDatabase "../../dbd/devTiAm335XAdcTest.dbd"
devTiAm335XAdcTest_registerRecordDeviceDriver(pdbbase) 

## Load record instances
dbLoadRecords("../../db/AquilaAm69Adc.db","P=ADC:")

iocInit()
