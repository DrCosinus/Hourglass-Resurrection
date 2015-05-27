#include "MemoryRegionCollection.h"

#include <Windows.h>
#include <shared/winutil.h> // AutoCritSect
extern CRITICAL_SECTION s_activeMemoryRegionsCS;
extern CRITICAL_SECTION g_processMemCS;
//extern Score::Ram::Search::MemoryRegionCollection s_activeMemoryRegions;
extern HANDLE hGameProcess;

bool IsInNonCurrentYetTrustedAddressSpace(DWORD address);

namespace Score
{
    namespace Ram
    {
        namespace Search
        {
            //int MAX_RAM_SIZE = 0;

            auto MemoryRegionCollection::ResetAll() -> void
            {
                myContainer.clear();

                EnterCriticalSection(&s_activeMemoryRegionsCS);

                if (hGameProcess)
                {
                    EnterCriticalSection(&g_processMemCS);

                    auto si = SYSTEM_INFO();
                    GetSystemInfo(&si);

                    // walk process addresses
                    auto lpMem = si.lpMinimumApplicationAddress;
                    int totalSize = 0;

                    MEMORY_BASIC_INFORMATION mbi = { 0 };
                    while (lpMem < si.lpMaximumApplicationAddress)
                    {
                        VirtualQueryEx(hGameProcess, lpMem, &mbi, sizeof(MEMORY_BASIC_INFORMATION));
                        // increment lpMem to next region of memory
                        lpMem = (LPVOID)((unsigned char*)mbi.BaseAddress + (DWORD)mbi.RegionSize);

                        // check if it's readable and writable
                        // (including read-only regions gives us WAY too much memory to search)
                        if ((mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)) && !(mbi.Protect & PAGE_GUARD) && (mbi.State & MEM_COMMIT))
                        {
                            if (!IsInNonCurrentYetTrustedAddressSpace((unsigned int)mbi.BaseAddress))
                                continue;

                            if (HardwareAddress::IsValid(mbi.BaseAddress))
                            {
                                myContainer.push_back({ HardwareAddress(mbi.BaseAddress), mbi.RegionSize });
                            }
                        }
                    }

                    LeaveCriticalSection(&g_processMemCS);
                }

                size_t nextVirtualIndex = 0;

                for (auto& region : myContainer)
                {
                    region.virtualIndex = nextVirtualIndex;
                    nextVirtualIndex = region.virtualIndex + region.size;
                }

                if (nextVirtualIndex > myMaxRamSize)
                {
                    myPrevValues = (unsigned char*)realloc(myPrevValues, sizeof(char)*(nextVirtualIndex + 8));
                    memset(myPrevValues, 0, sizeof(char)*(nextVirtualIndex + 8));

                    myCurValues = (unsigned char*)realloc(myCurValues, sizeof(char)*(nextVirtualIndex + 8));
                    memset(myCurValues, 0, sizeof(char)*(nextVirtualIndex + 8));

                    myNumChanges = (unsigned short*)realloc(myNumChanges, sizeof(short)*(nextVirtualIndex + 8));
                    memset(myNumChanges, 0, sizeof(short)*(nextVirtualIndex + 8));

                    myItemIndexToRegionPointer = (MemoryRegion**)realloc(myItemIndexToRegionPointer, sizeof(MemoryRegion*)*(nextVirtualIndex + 8));
                    memset(myItemIndexToRegionPointer, 0, sizeof(MemoryRegion*)*(nextVirtualIndex + 8));

                    myMaxRamSize = nextVirtualIndex;
                }
                LeaveCriticalSection(&s_activeMemoryRegionsCS);
            }

            auto MemoryRegionCollection::ResetChanges() -> void
            {
                memset(myNumChanges, 0, sizeof(*myNumChanges)*myMaxRamSize);
            }

            auto MemoryRegionCollection::FreeAll() -> void
            {
                myMaxItemIndex = 0;
                myMaxRamSize = 0;
                myItemIndicesInvalid = true;

                free(myPrevValues);
                myPrevValues = nullptr;
                free(myCurValues);
                myCurValues = nullptr;
                free(myNumChanges);
                myNumChanges = nullptr;
                free(myItemIndexToRegionPointer);
                myItemIndexToRegionPointer = nullptr;

                myContainer.clear();
            }

            auto MemoryRegionCollection::CopyCurrentToPrevious() -> void
            {
                if (myPrevValues)
                {
                    memcpy(myPrevValues, myCurValues, (sizeof(*myPrevValues)*myMaxRamSize));
                }
            }

