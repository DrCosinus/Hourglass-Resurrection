/*  Copyright (C) 2011 nitsuja and contributors
    Hourglass is licensed under GPL v2. Full notice is in COPYING.txt. */

#pragma once

int debugprintf(const char * fmt, ...);
int cmdprintf(const char * fmt, ...);

//#define dinputdebugprintf verbosedebugprintf
#define ddrawdebugprintf verbosedebugprintf
#define d3ddebugprintf verbosedebugprintf
#define dsounddebugprintf verbosedebugprintf
#define dmusicdebugprintf verbosedebugprintf
#define sdldebugprintf verbosedebugprintf
#define gldebugprintf verbosedebugprintf
#define registrydebugprintf verbosedebugprintf
#define timedebugprintf verbosedebugprintf

#if defined(_DEBUG) && 0//1
    #define verbosedebugprintf debugprintf
#else
    #define verbosedebugprintf(...) ((void)0)
#endif


#include <shared/logcat.h>

extern LogCategoryFlag g_includeLogFlags;
extern LogCategoryFlag g_excludeLogFlags;

// generally should always be enabled because it's controlled by a runtime option,
// but maybe some faster-than-release configuration could disable it in the future
#define ENABLE_LOGGING

#ifdef ENABLE_LOGGING
    int logprintf_internal(LogCategoryFlag cat, const char * fmt, ...);
    #define debuglog(cat, ...)         ((((cat) & g_includeLogFlags) && !((cat) & g_excludeLogFlags)) ? logprintf_internal(cat, __VA_ARGS__) : 0)
#else
    #define debuglog(cat, ...)         0
#endif
