#include "../../shared/DllLoadInfos.h"

#include "../logging.h"

namespace Score
{
    DllLoadInfos theDllLoadInfos;

    namespace Exe
    {
        namespace
        {
            DllLoadInfos*   myRemoteDllLoadInfos = nullptr;
            bool            myDllLoadInfosSent = false;
        }
        // sends to UpdateHooks
        auto DllLoadInfos::AddAndSend(const char* filename, bool loaded, HANDLE hProcess) -> void
        {
            debugprintf("--  DllLoadInfos::AddAndSend( \"%s\", %s, 0x%x );\n", filename, loaded ? "true" : "false", reinterpret_cast<unsigned int>(hProcess));
            if (loaded)
            {
                SIZE_T bytesRead = 0;
                if (myDllLoadInfosSent && myRemoteDllLoadInfos)
                {
                    int numInfos;
                    ReadProcessMemory(hProcess, myRemoteDllLoadInfos, &numInfos, sizeof(numInfos), &bytesRead);
                    //myInfos.resize(numInfos);
                    debugprintf("#N#N#N#N#N#N#N#N#N#N#N#N#N#N#N#N#N#N#N#N#N#N#N#N#N# %d => %d\n", myInfos.size(), numInfos);
                }

                if (filename)
                {
                    const char* slash = max(strrchr(filename, '\\'), strrchr(filename, '/'));
                    const char* dllname = slash ? slash + 1 : filename;
                    // insert at start, since dll consumes from end
                    myInfos.emplace_front(dllname, loaded);
                }

                if (myRemoteDllLoadInfos)
                {
                    SIZE_T bytesWritten = 0;
                    if (WriteProcessMemory(hProcess, myRemoteDllLoadInfos, &theDllLoadInfos, sizeof(theDllLoadInfos), &bytesWritten))
                    {
                        SetDllLoadInfosSent(true);
                    }
                    else
                    {
                        auto error = GetLastError();
                        debugprintf("--  ->  WriteProcessMemory failed with error code %d (=0x%x)\n", error, error);
                    }
                }
            }
            //else
            //    return; // because UnInterceptUnloadingAPIs is disabled now

        }

        auto DllLoadInfos::SetRemoteDllLoadInfos(DllLoadInfos* aRemoteDllLoadInfos)               -> void
        {
            debugprintf("##  DllLoadInfos::SetRemoteDllLoadInfos to %p\n", aRemoteDllLoadInfos);
            myRemoteDllLoadInfos = aRemoteDllLoadInfos;
        }
        auto DllLoadInfos::SetDllLoadInfosSent(bool aDllLoadInfosSent)                    -> void
        {
            debugprintf("##  DllLoadInfos::SetDllLoadInfosSent to %s\n", aDllLoadInfosSent?"true":"false");
            myDllLoadInfosSent = aDllLoadInfosSent;
        }
    }
}