#pragma once

#include <Windows.h> // for HANDLE
#include <string>
#include <deque>

#include <functional>
#include "../wintasee/print.h"

namespace Score
{
#if defined(_USRDLL)
    enum { IsDll = 1 };
#else
    enum { IsDll = 0 };
#endif

    namespace Shared
    {
        class Memory
        {
        public:
            Memory()
            {
                Init();
            }
            ~Memory()
            {
                UnmapViewOfFile(myDefinitionPtr);
                if (myHandle != INVALID_HANDLE_VALUE)
                {
                    CloseHandle(myHandle);
                    myHandle = INVALID_HANDLE_VALUE;
                }
            }

            auto SetInfoCount(int anInfoCount) -> void
            {
                myDefinitionPtr->myInfoCount = anInfoCount;
            }
            auto GetInfoCount() -> int
            {
                return myDefinitionPtr->myInfoCount;
            }
            auto GetMagicNumber() -> int
            {
                return myDefinitionPtr->myMagicNumber;
            }
            auto GetBuffer() -> char*
            {
                return myDefinitionPtr->myBuffer;
            }
            auto GetBufferSize() -> size_t
            {
                return ARRAYSIZE(myDefinitionPtr->myBuffer);
            }

        private:
            struct definition
            {
                int     myInfoCount;
                int     myMagicNumber;
                char    myBuffer[4096];
            };

            auto Init() -> void
            {
                myHandle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(definition), "ShareMem");

                if (myHandle != INVALID_HANDLE_VALUE)
                {
                    myDefinitionPtr = reinterpret_cast<definition*>(MapViewOfFile(myHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(definition)));
                    //debugprintf("##  SharedMemory initilized successfully. %s %s\n", GetLastError() == ERROR_ALREADY_EXISTS ? "(Already exist)" : "", IsDll ? "[DLL]" : "[EXE]");
                }
                else
                {
                    //debugprintf("##  Failed to initilize SharedMemory. ErrorCode %d\n", GetLastError());
                }
                if (IsDll && myDefinitionPtr)
                {
                    myDefinitionPtr->myMagicNumber = 0x07142128;
                }
            }

            HANDLE myHandle = INVALID_HANDLE_VALUE;
            definition* myDefinitionPtr;
        };
    }

    extern Shared::Memory SharedMemory;

    // EXE only
    namespace Exe
    {
        class DllLoadInfos final
        {
        public:
            auto    AddAndSend(const char* filename, bool loaded, HANDLE hProcess)    -> void;
            auto    SetRemoteDllLoadInfos(DllLoadInfos* aRemoteDllLoadInfos)                     -> void;
            auto    SetDllLoadInfosSent(bool aDllLoadInfosSent)                                -> void;

            inline auto Clear() -> void
            {
                myInfos.clear();
            }
        private:
            class DllLoadInfo
            {
            public:
                DllLoadInfo() = default;
                DllLoadInfo(const char* aName, bool isLoaded)
                    : myName(aName)
                    , myIsLoaded(isLoaded)
                {
                }
                bool            myIsLoaded = false;
                std::string     myName = "";
            };
            std::deque<DllLoadInfo> myInfos;
        };
    }

    // DLL only
    namespace Dll
    {
        class DllLoadInfos final
        {
        public:
            auto    InitializeCriticalSection() -> void;
            auto    UpdateHooks() -> void;
        private:
            //friend Exe::DllLoadInfos;

            //int     myInfoCount = 0;
            //char    myBuffer[4096];
        };
    }

    using DllLoadInfos = std::conditional<IsDll, Dll::DllLoadInfos, Exe::DllLoadInfos>::type;

    extern DllLoadInfos theDllLoadInfos;
}