            auto MemoryRegionCollection::CalculateItemIndices(int anItemSize) -> void
            {
                if (myItemIndicesInvalid)
                {
                    AutoCritSect cs(&s_activeMemoryRegionsCS);
                    unsigned int itemIndex = 0;
                    //for (MemoryList::iterator iter = s_activeMemoryRegions.begin(); iter != s_activeMemoryRegions.end(); ++iter)
                    for (auto& region : myContainer)
                    {
                        region.itemIndex = itemIndex;
                        auto startSkipSize = region.hardwareAddress.SkipSize(anItemSize); // FIXME: is this still ok?
                        unsigned int start = startSkipSize;
                        unsigned int end = region.size;
                        for (unsigned int i = start; i < end; i += anItemSize)
                        {
                            myItemIndexToRegionPointer[itemIndex++] = &region;
                        }
                    }
                    myMaxItemIndex = itemIndex;
                    myItemIndicesInvalid = false;
                }
            }


            // eliminates a range of hardware addresses from the search results
            // returns 2 if it changed the region and moved the iterator to another region
            // returns 1 if it changed the region but didn't move the iterator
            // returns 0 if it had no effect
            // warning: don't call anything that takes an itemIndex in a loop that calls DeactivateRegion...
            //   doing so would be tremendously slow because DeactivateRegion invalidates the index cache
            auto MemoryRegionCollection::Deactivate(MemoryRegion& region, ContainerIterator_t& iter, HardwareAddress hardwareAddress, size_t size) -> DeactivateResult
            {
                if (hardwareAddress + size <= region.hardwareAddress || hardwareAddress >= region.hardwareAddress + region.size)
                {
                    // region is unaffected
                    return DeactivateResult::NoEffect;
                }
                else if (hardwareAddress > region.hardwareAddress && hardwareAddress + size >= region.hardwareAddress + region.size)
                {
                    // erase end of region
                    region.size = hardwareAddress - region.hardwareAddress;
                    return DeactivateResult::Changed;
                }
                else if (hardwareAddress <= region.hardwareAddress && hardwareAddress + size < region.hardwareAddress + region.size)
                {
                    // erase start of region
                    int eraseSize = (hardwareAddress + size) - region.hardwareAddress;
                    region.hardwareAddress += eraseSize;
                    region.size -= eraseSize;
                    region.virtualIndex += eraseSize;
                    return DeactivateResult::Changed;
                }
                else if (hardwareAddress <= region.hardwareAddress && hardwareAddress + size >= region.hardwareAddress + region.size)
                {
                    // erase entire region
                    iter = myContainer.erase(iter);
                    myItemIndicesInvalid = true;
                    return DeactivateResult::ChangedAndMoved;
                }
                else //if(hardwareAddress > region.hardwareAddress && hardwareAddress + size < region.hardwareAddress + region.size)
                {
                    // split region
                    int eraseSize = (hardwareAddress + size) - region.hardwareAddress;
                    MemoryRegion region2 = { region.hardwareAddress + eraseSize, region.size - eraseSize, /*region.softwareAddress + eraseSize,*/ region.virtualIndex + eraseSize };
                    region.size = hardwareAddress - region.hardwareAddress;
                    iter = myContainer.insert(++iter, region2);
                    myItemIndicesInvalid = true;
                    return DeactivateResult::ChangedAndMoved;
                }
            }

            /*
            // eliminates a range of hardware addresses from the search results
            // this is a simpler but usually slower interface for the above function
            void DeactivateRegion(HWAddressType hardwareAddress, unsigned int size)
            {
            for(MemoryList::iterator iter = s_activeMemoryRegions.begin(); iter != s_activeMemoryRegions.end(); )
            {
            MemoryRegion& region = *iter;
            if(2 != DeactivateRegion(region, iter, hardwareAddress, size))
            ++iter;
            }
            }
            */

#pragma region Compare to type functions
            template<size_t stepSize, typename compareType>
            void MemoryRegionCollection::SearchRelative(ComparisonCallback_t<compareType> cmpFun, compareType, compareType param)
            {
                for (auto iter = myContainer.begin(); iter != myContainer.end();)
                {
                    MemoryRegion& region = *iter;
                    auto startSkipSize = region.hardwareAddress.SkipSize(stepSize);
                    auto start = region.virtualIndex + startSkipSize;
                    auto end = region.virtualIndex + region.size;
                    auto hwaddr = region.hardwareAddress;
                    for (auto i = start; i < end; i += stepSize, hwaddr += stepSize)
                    {
                        if (!cmpFun(GetCurValueFromVirtualIndex<compareType>(i), GetPrevValueFromVirtualIndex<compareType>(i), param))
                        {
                            if (Deactivate(region, iter, hwaddr, stepSize) == DeactivateResult::ChangedAndMoved)
                            {
                                goto outerContinue;
                            }
                        }
                    }
                    ++iter;
                outerContinue:
                    continue;
                }
            }

