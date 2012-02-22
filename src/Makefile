###############################################################################
#
# Makefile
#
###############################################################################

TARGET = frodo
#LIBS = vengine
#LIBDIRS = ../vengine/target
DEBUG = 0

###############################################################################

VERSION   = 4
REVISION  = 1

RESOURCES = $(sort $(subst .svn,,$(wildcard ../resources/*)))

# LINEBASED or SINGLECYCLE
EMUTYPE = LINEBASED
#EMUTYPE = SINGLECYCLE

SRC     = $(wildcard *.cpp)
DEF    += HAVE_SDL=1

ifeq ($(EMUTYPE),SINGLECYCLE)
  DEF += FRODO_SC
  SRC += $(wildcard ./sc/*.cpp)
else
  DEF += FRODO_PC PRECISE_CPU_CYCLES=1 PRECISE_CIA_CYCLES=1 PC_IS_POINTER=0
  SRC += $(wildcard ./pc/*.cpp)
endif

#DEF += PRECISE_CPU_CYCLES=1 PRECISE_CIA_CYCLES=1 PC_IS_POINTER=0
          
###############################################################################
#include ../../vengine/generic.mak
include ./generic.mak

Display.opp: Display_SDL.h
SID.opp: SID_SDL.h
