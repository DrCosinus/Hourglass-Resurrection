#include "../../shared/DllLoadInfos.h"

#include "../logging.h"

#include <sstream>

namespace Score
{
    DllLoadInfos theDllLoadInfos;

    Shared::Memory SharedMemory;

    namespace Exe
    {
        namespace
        {
            bool            myDllLoadInfosSent = false;
        }
        // sends to UpdateHooks
        auto DllLoadInfos::AddAndSend(const char* filename, bool loaded, HANDLE hProcess) -> void
        {
            if (loaded)
            {
                if (myDllLoadInfosSent)
                {
                    int numInfos = SharedMemory.GetInfoCount();
                    debugprintf("--  change info count from %d to %d\n", myInfos.size(), numInfos);
                    myInfos.resize(numInfos);
                }

                if (filename)
                {
                    const char* slash = max(strrchr(filename, '\\'), strrchr(filename, '/'));
                    const char* dllname = slash ? slash + 1 : filename;
                    // insert at start, since dll consumes from end
                    myInfos.emplace_front(dllname, loaded);
                }

                //if (myRemoteDllLoadInfos)
                {
                    // preparing buffer
                    std::ostringstream oss;
                    int count = 0;
                    for (auto info : myInfos)
                    {
                        if (info.myIsLoaded)
                        {
                            oss << info.myName << '+';
                        }
                        count++;
                    }
                    oss << ';';

                    auto str = oss.str();
                    auto buff = str.c_str();
                    auto len = std::streamoff(oss.tellp());

                    debugprintf("##  \"%s\" (%d)\n", buff, count);

                    SharedMemory.SetInfoCount(count);
                    memcpy(SharedMemory.GetBuffer(), buff, size_t(len));
                    //SIZE_T bytesWritten = 0;
                    //WriteProcessMemory(hProcess, myRemoteDllLoadInfos + offsetof(Dll::DllLoadInfos, myInfoCount), &count, sizeof(int), &bytesWritten);
                    //if (WriteProcessMemory(hProcess, myRemoteDllLoadInfos + offsetof(Dll::DllLoadInfos, myBuffer), buff, SIZE_T(len), &bytesWritten))
                    //{
                        SetDllLoadInfosSent(true);
                    //}
                    //else
                    //{
                    //    auto error = GetLastError();
                    //    debugprintf("--  ->  WriteProcessMemory failed with error code %d (=0x%x)\n", error, error);
                    //}
                }
            }
            //else
            //    return; // because UnInterceptUnloadingAPIs is disabled now

        }

        auto DllLoadInfos::SetRemoteDllLoadInfos(DllLoadInfos* aRemoteDllLoadInfos)               -> void
        {
            debugprintf("##  DllLoadInfos::SetRemoteDllLoadInfos to %p IGNORED\n", aRemoteDllLoadInfos);
            //myRemoteDllLoadInfos = aRemoteDllLoadInfos;
        }
        auto DllLoadInfos::SetDllLoadInfosSent(bool aDllLoadInfosSent)                    -> void
        {
            debugprintf("##  DllLoadInfos::SetDllLoadInfosSent to %s \n", aDllLoadInfosSent?"true":"false");

            debugprintf("##  MagicNumber = 0x%08X\n", SharedMemory.GetMagicNumber());

            myDllLoadInfosSent = aDllLoadInfosSent;
        }
    }
}