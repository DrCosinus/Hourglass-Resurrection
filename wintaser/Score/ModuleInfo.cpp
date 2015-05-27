#include "ModuleInfo.h"

namespace Score
{
    struct MyMODULEINFO
    {
        MODULEINFO mi;
        std::string path;
    };
    static std::vector<MyMODULEINFO> trustedModuleInfos;
    static MyMODULEINFO injectedDllModuleInfo;

    void SetTrustedRangeInfo(TrustedRangeInfo& to, const MyMODULEINFO& from)
    {
        DWORD start = (DWORD)from.mi.lpBaseOfDll;
        to.end = start + from.mi.SizeOfImage;
        to.start = start;
    }

    void SendTrustedRangeInfos(HANDLE hProcess)
    {
        TrustedRangeInfos infos = {};

        SetTrustedRangeInfo(infos.infos[0], injectedDllModuleInfo);
        infos.numInfos++;

        int count = trustedModuleInfos.size();
        for (int i = 0; i < count; i++)
        {
            if (i + 1 < ARRAYSIZE(infos.infos))
            {
                SetTrustedRangeInfo(infos.infos[1 + i], trustedModuleInfos[i]);
                infos.numInfos++;
            }
        }

        SIZE_T bytesWritten = 0;
        WriteProcessMemory(hProcess, remoteTrustedRangeInfos, &infos, sizeof(infos), &bytesWritten);
    }



    DWORD CalculateModuleSize(LPVOID hModule, HANDLE hProcess)
    {
        // as noted below, I can't use GetModuleInformation here,
        // and I don't want to rely on the debughelp dll for core functionality either.
        DWORD size = 0;
        MEMORY_BASIC_INFORMATION mbi = {};
        while (VirtualQueryEx(hProcess, (char*)hModule + size + 0x1000, &mbi, sizeof(mbi))
            && (DWORD)mbi.AllocationBase == (DWORD)hModule)
            size += mbi.RegionSize;
        return max(size, 0x10000);
    }

    void RegisterModuleInfo(LPVOID hModule, HANDLE hProcess, const char* path)
    {
        int trusted = IsPathTrusted(path);
        if (!trusted)
            return; // we only want to keep track of the trusted ones

        MyMODULEINFO mmi;
        //	if(!GetModuleInformation(hProcess, (HMODULE)hModule, &mmi.mi, sizeof(mmi.mi)))
        {
            // GetModuleInformation never works, and neither does EnumProcessModules.
            // I think it's because we're calling it too early.
            // (either that or some versions of PSAPI.DLL are simply busted)
            // so this is the best fallback I could come up with
            mmi.mi.lpBaseOfDll = hModule;
            mmi.mi.SizeOfImage = CalculateModuleSize(hModule, hProcess);
            mmi.mi.EntryPoint = hModule; // don't care
        }
        mmi.path = path;
        if (trusted == 2)
        {
            injectedDllModuleInfo = mmi;
            //SendTrustedRangeInfos(hProcess); // disabled because it's probably not ready to receive it yet.
        }
        else
        {
            for (unsigned int i = 0; i < trustedModuleInfos.size(); i++)
            {
                MyMODULEINFO& mmi2 = trustedModuleInfos[i];
                if ((DWORD)mmi.mi.lpBaseOfDll >= (DWORD)mmi2.mi.lpBaseOfDll
                    && (DWORD)mmi.mi.lpBaseOfDll + mmi.mi.SizeOfImage <= (DWORD)mmi2.mi.lpBaseOfDll + mmi2.mi.SizeOfImage)
                {
                    debugprintf("apparently already TRUSTED MODULE 0x%08X - 0x%08X (%s)\n", mmi.mi.lpBaseOfDll, (DWORD)mmi.mi.lpBaseOfDll + mmi.mi.SizeOfImage, path);
                    return;
                }
            }
            trustedModuleInfos.push_back(mmi);
            SendTrustedRangeInfos(hProcess);
        }
        debugprintf("TRUSTED MODULE 0x%08X - 0x%08X (%s)\n", mmi.mi.lpBaseOfDll, (DWORD)mmi.mi.lpBaseOfDll + mmi.mi.SizeOfImage, path);
    }
    void UnregisterModuleInfo(LPVOID hModule, HANDLE hProcess, const char* path)
    {
        int trusted = IsPathTrusted(path);
        if (!trusted)
            return; // we only want to keep track of the trusted ones

        MEMORY_BASIC_INFORMATION mbi = {};
        if (VirtualQueryEx(hProcess, (char*)hModule + 0x1000, &mbi, sizeof(mbi)))
            hModule = (HMODULE)mbi.AllocationBase;

        if (trusted == 2)
        {
            injectedDllModuleInfo.path.clear();
            memset(&injectedDllModuleInfo.mi, 0, sizeof(injectedDllModuleInfo.mi));
            //SendTrustedRangeInfos(hProcess); // disabled because the thing we'd send it to is what just got unloaded
        }
        else
        {
            int count = trustedModuleInfos.size();
            for (int i = 0; i < count; i++)
            {
                if (trustedModuleInfos[i].mi.lpBaseOfDll == hModule)
                {
                    trustedModuleInfos.erase(trustedModuleInfos.begin() + i);
                    SendTrustedRangeInfos(hProcess);
                    break;
                }
            }
        }
    }

    bool IsInRange(DWORD address, const MODULEINFO& range)
    {
        return (DWORD)((DWORD)address - (DWORD)range.lpBaseOfDll) < (DWORD)(range.SizeOfImage);
    }
    bool IsInNonCurrentYetTrustedAddressSpace(DWORD address)
    {
        int count = trustedModuleInfos.size();
        for (int i = 0; i < count; i++)
            if (IsInRange(address, trustedModuleInfos[i].mi))
                return true;
        return (address == 0);
    }
}