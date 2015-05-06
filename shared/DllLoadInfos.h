#pragma once

#include <Windows.h> // for HANDLE
#include <string>
#include <deque>

namespace Score
{
#if defined(_USRDLL)
    enum { IsDll = 1 };
#else
    enum { IsDll = 0 };
#endif

    // Common for DLL & EXE
    namespace Common
    {
        class DllLoadInfos
        {
        public:
            inline auto Clear() -> void
            {
                myInfos.clear();
            }
        protected:
            class DllLoadInfo
            {
            public:
                DllLoadInfo(const char*	aName, bool isLoaded)
                    : myName(aName)
                    , myIsLoaded(isLoaded)
                {
                }
                bool            myIsLoaded;
                std::string     myName;
            };
            std::deque<DllLoadInfo> myInfos;
        private:
        };
    }

    // EXE only
    namespace Exe
    {
        class DllLoadInfos final : public Common::DllLoadInfos
        {
        public:
            auto    AddAndSend              (const char* filename, bool loaded, HANDLE hProcess)    -> void;
            auto    SetRemoteDllLoadInfos   (DllLoadInfos* aRemoteDllLoadInfos)                     -> void;
            auto    SetDllLoadInfosSent     (bool aDllLoadInfosSent)                                -> void;
        };
    }

    // DLL only
    namespace Dll
    {
        class DllLoadInfos final : public Common::DllLoadInfos
        {
        public:
            auto    InitializeCriticalSection   () -> void;
            auto    UpdateHooks                 () -> void;
        };
    }

    using DllLoadInfos = std::conditional<IsDll, Dll::DllLoadInfos, Exe::DllLoadInfos>::type;

    extern DllLoadInfos theDllLoadInfos;
}

