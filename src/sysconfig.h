#ifndef HAVE_SDL
#define HAVE_SDL 1
#endif

#if defined(WIN32)
    #include "./sysconfig_WIN32.h"
#else
    #include "./sysconfig_POSIX.h"
#endif