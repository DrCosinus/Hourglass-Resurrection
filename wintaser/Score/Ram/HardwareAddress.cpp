#include "HardwareAddress.h"

#include <Windows.h>

//extern HANDLE theGameProcessHandle;
extern HANDLE hGameProcess;

namespace Score
{
    namespace Ram
    {
        auto HardwareAddress::IsValid() const -> bool
        {
            return IsValid(reinterpret_cast<void*>(myAddress));
        }

        /*static*/ auto HardwareAddress::IsValid(void* anAddress) -> bool
        {
            char temp[4];
            return ReadProcessMemory(hGameProcess, anAddress, reinterpret_cast<void*>(temp), 1, nullptr)
                && WriteProcessMemory(hGameProcess, anAddress, reinterpret_cast<const void*>(temp), 1, nullptr);

        }
    } /* namespace Ram */
} /* namespace Score */
