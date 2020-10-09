TOP= ../

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += netDev

# xxxRecord.h will be created from xxxRecord.dbd
DBDINC += arrayoutRecord
DBDINC += miwfRecord
DBDINC += patternRecord
DBDINC += statusRecord
DBDINC += channelsRecord
DBDINC += chansRecord
# install netDev.dbd into <top>/dbd
DBD += netDev.dbd

INC += drvNetMpf.h
INC += devNetDev.h

# The following are compiled and added to the Support library
netDev_SRCS += drvNetMpf.c
netDev_SRCS += devNetDev.c
netDev_SRCS += netDevLib.c
netDev_SRCS += arrayoutRecord.c
netDev_SRCS += miwfRecord.c
netDev_SRCS += patternRecord.c
netDev_SRCS += devMiwfSoft.c
netDev_SRCS += devPtnSoft.c
netDev_SRCS += statusRecord.c
netDev_SRCS += channelsRecord.c
netDev_SRCS += chansRecord.c
netDev_SRCS += devYewPlc.c
netDev_SRCS += devDarwin.c
netDev_SRCS += devMW100.c
netDev_SRCS += devKeyPlc.c

USR_CFLAGS += -Werror
#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
