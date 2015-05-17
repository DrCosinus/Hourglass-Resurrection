#pragma once

#include <shared/logcat.h>
#include <shared/version.h>

enum
{
    EMUMODE_EMULATESOUND = 0x01,
    EMUMODE_NOTIMERS = 0x02,
    EMUMODE_NOPLAYBUFFERS = 0x04,
    EMUMODE_VIRTUALDIRECTSOUND = 0x08,
};

enum
{
    FFMODE_FRONTSKIP = 0x01,
    FFMODE_BACKSKIP = 0x02,
    FFMODE_SOUNDSKIP = 0x04,
    FFMODE_RAMSKIP = 0x08,
    FFMODE_SLEEPSKIP = 0x10,
    FFMODE_WAITSKIP = 0x20,
};

enum class DebugPrintModeMask
{
    None = 0,
    Debugger = 1,
    File = 2,

    DebuggerAndFile = Debugger | File
};

inline auto operator&&(DebugPrintModeMask left, DebugPrintModeMask right) -> bool
{
    return !!(static_cast<unsigned int>(left)& static_cast<unsigned int>(right));
}

inline auto operator^=(DebugPrintModeMask& left, DebugPrintModeMask right) -> DebugPrintModeMask&
{
    left = static_cast<DebugPrintModeMask>(static_cast<unsigned int>(left) ^ static_cast<unsigned int>(right));
    return left;
}

namespace Score
{
    //localTASflags : EXE
    //tasflags : DLL

    class TasFlags
    {
    public:
        bool                playback;
        int                 framerate;                                  // DLL
        int                 keylimit;
        int                 forceSoftware;
        int                 windowActivateFlags;                        // DLL
        int                 threadMode;
        unsigned int        threadStackSize;
        int                 timersMode;
        int                 messageSyncMode;
        int                 waitSyncMode;
        int                 aviMode;                                    // DLL
        int                 emuMode;                                    // DLL
        int                 forceWindowed;
        bool                fastForward;                                // DLL
        int                 forceSurfaceMemory;
        int                 audioFrequency;
        int                 audioBitsPerSecond;
        int                 audioChannels;
        int                 stateLoaded;                                // DLL
        int                 fastForwardFlags;                           // DLL
        int                 initialTime;                                // DLL
        DebugPrintModeMask  debugPrintMode;                             // DLL
        int                 timescale, timescaleDivisor;                // DLL
        int                 frameAdvanceHeld;
        bool                allowLoadInstalledDlls, allowLoadUxtheme;
        int                 storeVideoMemoryInSavestates;               // DLL
        int                 appLocale;                                  // DLL
        unsigned int        movieVersion;
        int                 osVersionMajor, osVersionMinor;             // DLL
        LogCategoryFlag     includeLogFlags;                            // DLL
        LogCategoryFlag     excludeLogFlags;                            // DLL
#ifdef _USRDLL
        char reserved[256]; // just-in-case overwrite guard
#endif
    };

}