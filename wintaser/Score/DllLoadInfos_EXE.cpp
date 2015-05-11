#include "../../shared/DllLoadInfos.h"

namespace Score
{
    namespace Exe
    {
        // sends to UpdateHooks
        auto DllLoadInfos::AddAndSend(const char* filename, bool loaded) -> void
        {
            if (loaded && filename)
            {
                const char* slash = max(strrchr(filename, '\\'), strrchr(filename, '/'));
                const char* dllname = slash ? slash + 1 : filename;
                // insert at start, since dll consumes from end
                PushFirst(dllname);
            }
            //else
            //    return; // because UnInterceptUnloadingAPIs is disabled now
        }
    }
}