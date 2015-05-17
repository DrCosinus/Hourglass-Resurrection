#pragma once

#include <shared/Score/TasFlags.h>

namespace Score { namespace Exe {

    class TasFlags : public ::Score::TasFlags
    {
    public:
        TasFlags()
        {
            playback = true;
            framerate = 60;
            keylimit = 8;
            forceSoftware = false;
            windowActivateFlags = 0;
            threadMode = 1;
            threadStackSize = 0;
            timersMode = 1;
            messageSyncMode = 1;
            waitSyncMode = 1;
            aviMode = 0;
            emuMode = EMUMODE_EMULATESOUND; // | (((recoveringStale||(fastForwardFlags&FFMODE_SOUNDSKIP))&&fastforward) ? EMUMODE_NOPLAYBUFFERS : 0) | ((threadMode==0||threadMode==4||threadMode==5) ? EMUMODE_VIRTUALDIRECTSOUND : 0),
            forceWindowed = 1;
            fastForward = false;
            forceSurfaceMemory = 0;
            audioFrequency = 44100;
            audioBitsPerSecond = 16;
            audioChannels = 2;
            stateLoaded = 0;
            fastForwardFlags = FFMODE_FRONTSKIP | FFMODE_BACKSKIP | FFMODE_RAMSKIP | FFMODE_SLEEPSKIP; // | (recoveringStale ? (FFMODE_FRONTSKIP|FFMODE_BACKSKIP) ? 0),
            initialTime = 6000;
            debugPrintMode = DebugPrintModeMask::DebuggerAndFile;
            timescale = 1;
            timescaleDivisor = 1;
            frameAdvanceHeld = false;
            allowLoadInstalledDlls = 0;
            allowLoadUxtheme = 0;
            storeVideoMemoryInSavestates = 1;
            appLocale = 0;
            movieVersion = VERSION;
            osVersionMajor = 0; // This will be filled in before the struct is used by anything else, look for the call to "DiscoverOS"
            osVersionMinor = 0; // This will be filled in before the struct is used by anything else, look for the call to "DiscoverOS"
            includeLogFlags = LCF_NONE | LCF_NONE; //includeLogFlags|traceLogFlags,
            excludeLogFlags = LCF_ERROR; //excludeLogFlags
        }
    };
} /*namespace Exe */ } /* namespace Score */

namespace Config
{
    extern Score::Exe::TasFlags localTASflags;
}