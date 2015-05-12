#include "DllLoadInfos_DLL.h"

#include "../wintasee/intercept.h" // for RetryInterceptAPIs

namespace Score
{
    Dll::DllLoadInfos theDllLoadInfos;

    namespace Dll
    {
        // consumes data sent by AddAndSend
        auto DllLoadInfos::UpdateHooks() -> void
        {
            while (GetInfoCount())
            {
                RetryInterceptAPIs(PopLast().myDllName);
            }
        }
    }
}