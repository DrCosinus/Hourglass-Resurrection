#include "../../shared/DllLoadInfos.h"

// only for DLL
#include "winutil.h" // for AutoCritSect
#include "../wintasee/intercept.h" // for RetryInterceptAPIs

namespace Score
{
    DllLoadInfos theDllLoadInfos;

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

            //int i = 0;
            //for (auto info : myInfos)
            //{
            //    debugprintf("theDllLoadInfos.dllname #%d = %s\n", i++, info.myName.c_str());
            //}

            while (!myInfos.empty())
            {
                const DllLoadInfo& info = myInfos.back(); //dllLoadInfos.infos[--dllLoadInfos.numInfos];
                if (info.myIsLoaded)
                {
                    RetryInterceptAPIs(info.myName.c_str());
                }
                //else
                //{
                //    UnInterceptUnloadingAPIs(info.dllname);
                //}
                myInfos.pop_back();
            }
        }
    }
}