            template<>
            void MemoryRegionCollection::SearchRelative < 1, char >(ComparisonCallback_t<char> cmpFun, char, char param);

            template<size_t stepSize, typename compareType>
            void MemoryRegionCollection::SearchSpecific(ComparisonCallback_t<compareType> cmpFun, compareType value, compareType param)
            {
                for (auto iter = myContainer.begin(); iter != myContainer.end();)
                {
                    MemoryRegion& region = *iter;
                    auto startSkipSize = region.hardwareAddress.SkipSize(stepSize);
                    auto start = region.virtualIndex + startSkipSize;
                    auto end = region.virtualIndex + region.size;
                    auto hwaddr = region.hardwareAddress;
                    for (auto i = start; i < end; i += stepSize, hwaddr += stepSize)
                    {
                        if (!cmpFun(GetCurValueFromVirtualIndex<compareType>(i), value, param))
                        {
                            if (Deactivate(region, iter, hwaddr, stepSize) == DeactivateResult::ChangedAndMoved)
                            {
                                goto outerContinue;
                            }
                        }
                    }
                    ++iter;
                outerContinue:
                    continue;
                }
            }

            template<size_t stepSize, typename compareType>
            void MemoryRegionCollection::SearchAddress(ComparisonCallback_t<compareType> cmpFun, compareType address, compareType param)
            {
                for (auto iter = myContainer.begin(); iter != myContainer.end();)
                {
                    auto& region = *iter;
                    auto startSkipSize = region.hardwareAddress.SkipSize(stepSize);
                    auto start = region.virtualIndex + startSkipSize;
                    auto end = region.virtualIndex + region.size;
                    auto hwaddr = region.hardwareAddress;
                    for (auto i = start; i < end; i += stepSize, hwaddr += stepSize)
                    {
                        if (!cmpFun(compareType(hwaddr), address, param))
                        {
                            if (DeactivateRegion(region, iter, hwaddr, stepSize) == DeactivateResult::ChangedAndMoved)
                            {
                                goto outerContinue;
                            }
                        }
                    }
                    ++iter;
                outerContinue:
                    continue;
                }
            }

            template<size_t stepSize, typename compareType>
            void MemoryRegionCollection::SearchChanges(ComparisonCallback_t<compareType> cmpFun, compareType changes, compareType param)
            {
                for (auto iter = myContainer.begin(); iter != myContainer.end();)
                {
                    auto& region = *iter;
                    auto startSkipSize = region.hardwareAddress.SkipSize(stepSize);
                    auto start = region.virtualIndex + startSkipSize;
                    auto end = region.virtualIndex + region.size;
                    auto hwaddr = region.hardwareAddress;
                    for (auto i = start; i < end; i += stepSize, hwaddr += stepSize)
                    {
                        if (!cmpFun(GetNumChangesFromVirtualIndex(i), changes, param))
                        {
                            if (DeactivateRegion(region, iter, hwaddr, stepSize)== DeactivateResult::ChangedAndMoved)
                            {
                                goto outerContinue;
                            }
                        }
                    }
                    ++iter;
                outerContinue:
                    continue;
                }
            }
#pragma endregion Compare to type functions

            template<size_t stepSize, typename>
            auto MemoryRegionCollection::CountRegionItemsT()->size_t
            {
                AutoCritSect cs(&s_activeMemoryRegionsCS);

                if (stepSize == 1)
                {
                    if (myContainer.empty())
                    {
                        return 0;
                    }

                    CalculateItemIndices(stepSize);

                    MemoryRegion& lastRegion = myContainer.back();
                    return lastRegion.itemIndex + lastRegion.size;
                }
                else // the branch above is faster but won't work if the step size isn't 1
                {
                    int total = 0;
                    for (auto& region : myContainer)
                    {
                        auto startSkipSize = region.hardwareAddress.SkipSize(stepSize);
                        total += (region.size - startSkipSize + (stepSize - 1)) / stepSize;
                    }
                    return total;
                }
            }

