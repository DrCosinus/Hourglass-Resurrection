#include "../../shared/DllLoadInfos.h"

#include "../wintasee/intercept.h" // for RetryInterceptAPIs

namespace Score
{
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