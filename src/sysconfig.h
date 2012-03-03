/*
 *  sysconfig.h - System configuration
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */

#define USE_OPENGL 1

#if defined(WIN32)
    #include "./sysconfig_WIN32.h"
#else
    #include "./sysconfig_POSIX.h"
#endif
