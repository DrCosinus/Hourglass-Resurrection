/*  Copyright (C) 2011 nitsuja and contributors
    Hourglass is licensed under GPL v2. Full notice is in COPYING.txt. */

#pragma once

#include <mmsystem.h>

#if defined(_USRDLL)
#include <wintasee/Score/TasFlags.h>
#else
#include <wintaser/Score/TasFlags.h>
#endif

struct InfoForDebugger // GeneralInfoFromDll
{
    int frames;
    int ticks;
    int addedDelay;
    int lastNewTicks;
};


enum class CAPTUREINFO
{
    TYPE_NONE, // nothing sent
    TYPE_NONE_SUBSEQUENT, // nothing sent and it's the same frame/time as last time
    TYPE_PREV, // reuse previous frame's image (new sleep frame)
    TYPE_DDSD, // locked directdraw surface description
};

struct LastFrameSoundInfo
{
    DWORD size;
    unsigned char* buffer;
    LPWAVEFORMATEX format;
};


struct TrustedRangeInfo
{
    DWORD start, end;
};
struct TrustedRangeInfos
{
    int numInfos;
    TrustedRangeInfo infos[32]; // the first one is assumed to be the injected dll's range
};

#ifndef SUCCESSFUL_EXITCODE
#define SUCCESSFUL_EXITCODE 4242
#endif