            template<size_t stepSize, typename compareType>
            auto MemoryRegionCollection::UpdateRegionsT() -> void
            {
                for (auto iter = myContainer.begin(); iter != myContainer.end();)
                {
                    const MemoryRegion& region = *iter;
                    ++iter;
                    const MemoryRegion* nextRegion = (iter == myContainer.end()) ? nullptr : &*iter;

                    UpdateRegionT<stepSize, sizeof(compareType)>(region, nextRegion);
                }

                myPrevValuesNeedUpdate = false;
            }

            template<size_t stepSize, size_t compareSize>
            auto MemoryRegionCollection::UpdateRegionT(const MemoryRegion& region, const MemoryRegion* nextRegionPtr) -> void
            {
                //if(GetAsyncKeyState(VK_SHIFT) & 0x8000) // speed hack
                //	return;

                if (myPrevValuesNeedUpdate)
                {
                    memcpy(myPrevValues + region.virtualIndex, myCurValues + region.virtualIndex, region.size + compareSize - stepSize);
                }

                auto startSkipSize = region.hardwareAddress.SkipSize(stepSize);

                //unsigned char* sourceAddr = region.softwareAddress - region.virtualIndex;
                static unsigned char* memoryBuffer = nullptr;
                static unsigned int memoryBufferAllocated = 0;
                if (memoryBufferAllocated < region.size + 8)
                {
                    memoryBufferAllocated = region.size + 8;
                    memoryBuffer = (unsigned char*)realloc(memoryBuffer, memoryBufferAllocated);
                }
                ReadProcessMemory(hGameProcess, (const void*)region.hardwareAddress, (void*)memoryBuffer, region.size, nullptr);
                if (compareSize > 1)
                {
                    ReadProcessMemory(hGameProcess, (const void*)(region.hardwareAddress + region.size), (void*)(memoryBuffer + region.size), compareSize - 1, nullptr);
                }
                unsigned char* sourceAddr = memoryBuffer - region.virtualIndex;

                unsigned int indexStart = region.virtualIndex + startSkipSize;
                unsigned int indexEnd = region.virtualIndex + region.size;

                if (compareSize == 1)
                {
                    for (unsigned int i = indexStart; i < indexEnd; i++)
                    {
                        if (s_curValues[i] != sourceAddr[i]) // if value changed
                        {
                            s_curValues[i] = sourceAddr[i]; // update value
                            //if(s_numChanges[i] != 0xFFFF)
                            s_numChanges[i]++; // increase change count
                        }
                    }
                }
                else // it's more complicated for non-byte sizes because:
                {    // - more than one byte can affect a given change count entry
                    // - when more than one of those bytes changes simultaneously the entry's change count should only increase by 1
                    // - a few of those bytes can be outside the region

                    unsigned int endSkipSize = ((unsigned int)(startSkipSize - region.size)) % stepSize;
                    unsigned int lastIndexToRead = indexEnd + endSkipSize + compareSize - stepSize;
                    unsigned int lastIndexToCopy = lastIndexToRead;
                    if (nextRegionPtr)
                    {
                        const MemoryRegion& nextRegion = *nextRegionPtr;
                        int nextStartSkipSize = ((unsigned int)(stepSize - size_t(nextRegion.hardwareAddress))) % stepSize;
                        unsigned int nextIndexStart = nextRegion.virtualIndex + nextStartSkipSize;
                        if (lastIndexToCopy > nextIndexStart)
                        {
                            lastIndexToCopy = nextIndexStart;
                        }
                    }

                    unsigned int nextValidChange[compareSize];
                    for (unsigned int i = 0; i < compareSize; ++i)
                    {
                        nextValidChange[i] = indexStart + i;
                    }

                    for (unsigned int i = indexStart, j = 0; i < lastIndexToRead; i++, j++)
                    {
                        if (s_curValues[i] != sourceAddr[i]) // if value of this byte changed
                        {
                            if (i < lastIndexToCopy)
                            {
                                s_curValues[i] = sourceAddr[i]; // update value
                            }
                            for (int k = 0; k < compareSize; k++) // loop through the previous entries that contain this byte
                            {
                                if (i >= indexEnd + k)
                                {
                                    continue;
                                }
                                int m = (j - k + compareSize) & (compareSize - 1);
                                if (nextValidChange[m] <= i) // if we didn't already increase the change count for this entry
                                {
                                    //if(s_numChanges[i-k] != 0xFFFF)
                                    s_numChanges[i - k]++; // increase the change count for this entry
                                    nextValidChange[m] = i - k + compareSize; // and remember not to increase it again
                                }
                            }
                        }
                    }
                }
            }

        } /* namespace Search */
    } /* namespace Ram */
} /* namespace Score */
