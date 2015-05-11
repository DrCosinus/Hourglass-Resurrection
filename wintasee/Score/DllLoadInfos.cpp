#include "../../shared/DllLoadInfos.h"

// only for DLL
#include "winutil.h" // for AutoCritSect
#include "../wintasee/intercept.h" // for RetryInterceptAPIs
//#include <vector>

#include <sstream>

namespace Score
{
    DllLoadInfos theDllLoadInfos;

    Shared::Memory SharedMemory;

    namespace Dll
    {
        static CRITICAL_SECTION s_dllLoadAndRetryInterceptCS;

        auto DllLoadInfos::InitializeCriticalSection() -> void
        {
            ::InitializeCriticalSection(&s_dllLoadAndRetryInterceptCS);
        }

        // consumes data sent by AddAndSend
        auto DllLoadInfos::UpdateHooks() -> void
        {
            AutoCritSect cs(&s_dllLoadAndRetryInterceptCS);

            auto cbuffer = SharedMemory.GetBuffer();
            do
            {
                auto c = *cbuffer++;
                if (c != ';')
                {
                    std::ostringstream oss;
                    do
                    {
                        oss << c;
                        c = *cbuffer++;
                    } while (c != '+');
                    auto strDll = oss.str();
                    RetryInterceptAPIs(strDll.c_str());
                }
                else
                {
                    break;
                }
            } while (true);

            SharedMemory.SetInfoCount(0);

        }
    }
}