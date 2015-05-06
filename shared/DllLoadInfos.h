#pragma once

#include <Windows.h> // for HANDLE
#include <string>
#include <deque>

#include <functional>

namespace Score
{
#if defined(_USRDLL)
    enum { IsDll = 1 };
#else
    enum { IsDll = 0 };
#endif


    //namespace Marshal
    //{
    //    template<typename T>
    //    struct Marshalled
    //    {
    //        Marshalled(T aValue) : myValue(aValue)
    //        {
    //        }
    //        auto operator=(const Marshalled& anOther) -> Marshalled&
    //        {
    //            Register(toto);
    //            mValue = anOther->myValue;
    //            Notify();
    //            return *this;
    //        }
    //        auto Register(std::function<void()> aFunction) -> void
    //        {
    //            
    //        }
    //        auto toto(bool)->void;
    //    private:
    //        std::bind
    //        auto Notify()
    //        {

    //        }

    //        T myValue;
    //    };
    //    struct DllLoadInfos
    //    {
    //        HANDLE myProcessHandle;
    //        LPVOID myBaseAddress;

    //        int     myInfoCount = 0;
    //        char    myBuffer[4096];
    //    };
    //}

    // EXE only
    namespace Exe
    {
        class DllLoadInfos final
        {
        public:
            auto    AddAndSend              (const char* filename, bool loaded, HANDLE hProcess)    -> void;
            auto    SetRemoteDllLoadInfos   (DllLoadInfos* aRemoteDllLoadInfos)                     -> void;
            auto    SetDllLoadInfosSent     (bool aDllLoadInfosSent)                                -> void;

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
            auto    InitializeCriticalSection   () -> void;
            auto    UpdateHooks                 () -> void;
        private:
            friend Exe::DllLoadInfos;

            int     myInfoCount = 0;
            char    myBuffer[4096];
        };
    }

    using DllLoadInfos = std::conditional<IsDll, Dll::DllLoadInfos, Exe::DllLoadInfos>::type;

    extern DllLoadInfos theDllLoadInfos;
}

