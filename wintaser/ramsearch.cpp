/*  Copyright (C) 2011 nitsuja and contributors
    Hourglass is licensed under GPL v2. Full notice is in COPYING.txt. */

// A few notes about this implementation of a RAM search window:
// (although, note first that it was created for a different application.
//  the tradeoffs it makes were focused towards a 16-bit console emulator
//  and not might be particularly appropriate here. consider using MHS instead.)
//
// Speed of update was one of the highest priories.
// This is because I wanted the RAM search window to be able to
// update every single value in RAM every single frame, and
// keep track of the exact number of frames across which each value has changed,
// without causing the emulation to run noticeably slower than normal.
//
// The data representation was changed from one entry per valid address
// to one entry per contiguous range of uneliminated addresses
// which references uniform pools of per-address properties.
// - This saves time when there are many items because
//   it minimizes the amount of data that needs to be stored and processed per address.
// - It also saves time when there are few items because
//   it ensures that no time is wasted in iterating through
//   addresses that have already been eliminated from the search.
//
// The worst-case scenario is when every other item has been
// eliminated from the search, maximizing the number of regions.
// This implementation manages to handle even that pathological case
// acceptably well. In fact, it still updates faster than the previous implementation.
// The time spent setting up or clearing such a large number of regions
// is somewhat horrendous, but it seems reasonable to have poor worst-case speed
// during these sporadic "setup" steps to achieve an all-around faster per-update speed.
// (You can test this case by performing the search: Modulo 2 Is Specific Address 0)

// this ram search module was made for Gens (Sega Genesis/CD/32X games)
// and adapted for use in winTASer (PC games)
// which means the amount of memory it has to deal with has greatly increased.
// it also has to use a slower call to get at the memory because it's owned by another process.
// as a result, it's now kind of slow instead of blazingly fast.
// it's still good enough for me in certain games (Cave Story), though.
//
// since dynamically allocated memory was not a possibility on the Genesis,
// dynamically allocated regions of memory are only added when "Reset" is clicked.
// also, only a small part of the application's total memory is searched.
// because of the way it is currently written, this module needs to allocate
// 8 times the amount of memory it is able to search.
//
// possibly the only advantage this has over other PC memory search tools
// is that the search results are exactly synchronized with the frame boundaries,
// which makes it easy to narrow down results using the "change count" number.

#include "ramsearch.h"

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include "resource.h"
#include "ramwatch.h"
#include "Config.h"
#include <shared/winutil.h>
#include <assert.h>
#include <commctrl.h>
#include <list>
#include <vector>

#ifdef _WIN32
#include "BaseTsd.h"
typedef INT_PTR intptr_t;
#else
#include "stdint.h"
#endif

#pragma region The Score
#include <Score/Ram/HardwareAddress.h>
#include <Score/Ram/Watch/WatcherCollection.h>
#include <Score/Ram/Search/MemoryRegionCollection.h>

#pragma message("DrCos: Temporary using declaration")
using Score::Ram::HardwareAddress;
using Score::Ram::Search::MemoryRegion;
using Score::Ram::Search::MemoryRegionCollection;

extern Score::Ram::Watch::WatcherCollection theWatchers;
#pragma endregion The Score

#pragma region Declarations
void CompactAddrs();
void signal_new_size();
void UpdateRamSearchTitleBar(int percent = 0);
void SetRamSearchUndoType(HWND hDlg, int type);
void ResetResults();

#pragma endregion Declarations

extern HWND RamSearchHWnd;
extern HWND RamWatchHWnd;
extern HWND hWnd;
extern HANDLE hGameProcess;
extern HINSTANCE hInst;
extern CRITICAL_SECTION g_processMemCS;
static char Str_Tmp_RS[1024];

int disableRamSearchUpdate = false;

// list of contiguous uneliminated memory regions

static MemoryRegionCollection theActiveMemoryRegions; //s_activeMemoryRegions;
static CRITICAL_SECTION s_activeMemoryRegionsCS;

// for undo support (could be better, but this way was really easy)
static MemoryRegionCollection theActiveMemoryRegionsBackup;
static int s_undoType = 0; // 0 means can't undo, 1 means can undo, 2 means can redo

//void RamSearchSaveUndoStateIfNotTooBig(HWND hDlg);
static const int tooManyRegionsForUndo = 10000;

// returns information about the item in the form of a "fake" region
// that has the item in it and nothing else
template<size_t stepSize, size_t compareSize>
void ItemIndexToVirtualRegion(unsigned int itemIndex, MemoryRegion& virtualRegion)
{
    theActiveMemoryRegions.CalculateItemIndices(stepSize);

    if (itemIndex >= s_maxItemIndex)
    {
        memset(&virtualRegion, 0, sizeof(MemoryRegion));
        return;
    }

    const MemoryRegion* regionPtr = s_itemIndexToRegionPointer[itemIndex];
    const MemoryRegion& region = *regionPtr;

    int bytesWithinRegion = (itemIndex - region.itemIndex) * stepSize;
    auto startSkipSize = region.hardwareAddress.SkipSize(stepSize);
    bytesWithinRegion += startSkipSize;

    virtualRegion.size = compareSize;
    virtualRegion.hardwareAddress = region.hardwareAddress + bytesWithinRegion;
    //virtualRegion.softwareAddress = region.softwareAddress + bytesWithinRegion;
    virtualRegion.virtualIndex = region.virtualIndex + bytesWithinRegion;
    virtualRegion.itemIndex = itemIndex;
    return;
}

template<size_t stepSize, size_t compareSize>
unsigned int ItemIndexToVirtualIndex(unsigned int itemIndex)
{
    MemoryRegion virtualRegion;
    ItemIndexToVirtualRegion<stepSize, compareSize>(itemIndex, virtualRegion);
    return virtualRegion.virtualIndex;
}

template<typename T>
T ReadLocalValue(const unsigned char* data)
{
    return *reinterpret_cast<const T*>(data);
}
template<typename compareType>
compareType GetPrevValueFromVirtualIndex(unsigned int virtualIndex)
{
    return ReadLocalValue<compareType>(s_prevValues + virtualIndex);
}

template<typename compareType>
compareType GetCurValueFromVirtualIndex(unsigned int virtualIndex)
{
    return ReadLocalValue<compareType>(s_curValues + virtualIndex);
}

template<size_t stepSize, typename compareType>
compareType GetPrevValueFromItemIndex(unsigned int itemIndex)
{
    int virtualIndex = ItemIndexToVirtualIndex<stepSize, sizeof(compareType)>(itemIndex);
    return GetPrevValueFromVirtualIndex<compareType>(virtualIndex);
}

template<size_t stepSize, typename compareType>
compareType GetCurValueFromItemIndex(unsigned int itemIndex)
{
    int virtualIndex = ItemIndexToVirtualIndex<stepSize, sizeof(compareType)>(itemIndex);
    return GetCurValueFromVirtualIndex<compareType>(virtualIndex);
}

template<size_t stepSize, typename compareType>
unsigned short GetNumChangesFromItemIndex(unsigned int itemIndex)
{
    int virtualIndex = ItemIndexToVirtualIndex<stepSize, sizeof(compareType)>(itemIndex);
    return GetNumChangesFromVirtualIndex(virtualIndex);
}

template<size_t stepSize, typename compareType>
HardwareAddress GetHardwareAddressFromItemIndex(unsigned int itemIndex)
{
    MemoryRegion virtualRegion;
    ItemIndexToVirtualRegion<stepSize, sizeof(compareType)>(itemIndex, virtualRegion);
    return virtualRegion.hardwareAddress;
}

// this one might be unreliable, haven't used it much
template<size_t stepSize, typename>
unsigned int HardwareAddressToItemIndex(HardwareAddress hardwareAddress)
{
    theActiveMemoryRegions.CalculateItemIndices(stepSize);

    for (MemoryList::iterator iter = s_activeMemoryRegions.begin(); iter != s_activeMemoryRegions.end(); ++iter)
    {
        MemoryRegion& region = *iter;
        if (hardwareAddress >= region.hardwareAddress && hardwareAddress < region.hardwareAddress + region.size)
        {
            int indexWithinRegion = (hardwareAddress - region.hardwareAddress) / stepSize;
            return region.itemIndex + indexWithinRegion;
        }
    }

    return -1;
}




// it's ugly but I can't think of a better way to call these functions that isn't also slower, since
// I need the current values of these arguments to determine which primitive types are used within the function
#define CALL_WITH_T_SIZE_TYPES(functionName, sizeTypeID, typeID, requireAligned, ...)   \
    (typeID == Score::Ram::TypeID::Float                                                \
    ?   (sizeTypeID == Score::Ram::SizeID::_64bits                                      \
        ?   (requireAligned                                                             \
            ?   functionName<8, double>(##__VA_ARGS__)                                  \
            :   functionName<1, double>(##__VA_ARGS__)                                  \
            )                                                                           \
        :   (requireAligned                                                             \
            ?   functionName<4, float>(##__VA_ARGS__)                                   \
            :   functionName<1, float>(##__VA_ARGS__)                                   \
            )                                                                           \
        )                                                                               \
    :   sizeTypeID == Score::Ram::SizeID::_8bits                                        \
        ?   (typeID == Score::Ram::TypeID::SignedDecimal                                \
            ?   functionName<1, signed char>(##__VA_ARGS__)                             \
            :   functionName<1, unsigned char>(##__VA_ARGS__)                           \
            )                                                                           \
        :   sizeTypeID == Score::Ram::SizeID::_16bits                                   \
            ?   (typeID == Score::Ram::TypeID::SignedDecimal                            \
                ?   (requireAligned                                                     \
                    ?   functionName<2, signed short>(##__VA_ARGS__)                    \
                    :   functionName<1, signed short>(##__VA_ARGS__)                    \
                    )                                                                   \
                :   (requireAligned                                                     \
                    ?   functionName<2, unsigned short>(##__VA_ARGS__)                  \
                    :   functionName<1, unsigned short>(##__VA_ARGS__)                  \
                    )                                                                   \
                )                                                                       \
            :   sizeTypeID == Score::Ram::SizeID::_32bits                               \
                ?   (typeID == Score::Ram::TypeID::SignedDecimal                        \
                    ?   (requireAligned                                                 \
                        ?   functionName<4, signed long>(##__VA_ARGS__)                 \
                        :   functionName<1, signed long>(##__VA_ARGS__)                 \
                        )                                                               \
                    :   (requireAligned                                                 \
                        ?   functionName<4, unsigned long>(##__VA_ARGS__)               \
                        :   functionName<1, unsigned long>(##__VA_ARGS__)               \
                        )                                                               \
                    )                                                                   \
                :   sizeTypeID == Score::Ram::SizeID::_64bits                           \
                    ?   (typeID == Score::Ram::TypeID::SignedDecimal                    \
                        ?   (requireAligned                                             \
                            ?   functionName<8, signed long long>(##__VA_ARGS__)        \
                            :   functionName<1, signed long long>(##__VA_ARGS__)        \
                            )                                                           \
                        :   (requireAligned                                             \
                            ?   functionName<8, unsigned long long>(##__VA_ARGS__)      \
                            :   functionName<1, unsigned long long>(##__VA_ARGS__)      \
                            )                                                           \
                        )                                                               \
                    :   functionName<1, signed char>(##__VA_ARGS__)                     \
    )

        // version that takes a forced comparison type
#define CALL_WITH_T_STEP(functionName, sizeTypeID, sign, type, requireAligned, ...) \
    (sizeTypeID == Score::Ram::SizeID::_8bits                                       \
    ?   functionName<1, sign type>(##__VA_ARGS__)                                   \
    :   sizeTypeID == Score::Ram::SizeID::_16bits                                   \
        ?   (requireAligned                                                         \
            ?   functionName<2, sign type>(##__VA_ARGS__)                           \
            :   functionName<1, sign type>(##__VA_ARGS__))                          \
        :   sizeTypeID == Score::Ram::SizeID::_32bits                               \
            ? (requireAligned                                                       \
                ?   functionName<4, sign type>(##__VA_ARGS__)                       \
                :   functionName<1, sign type>(##__VA_ARGS__))                      \
            :   sizeTypeID == Score::Ram::SizeID::_64bits                           \
                ?   (requireAligned                                                 \
                    ?   functionName<8, sign type>(##__VA_ARGS__)                   \
                    :   functionName<1, sign type>(##__VA_ARGS__))                  \
                :   functionName<1, sign type>(##__VA_ARGS__))


#pragma region Basic comparison functions:
namespace Comparison
{
    template<typename T>
    inline bool Less(T x, T y, T)
    {
        return x < y;
    }

    template<typename T>
    inline bool Greater(T x, T y, T)
    {
        return x > y;
    }

    template<typename T>
    inline bool LessOrEqual(T x, T y, T)
    {
        return x <= y;
    }

    template<typename T>
    inline bool GreaterOrEqual(T x, T y, T)
    {
        return x >= y;
    }

    template<typename T>
    inline bool Equal(T x, T y, T)
    {
        return x == y;
    }

    template<typename T>
    inline bool Unequal(T x, T y, T)
    {
        return x != y;
    }

    template<typename T>
    inline bool DiffBy(T x, T y, T p)
    {
        return x - y == p || y - x == p;
    }

    template<typename T>
    inline bool ModIs(T x, T y, T p)
    {
        return p && x % p == y;
    }

    template<>
    inline bool ModIs(float x, float y, float p)
    {
        return p && fmodf(x, p) == y;
    }

    template<>
    inline bool ModIs(double x, double y, double p)
    {
        return p && fmod(x, p) == y;
    }
}
#pragma endregion Basic comparison functions

    char rs_c = 's'; // Compare To/by: 'r' previous value, 's' specific value, 'a' specific address, 'n' number of changes
    char rs_o = '='; // Comparison Operator: Less than, Greater than, Less than or equal to, Greater than or equal to, equal to, Not equel to, Different by:, Modulo X is
    auto rs_type = Score::Ram::TypeID::SignedDecimal;
    auto rs_param = Score::Ram::Value{ 0 };
    auto rs_val = Score::Ram::Value{ 0 };
    auto rs_val_valid = false;
    auto rs_type_size = Score::Ram::SizeID::_8bits;
    auto rs_last_type_size = Score::Ram::SizeID::_8bits;
    auto noMisalign = true;
    auto rs_last_no_misalign = true;
    auto last_rs_possible = -1;
    auto last_rs_regions = -1;

    auto sizeTypeIDToSize(Score::Ram::SizeID aSizeID) -> size_t
    {
        return Score::Ram::GetSizeInBytes(aSizeID);
    }

    static auto prune(char c, char o, Score::Ram::TypeID t, Score::Ram::Value v, Score::Ram::Value p) -> void
    {
        EnterCriticalSection(&s_activeMemoryRegionsCS);

        // repetition-reducing macros
#define DO_SEARCH(sf) \
    switch (o) \
                { \
        case '<': DO_SEARCH_2(Comparison::Less,sf); break; \
        case '>': DO_SEARCH_2(Comparison::Greater,sf); break; \
        case '=': DO_SEARCH_2(Comparison::Equal,sf); break; \
        case '!': DO_SEARCH_2(Comparison::Unequal,sf); break; \
        case 'l': DO_SEARCH_2(Comparison::LessOrEqual,sf); break; \
        case 'm': DO_SEARCH_2(Comparison::GreaterOrEqual,sf); break; \
        case 'd': DO_SEARCH_2(Comparison::DiffBy,sf); break; \
        case '%': DO_SEARCH_2(Comparison::ModIs,sf); break; \
        default: assert(!"Invalid operator for this search type."); break; \
                }

        // perform the search, eliminating nonmatching values
        switch (c)
        {
#define DO_SEARCH_2(CmpFun,sf) CALL_WITH_T_SIZE_TYPES(sf, rs_type_size, t, noMisalign, CmpFun,v,p)
        case 'r': DO_SEARCH(theActiveMemoryRegions.SearchRelative); break;
        case 's': DO_SEARCH(theActiveMemoryRegions.SearchSpecific); break;
#undef DO_SEARCH_2

#define DO_SEARCH_2(CmpFun,sf) CALL_WITH_T_STEP(sf, rs_type_size, unsigned, int, noMisalign, CmpFun,v,p);
        case 'a': DO_SEARCH(theActiveMemoryRegions.SearchAddress); break;
#undef DO_SEARCH_2

#define DO_SEARCH_2(CmpFun,sf) CALL_WITH_T_STEP(sf, rs_type_size, unsigned, short, noMisalign, CmpFun,v,p);
        case 'n': DO_SEARCH(theActiveMemoryRegions.SearchChanges); break;
#undef DO_SEARCH_2

        default: assert(!"Invalid search comparison type."); break;
        }

        LeaveCriticalSection(&s_activeMemoryRegionsCS);

        s_prevValuesNeedUpdate = true;

        int prevNumItems = last_rs_possible;

        CompactAddrs();

        if (prevNumItems == last_rs_possible)
        {
            SetRamSearchUndoType(RamSearchHWnd, 0); // nothing to undo
        }
    }

    template<size_t stepSize, typename compareType>
    bool CompareRelativeAtItem(bool(*cmpFun)(compareType, compareType, compareType), int itemIndex, compareType ignored, compareType param)
    {
        return cmpFun(GetCurValueFromItemIndex<stepSize, compareType>(itemIndex), GetPrevValueFromItemIndex<stepSize, compareType>(itemIndex), param);
    }
    template<size_t stepSize, typename compareType>
    bool CompareSpecificAtItem(bool(*cmpFun)(compareType, compareType, compareType), int itemIndex, compareType value, compareType param)
    {
        return cmpFun(GetCurValueFromItemIndex<stepSize, compareType>(itemIndex), value, param);
    }
    template<size_t stepSize, typename compareType>
    bool CompareAddressAtItem(bool(*cmpFun)(compareType, compareType, compareType), int itemIndex, compareType address, compareType param)
    {
        return cmpFun(compareType(GetHardwareAddressFromItemIndex<stepSize, compareType>(itemIndex)), address, param);
    }
    template<size_t stepSize, typename compareType>
    bool CompareChangesAtItem(bool(*cmpFun)(compareType, compareType, compareType), int itemIndex, compareType changes, compareType param)
    {
        return cmpFun(GetNumChangesFromItemIndex<stepSize, compareType>(itemIndex), changes, param);
    }

    auto ReadControlInt(int controlID, Score::Ram::SizeID sizeTypeID, Score::Ram::TypeID typeID, bool& success) -> Score::Ram::Value
    {
        auto rv = Score::Ram::Value{ 0 };
        success = false;

        char text[64];
        if (GetDlgItemText(RamSearchHWnd, controlID, text, 64))
        {
            success = rv.sscan(text, sizeTypeID, typeID);
        }

        return rv;
    }


    bool Set_RS_Val()
    {
        bool success;

        // update rs_val
        switch (rs_c)
        {
        case 'r':
        default:
            rs_val = 0;
            break;
        case 's':
            rs_val = ReadControlInt(IDC_EDIT_COMPAREVALUE, rs_type_size, rs_type, success);
            if (!success)
            {
                return false;
            }
            if ((rs_type_size == Score::Ram::SizeID::_8bits && rs_type == Score::Ram::TypeID::SignedDecimal && ((int)rs_val < -128 || (int)rs_val > 127)) ||
                (rs_type_size == Score::Ram::SizeID::_8bits && rs_type != Score::Ram::TypeID::SignedDecimal && ((int)rs_val < 0 || (int)rs_val > 255)) ||
                (rs_type_size == Score::Ram::SizeID::_16bits && rs_type == Score::Ram::TypeID::SignedDecimal && ((int)rs_val < -32768 || (int)rs_val > 32767)) ||
                (rs_type_size == Score::Ram::SizeID::_16bits && rs_type != Score::Ram::TypeID::SignedDecimal && ((int)rs_val < 0 || (int)rs_val > 65535)))
            {
                return false;
            }
            break;
        case 'a':
            rs_val = ReadControlInt(IDC_EDIT_COMPAREADDRESS, Score::Ram::SizeID::_32bits, Score::Ram::TypeID::Hexadecimal, success);
            if (!success || (int)rs_val < 0/* || (int)rs_val > 0x06040000*/)
            {
                return false;
            }
            break;
        case 'n':
            rs_val = ReadControlInt(IDC_EDIT_COMPARECHANGES, Score::Ram::SizeID::_32bits, Score::Ram::TypeID::UnsignedDecimal, success);
            if (!success || (int)rs_val < 0 || (int)rs_val > 0xFFFF)
            {
                return false;
            }
            break;
        }

        // also update rs_param
        switch (rs_o)
        {
        default:
            rs_param = 0;
            break;
        case 'd':
            rs_param = ReadControlInt(IDC_EDIT_DIFFBY, (rs_c == 'r' || rs_c == 's') ? rs_type_size : Score::Ram::SizeID::_32bits, (rs_c == 'r' || rs_c == 's') ? rs_type : (rs_c == 'a' ? Score::Ram::TypeID::Hexadecimal : Score::Ram::TypeID::SignedDecimal), success);
            if (!success)
            {
                return false;
            }
            if (rs_param < 0)
            {
                rs_param = -rs_param;
            }
            break;
        case '%':
            rs_param = ReadControlInt(IDC_EDIT_MODBY, (rs_c == 'r' || rs_c == 's') ? rs_type_size : Score::Ram::SizeID::_32bits, (rs_c == 'r' || rs_c == 's') ? rs_type : (rs_c == 'a' ? Score::Ram::TypeID::Hexadecimal : Score::Ram::TypeID::SignedDecimal), success);
            if (!success || (int)rs_param == 0)
            {
                return false;
            }
            break;
        }

        // validate that rs_param fits in the comparison data type
        {
            auto appliedSize = rs_type_size;
            auto appliedSign = rs_type;
            if (rs_c == 'n')
            {
                appliedSize = Score::Ram::SizeID::_16bits;
                appliedSign = Score::Ram::TypeID::UnsignedDecimal;
            }
            if (rs_c == 'a')
            {
                appliedSize = Score::Ram::SizeID::_32bits;
                appliedSign = Score::Ram::TypeID::UnsignedDecimal;
            }
            if ((appliedSize == Score::Ram::SizeID::_8bits  && appliedSign == Score::Ram::TypeID::SignedDecimal && ((int)rs_param < -128 || (int)rs_param > 127)) ||
                (appliedSize == Score::Ram::SizeID::_8bits  && appliedSign != Score::Ram::TypeID::SignedDecimal && ((int)rs_param < 0 || (int)rs_param > 255)) ||
                (appliedSize == Score::Ram::SizeID::_16bits && appliedSign == Score::Ram::TypeID::SignedDecimal && ((int)rs_param < -32768 || (int)rs_param > 32767)) ||
                (appliedSize == Score::Ram::SizeID::_16bits && appliedSign != Score::Ram::TypeID::SignedDecimal && ((int)rs_param < 0 || (int)rs_param > 65535)))
            {
                return false;
            }
        }

        return true;
    }

    bool IsSatisfied(int itemIndex)
    {
        if (!rs_val_valid)
            return true;
        int o = rs_o;
        switch (rs_c)
        {
#define DO_SEARCH_2(CmpFun,sf) return CALL_WITH_T_SIZE_TYPES(sf, rs_type_size,rs_type,noMisalign, CmpFun,itemIndex,rs_val,rs_param);
        case 'r': DO_SEARCH(CompareRelativeAtItem); break;
        case 's': DO_SEARCH(CompareSpecificAtItem); break;
#undef DO_SEARCH_2

#define DO_SEARCH_2(CmpFun,sf) return CALL_WITH_T_STEP(sf, rs_type_size, unsigned,int, noMisalign, CmpFun,itemIndex,rs_val,rs_param);
        case 'a': DO_SEARCH(CompareAddressAtItem); break;
#undef DO_SEARCH_2

#define DO_SEARCH_2(CmpFun,sf) return CALL_WITH_T_STEP(sf, rs_type_size, unsigned,short, noMisalign, CmpFun,itemIndex,rs_val,rs_param);
        case 'n': DO_SEARCH(CompareChangesAtItem); break;
#undef DO_SEARCH_2
        }
        return false;
    }

    int ResultCount = 0;
    bool AutoSearch = false;
    bool AutoSearchAutoRetry = false;
    LRESULT CALLBACK PromptWatchNameProc(HWND, UINT, WPARAM, LPARAM);
    void UpdatePossibilities(int rs_possible, int regions);


    void CompactAddrs()
    {
        int size = noMisalign ? sizeTypeIDToSize(rs_type_size) : 1;
        int prevResultCount = ResultCount;

        theActiveMemoryRegions.CalculateItemIndices(size);
        ResultCount = CALL_WITH_T_SIZE_TYPES(theActiveMemoryRegions.CountRegionItemsT, rs_type_size, rs_type, noMisalign);

        UpdatePossibilities(ResultCount, theActiveMemoryRegions.Count());

        if (ResultCount != prevResultCount)
            ListView_SetItemCount(GetDlgItem(RamSearchHWnd, IDC_RAMLIST), ResultCount);
    }

    void soft_reset_address_info()
    {
        s_prevValuesNeedUpdate = false;
        theActiveMemoryRegions.ResetAll();
        if (!RamSearchHWnd)
        {
            theActiveMemoryRegions.Clear();
            ResultCount = 0;
        }
        else
        {
            // force s_prevValues to be valid
            signal_new_frame();
            s_prevValuesNeedUpdate = true;
            signal_new_frame();
        }
        theActiveMemoryRegions.ResetChanges();
        CompactAddrs();
    }
    void reset_address_info()
    {
        SetRamSearchUndoType(RamSearchHWnd, 0);
        EnterCriticalSection(&s_activeMemoryRegionsCS);
        theActiveMemoryRegions.Clear(); // not necessary, but we'll take the time hit here instead of at the next thing that sets up an undo
        LeaveCriticalSection(&s_activeMemoryRegionsCS);
        theActiveMemoryRegions.CopyCurrentToPrevious();
        s_prevValuesNeedUpdate = false;
        theActiveMemoryRegions.ResetAll();
        if (!RamSearchHWnd)
        {
            EnterCriticalSection(&s_activeMemoryRegionsCS);
            theActiveMemoryRegions.Clear();
            LeaveCriticalSection(&s_activeMemoryRegionsCS);
            ResultCount = 0;
        }
        else
        {
            // force s_prevValues to be valid
            signal_new_frame();
            s_prevValuesNeedUpdate = true;
            signal_new_frame();
        }
        theActiveMemoryRegions.ResetChanges();
        CompactAddrs();
    }

    void signal_new_frame()
    {
        EnterCriticalSection(&s_activeMemoryRegionsCS);
        EnterCriticalSection(&g_processMemCS);
        CALL_WITH_T_SIZE_TYPES(theActiveMemoryRegions.UpdateRegionsT, rs_type_size, rs_type, noMisalign);
        LeaveCriticalSection(&g_processMemCS);
        LeaveCriticalSection(&s_activeMemoryRegionsCS);
    }





    bool RamSearchClosed = false;
    bool RamWatchClosed = false;

    void ResetResults()
    {
        reset_address_info();
        ResultCount = 0;
        if (RamSearchHWnd)
            ListView_SetItemCount(GetDlgItem(RamSearchHWnd, IDC_RAMLIST), ResultCount);
    }
    void ReopenRamWindows() //Reopen them when a new Rom is loaded
    {
        HWND hwnd = GetActiveWindow();

        if (RamSearchClosed)
        {
            RamSearchClosed = false;
            if (!RamSearchHWnd)
            {
                reset_address_info(); // TODO: is this prone to deadlock? should we set ResultCount = 0 instead?
                LRESULT CALLBACK RamSearchProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
                RamSearchHWnd = CreateDialog(hInst, MAKEINTRESOURCE(IDD_RAMSEARCH), hWnd, (DLGPROC)RamSearchProc);
            }
        }
        if (RamWatchClosed || AutoRWLoad)
        {
            RamWatchClosed = false;
            if (!RamWatchHWnd)
            {
                if (AutoRWLoad) OpenRWRecentFile(0);
                LRESULT CALLBACK RamWatchProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
                RamWatchHWnd = CreateDialog(hInst, MAKEINTRESOURCE(IDD_RAMWATCH), hWnd, (DLGPROC)RamWatchProc);
            }
        }

        if (hwnd == hWnd && hwnd != GetActiveWindow())
            SetActiveWindow(hWnd); // restore focus to the main window if it had it before
    }






    void RefreshRamListSelectedCountControlStatus(HWND hDlg)
    {
        static int prevSelCount = -1;
        int selCount = ListView_GetSelectedCount(GetDlgItem(hDlg, IDC_RAMLIST));
        if (selCount != prevSelCount)
        {
            if (selCount < 2 || prevSelCount < 2)
            {
                EnableWindow(GetDlgItem(hDlg, IDC_C_WATCH), (selCount >= 1) ? TRUE : FALSE);
                EnableWindow(GetDlgItem(hDlg, IDC_C_ADDCHEAT), (selCount >= 1) ? /*TRUE*/FALSE : FALSE);
                EnableWindow(GetDlgItem(hDlg, IDC_C_ELIMINATE), (selCount >= 1) ? TRUE : FALSE);
            }
            prevSelCount = selCount;
        }
    }




    struct AddrRange
    {
        HardwareAddress addr;
        size_t size;
        auto End() const -> HardwareAddress { return addr + size; }
        AddrRange(HardwareAddress a, size_t s) : addr(a), size(s){}
    };

    void signal_new_size()
    {
        HWND lv = GetDlgItem(RamSearchHWnd, IDC_RAMLIST);

        int oldSize = rs_last_no_misalign ? sizeTypeIDToSize(rs_last_type_size) : 1;
        int newSize = noMisalign ? sizeTypeIDToSize(rs_type_size) : 1;
        bool numberOfItemsChanged = (oldSize != newSize);

        unsigned int itemsPerPage = ListView_GetCountPerPage(lv);
        unsigned int oldTopIndex = ListView_GetTopIndex(lv);
        unsigned int oldSelectionIndex = ListView_GetSelectionMark(lv);
        auto oldTopAddr = CALL_WITH_T_SIZE_TYPES(GetHardwareAddressFromItemIndex, rs_last_type_size, rs_type, rs_last_no_misalign, oldTopIndex);
        auto oldSelectionAddr = CALL_WITH_T_SIZE_TYPES(GetHardwareAddressFromItemIndex, rs_last_type_size, rs_type, rs_last_no_misalign, oldSelectionIndex);

        std::vector<AddrRange> selHardwareAddrs;
        if (numberOfItemsChanged)
        {
            // store selection ranges
            // unfortunately this can take a while if the user has a huge range of items selected
            //		Clear_Sound_Buffer();
            int selCount = ListView_GetSelectedCount(lv);
            int size = rs_last_no_misalign ? sizeTypeIDToSize(rs_last_type_size) : 1;
            int watchIndex = -1;
            for (int i = 0; i < selCount; ++i)
            {
                watchIndex = ListView_GetNextItem(lv, watchIndex, LVNI_SELECTED);
                auto addr = CALL_WITH_T_SIZE_TYPES(GetHardwareAddressFromItemIndex, rs_last_type_size, rs_type, rs_last_no_misalign, watchIndex);
                if (!selHardwareAddrs.empty() && addr == selHardwareAddrs.back().End())
                {
                    selHardwareAddrs.back().size += size;
                }
#pragma message ("DrCos: condition end has no sense!!")
                else if (!(noMisalign && oldSize < newSize/* && (addr & (newSize - 1)) != 0 ???*/))
                {
                    selHardwareAddrs.push_back(AddrRange(addr, size));
                }
            }
        }

        CompactAddrs();

        rs_last_type_size = rs_type_size;
        rs_last_no_misalign = noMisalign;

        if (numberOfItemsChanged)
        {
            // restore selection ranges
            auto newTopIndex = CALL_WITH_T_SIZE_TYPES(HardwareAddressToItemIndex, rs_type_size, rs_type, noMisalign, oldTopAddr);
            unsigned int newBottomIndex = newTopIndex + itemsPerPage - 1;
            SendMessage(lv, WM_SETREDRAW, FALSE, 0);
            ListView_SetItemState(lv, -1, 0, LVIS_SELECTED | LVIS_FOCUSED); // deselect all
            for (unsigned int i = 0; i < selHardwareAddrs.size(); i++)
            {
                // calculate index ranges of this selection
                const AddrRange& range = selHardwareAddrs[i];
                auto selRangeTop = CALL_WITH_T_SIZE_TYPES(HardwareAddressToItemIndex, rs_type_size, rs_type, noMisalign, range.addr);
                int selRangeBottom = -1;
                for (auto endAddr = range.End() - 1; endAddr >= range.addr/*selRangeTop*/ && selRangeBottom == -1; endAddr--)
                    selRangeBottom = CALL_WITH_T_SIZE_TYPES(HardwareAddressToItemIndex, rs_type_size, rs_type, noMisalign, endAddr);
                if (selRangeBottom == -1)
                    selRangeBottom = selRangeTop;
                if (selRangeTop == -1)
                    continue;

                //// select the entire range at once without deselecting the other ranges
                //// looks hacky but it works, and the only documentation I found on how to do this was blatantly false and equally hacky anyway
                //POINT pos;
                //ListView_EnsureVisible(lv, selRangeTop, 0);
                //ListView_GetItemPosition(lv, selRangeTop, &pos);
                //SendMessage(lv, WM_LBUTTONDOWN, MK_LBUTTON|MK_CONTROL, MAKELONG(pos.x,pos.y));
                //ListView_EnsureVisible(lv, selRangeBottom, 0);
                //ListView_GetItemPosition(lv, selRangeBottom, &pos);
                //SendMessage(lv, WM_LBUTTONDOWN, MK_LBUTTON|MK_CONTROL|MK_SHIFT, MAKELONG(pos.x,pos.y));

                // select the entire range
                for (int j = selRangeTop; j <= selRangeBottom; j++)
                {
                    ListView_SetItemState(lv, j, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                }
            }

            // restore previous scroll position
            if (newBottomIndex != -1)
                ListView_EnsureVisible(lv, newBottomIndex, 0);
            if (newTopIndex != -1)
                ListView_EnsureVisible(lv, newTopIndex, 0);

            SendMessage(lv, WM_SETREDRAW, TRUE, 0);

            RefreshRamListSelectedCountControlStatus(RamSearchHWnd);

            EnableWindow(GetDlgItem(RamSearchHWnd, IDC_MISALIGN), rs_type_size != Score::Ram::SizeID::_8bits);
        }
        else
        {
            ListView_Update(lv, -1);
        }
        InvalidateRect(lv, nullptr, TRUE);
    }




    LRESULT CustomDraw(LPARAM lParam)
    {
        LPNMLVCUSTOMDRAW lplvcd = (LPNMLVCUSTOMDRAW)lParam;

        switch (lplvcd->nmcd.dwDrawStage)
        {
        case CDDS_PREPAINT:
            return CDRF_NOTIFYITEMDRAW;

        case CDDS_ITEMPREPAINT:
            {
                int rv = CDRF_DODEFAULT;

                if (lplvcd->nmcd.dwItemSpec % 2)
                {
                    // alternate the background color slightly
                    lplvcd->clrTextBk = RGB(248, 248, 255);
                    rv = CDRF_NEWFONT;
                }

                if (!IsSatisfied(lplvcd->nmcd.dwItemSpec))
                {
                    // tint red any items that would be eliminated if a search were to run now
                    lplvcd->clrText = RGB(192, 64, 64);
                    rv = CDRF_NEWFONT;
                }

                return rv;
            }	break;
        }
        return CDRF_DODEFAULT;
    }

    void Update_RAM_Search() //keeps RAM values up to date in the search and watch windows
    {
        if (disableRamSearchUpdate)
            return;

        if (Config::fastforward && (RamWatchHWnd || last_rs_possible > 10000 || Config::recoveringStale) && (Config::localTASflags.fastForwardFlags & /*FFMODE_RAMSKIP*/0x08))
        {
            static int count = 0;

            if (Config::recoveringStale)
            {
                if (++count % 128)
                    return;
            }
            else
            {
                if (++count % 32)
                    return;
            }
        }

        bool prevValuesNeededUpdate;
        if (AutoSearch && !ResultCount)
        {
            if (!AutoSearchAutoRetry)
            {
                //			Clear_Sound_Buffer();
                int answer = MessageBox(RamSearchHWnd, "Choosing Retry will reset the search once and continue autosearching.\nChoose Ignore will reset the search whenever necessary and continue autosearching.\nChoosing Abort will reset the search once and stop autosearching.", "Autosearch - out of results.", MB_ABORTRETRYIGNORE | MB_DEFBUTTON2 | MB_ICONINFORMATION);
                if (answer == IDABORT)
                {
                    SendDlgItemMessage(RamSearchHWnd, IDC_C_AUTOSEARCH, BM_SETCHECK, BST_UNCHECKED, 0);
                    SendMessage(RamSearchHWnd, WM_COMMAND, IDC_C_AUTOSEARCH, 0);
                }
                if (answer == IDIGNORE)
                    AutoSearchAutoRetry = true;
            }
            reset_address_info();
            prevValuesNeededUpdate = s_prevValuesNeedUpdate;
        }
        else
        {
            prevValuesNeededUpdate = s_prevValuesNeedUpdate;
            if (RamSearchHWnd)
            {
                // update active RAM values
                signal_new_frame();
            }

            if (AutoSearch && ResultCount)
            {
                //Clear_Sound_Buffer();
                if (!rs_val_valid)
                {
                    rs_val_valid = Set_RS_Val();
                }
                if (rs_val_valid)
                {
                    prune(rs_c, rs_o, rs_type, rs_val, rs_param);
                }
            }
        }

        if (RamSearchHWnd)
        {
            HWND lv = GetDlgItem(RamSearchHWnd, IDC_RAMLIST);
            if (prevValuesNeededUpdate != s_prevValuesNeedUpdate)
            {
                // previous values got updated, refresh everything visible
                ListView_Update(lv, -1);
            }
            else if (theActiveMemoryRegions.IsNumChangesValid())
            {
                // refresh any visible parts of the listview box that changed
                static int changes[128];
                int top = ListView_GetTopIndex(lv);
                int count = ListView_GetCountPerPage(lv);
                int start = -1;
                for (int i = top; i <= top + count; i++)
                {
                    int changeNum = CALL_WITH_T_SIZE_TYPES(GetNumChangesFromItemIndex, rs_type_size, rs_type, noMisalign, i); //s_numChanges[i];
                    int changed = changeNum != changes[i - top];
                    if (changed)
                        changes[i - top] = changeNum;

                    if (start == -1)
                    {
                        if (i != top + count && changed)
                        {
                            start = i;
                            //somethingChanged = true;
                        }
                    }
                    else
                    {
                        if (i == top + count || !changed)
                        {
                            ListView_RedrawItems(lv, start, i - 1);
                            start = -1;
                        }
                    }
                }
            }
        }

        if (RamWatchHWnd)
        {
            Update_RAM_Watch();
        }
    }

    static int rs_lastPercent = -1;
    inline void UpdateRamSearchProgressBar(int percent)
    {
        if (rs_lastPercent != percent)
        {
            rs_lastPercent = percent;
            UpdateRamSearchTitleBar(percent);
        }
    }

    static void SelectEditControl(int controlID)
    {
        HWND hEdit = GetDlgItem(RamSearchHWnd, controlID);
        SetFocus(hEdit);
        SendMessage(hEdit, EM_SETSEL, 0, -1);
    }

    static BOOL SelectingByKeyboard()
    {
        int a = GetKeyState(VK_LEFT);
        int b = GetKeyState(VK_RIGHT);
        int c = GetKeyState(VK_UP);
        int d = GetKeyState(VK_DOWN); // space and tab are intentionally omitted
        return (a | b | c | d) & 0x80;
    }


    LRESULT CALLBACK RamSearchProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        RECT r;
        RECT r2;
        int dx1, dy1, dx2, dy2;
        static int watchIndex = 0;

        switch (uMsg)
        {
        case WM_INITDIALOG:
        {
            RamSearchHWnd = hDlg;

            GetWindowRect(hWnd, &r);
            dx1 = (r.right - r.left) / 2;
            dy1 = (r.bottom - r.top) / 2;

            GetWindowRect(hDlg, &r2);
            dx2 = (r2.right - r2.left) / 2;
            dy2 = (r2.bottom - r2.top) / 2;

            // push it away from the main window if we can
            const int width = (r.right - r.left);
            const int width2 = (r2.right - r2.left);
            if (r.left + width2 + width < GetSystemMetrics(SM_CXSCREEN))
            {
                r.right += width;
                r.left += width;
            }
            else if ((int)r.left - (int)width2 > 0)
            {
                r.right -= width2;
                r.left -= width2;
            }

            SetWindowPos(hDlg, nullptr, r.left, r.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
            switch (rs_o)
            {
            case '<':
                SendDlgItemMessage(hDlg, IDC_LESSTHAN, BM_SETCHECK, BST_CHECKED, 0);
                break;
            case '>':
                SendDlgItemMessage(hDlg, IDC_MORETHAN, BM_SETCHECK, BST_CHECKED, 0);
                break;
            case 'l':
                SendDlgItemMessage(hDlg, IDC_NOMORETHAN, BM_SETCHECK, BST_CHECKED, 0);
                break;
            case 'm':
                SendDlgItemMessage(hDlg, IDC_NOLESSTHAN, BM_SETCHECK, BST_CHECKED, 0);
                break;
            case '=':
                SendDlgItemMessage(hDlg, IDC_EQUALTO, BM_SETCHECK, BST_CHECKED, 0);
                break;
            case '!':
                SendDlgItemMessage(hDlg, IDC_DIFFERENTFROM, BM_SETCHECK, BST_CHECKED, 0);
                break;
            case 'd':
                SendDlgItemMessage(hDlg, IDC_DIFFERENTBY, BM_SETCHECK, BST_CHECKED, 0);
                EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DIFFBY), true);
                break;
            case '%':
                SendDlgItemMessage(hDlg, IDC_MODULO, BM_SETCHECK, BST_CHECKED, 0);
                EnableWindow(GetDlgItem(hDlg, IDC_EDIT_MODBY), true);
                break;
            }
            switch (rs_c)
            {
            case 'r':
                SendDlgItemMessage(hDlg, IDC_PREVIOUSVALUE, BM_SETCHECK, BST_CHECKED, 0);
                break;
            case 's':
                SendDlgItemMessage(hDlg, IDC_SPECIFICVALUE, BM_SETCHECK, BST_CHECKED, 0);
                EnableWindow(GetDlgItem(hDlg, IDC_EDIT_COMPAREVALUE), true);
                break;
            case 'a':
                SendDlgItemMessage(hDlg, IDC_SPECIFICADDRESS, BM_SETCHECK, BST_CHECKED, 0);
                EnableWindow(GetDlgItem(hDlg, IDC_EDIT_COMPAREADDRESS), true);
                break;
            case 'n':
                SendDlgItemMessage(hDlg, IDC_NUMBEROFCHANGES, BM_SETCHECK, BST_CHECKED, 0);
                EnableWindow(GetDlgItem(hDlg, IDC_EDIT_COMPARECHANGES), true);
                break;
            }
            switch (rs_type)
            {
            case Score::Ram::TypeID::SignedDecimal:
                SendDlgItemMessage(hDlg, IDC_SIGNED, BM_SETCHECK, BST_CHECKED, 0);
                EnableWindow(GetDlgItem(hDlg, IDC_1_BYTE), true);
                EnableWindow(GetDlgItem(hDlg, IDC_2_BYTES), true);
                break;
            case Score::Ram::TypeID::UnsignedDecimal:
                SendDlgItemMessage(hDlg, IDC_UNSIGNED, BM_SETCHECK, BST_CHECKED, 0);
                EnableWindow(GetDlgItem(hDlg, IDC_1_BYTE), true);
                EnableWindow(GetDlgItem(hDlg, IDC_2_BYTES), true);
                break;
            case Score::Ram::TypeID::Hexadecimal:
                SendDlgItemMessage(hDlg, IDC_HEX, BM_SETCHECK, BST_CHECKED, 0);
                EnableWindow(GetDlgItem(hDlg, IDC_1_BYTE), true);
                EnableWindow(GetDlgItem(hDlg, IDC_2_BYTES), true);
                break;
            case Score::Ram::TypeID::Float:
                SendDlgItemMessage(hDlg, IDC_FLOAT, BM_SETCHECK, BST_CHECKED, 0);
                EnableWindow(GetDlgItem(hDlg, IDC_1_BYTE), false);
                EnableWindow(GetDlgItem(hDlg, IDC_2_BYTES), false);
                break;
            }
            switch (rs_type_size)
            {
            case Score::Ram::SizeID::_8bits:
                SendDlgItemMessage(hDlg, IDC_1_BYTE, BM_SETCHECK, BST_CHECKED, 0);
                break;
            case Score::Ram::SizeID::_16bits:
                SendDlgItemMessage(hDlg, IDC_2_BYTES, BM_SETCHECK, BST_CHECKED, 0);
                break;
            case Score::Ram::SizeID::_32bits:
                SendDlgItemMessage(hDlg, IDC_4_BYTES, BM_SETCHECK, BST_CHECKED, 0);
                break;
            case Score::Ram::SizeID::_64bits:
                SendDlgItemMessage(hDlg, IDC_8_BYTES, BM_SETCHECK, BST_CHECKED, 0);
                break;
            }

            s_prevValuesNeedUpdate = true;

            SendDlgItemMessage(hDlg, IDC_C_AUTOSEARCH, BM_SETCHECK, AutoSearch ? BST_CHECKED : BST_UNCHECKED, 0);
            //const char* names[5] = {"Address","Value","Previous","Changes","Notes"};
            //int widths[5] = {62,64,64,55,55};
            const char* names[] = { "Address", "Value", "Previous", "Changes" };
            int widths[4] = { 68, 76, 76, 68 };
            if (!ResultCount)
                reset_address_info();
            else
            {
                signal_new_frame();
                CompactAddrs();
            }
            void init_list_box(HWND Box, const char* Strs[], int numColumns, int *columnWidths);
            init_list_box(GetDlgItem(hDlg, IDC_RAMLIST), names, 4, widths);
            //ListView_SetItemCount(GetDlgItem(hDlg,IDC_RAMLIST),ResultCount);
            if (!noMisalign) SendDlgItemMessage(hDlg, IDC_MISALIGN, BM_SETCHECK, BST_CHECKED, 0);
            //if (littleEndian) SendDlgItemMessage(hDlg, IDC_ENDIAN, BM_SETCHECK, BST_CHECKED, 0);
            last_rs_possible = -1;
            RefreshRamListSelectedCountControlStatus(hDlg);

            // force misalign checkbox to refresh
            signal_new_size();

            // force undo button to refresh
            int undoType = s_undoType;
            SetRamSearchUndoType(hDlg, -2);
            SetRamSearchUndoType(hDlg, undoType);

            // force possibility count to refresh
            last_rs_possible--;
            UpdatePossibilities(ResultCount, theActiveMemoryRegions.Count());

            rs_val_valid = Set_RS_Val();

            ListView_SetCallbackMask(GetDlgItem(hDlg, IDC_RAMLIST), LVIS_FOCUSED | LVIS_SELECTED);

            return true;
        }

        case WM_NOTIFY:
        {
            LPNMHDR lP = (LPNMHDR)lParam;
            switch (lP->code)
            {
            case LVN_ITEMCHANGED: // selection changed event
            {
                NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)lP;
                if (pNMListView->uNewState & LVIS_FOCUSED ||
                    (pNMListView->uNewState ^ pNMListView->uOldState) & LVIS_SELECTED)
                {
                    // disable buttons that we don't have the right number of selected items for
                    RefreshRamListSelectedCountControlStatus(hDlg);
                }
                break;
            }

            case LVN_GETDISPINFO:
            {
                LV_DISPINFO *Item = (LV_DISPINFO *)lParam;
                Item->item.mask = LVIF_TEXT;
                Item->item.state = 0;
                Item->item.iImage = 0;
                const unsigned int iNum = Item->item.iItem;
                static char num[64];
                switch (Item->item.iSubItem)
                {
                case 0:
                {
                    auto addr = CALL_WITH_T_SIZE_TYPES(GetHardwareAddressFromItemIndex, rs_type_size, rs_type, noMisalign, iNum);
                    sprintf(num, "%08X", addr);
                    Item->item.pszText = num;
                    return true;
                }
                case 1:
                {
                    auto rsval = Score::Ram::Value(CALL_WITH_T_SIZE_TYPES(GetCurValueFromItemIndex, rs_type_size, rs_type, noMisalign, iNum));
                    rsval.sprint(num, rs_type_size, rs_type);
                    Item->item.pszText = num;
                    return true;
                }
                case 2:
                {
                    auto rsval = Score::Ram::Value(CALL_WITH_T_SIZE_TYPES(GetPrevValueFromItemIndex, rs_type_size, rs_type, noMisalign, iNum));
                    rsval.sprint(num, rs_type_size, rs_type);
                    Item->item.pszText = num;
                    return true;
                }
                case 3:
                {
                    int i = CALL_WITH_T_SIZE_TYPES(GetNumChangesFromItemIndex, rs_type_size, rs_type, noMisalign, iNum);
                    sprintf(num, "%d", i);
                    Item->item.pszText = num;
                    return true;
                }
                //case 4:
                //	Item->item.pszText = rsaddrs[rsresults[iNum].Index].comment ? rsaddrs[rsresults[iNum].Index].comment : "";
                //	return true;
                default:
                    return false;
                }
            }

            case NM_CUSTOMDRAW:
            {
                SetWindowLong(hDlg, DWL_MSGRESULT, CustomDraw(lParam));
                return TRUE;
            }

            //case LVN_ODCACHEHINT: //Copied this bit from the MSDN virtual listbox code sample. Eventually it should probably do something.
            //{
            //	LPNMLVCACHEHINT   lpCacheHint = (LPNMLVCACHEHINT)lParam;
            //	return 0;
            //}
            //case LVN_ODFINDITEM: //Copied this bit from the MSDN virtual listbox code sample. Eventually it should probably do something.
            //{	
            //	LPNMLVFINDITEM lpFindItem = (LPNMLVFINDITEM)lParam;
            //	return 0;
            //}
            }
            break;
        }

        case WM_COMMAND:
            {
                int rv = false;
                switch (LOWORD(wParam))
                {
                case IDC_SIGNED:
                    rs_type = Score::Ram::TypeID::SignedDecimal;
                    signal_new_size();
                    EnableWindow(GetDlgItem(hDlg, IDC_1_BYTE), true);
                    EnableWindow(GetDlgItem(hDlg, IDC_2_BYTES), true);
                    {rv = true; break; }
                case IDC_UNSIGNED:
                    rs_type = Score::Ram::TypeID::UnsignedDecimal;
                    signal_new_size();
                    EnableWindow(GetDlgItem(hDlg, IDC_1_BYTE), true);
                    EnableWindow(GetDlgItem(hDlg, IDC_2_BYTES), true);
                    {rv = true; break; }
                case IDC_HEX:
                    rs_type = Score::Ram::TypeID::Hexadecimal;
                    signal_new_size();
                    EnableWindow(GetDlgItem(hDlg, IDC_1_BYTE), true);
                    EnableWindow(GetDlgItem(hDlg, IDC_2_BYTES), true);
                    {rv = true; break; }
                case IDC_FLOAT:
                    rs_type = Score::Ram::TypeID::Float;
                    if (rs_type_size == Score::Ram::SizeID::_8bits || rs_type_size == Score::Ram::SizeID::_16bits)
                    {
                        SendDlgItemMessage(hDlg, IDC_4_BYTES, BM_SETCHECK, BST_CHECKED, 0);
                        SendDlgItemMessage(hDlg, IDC_8_BYTES, BM_SETCHECK, BST_UNCHECKED, 0);
                        SendDlgItemMessage(hDlg, IDC_2_BYTES, BM_SETCHECK, BST_UNCHECKED, 0);
                        SendDlgItemMessage(hDlg, IDC_1_BYTE, BM_SETCHECK, BST_UNCHECKED, 0);
                        rs_type_size = Score::Ram::SizeID::_32bits;
                    }
                    EnableWindow(GetDlgItem(hDlg, IDC_1_BYTE), false);
                    EnableWindow(GetDlgItem(hDlg, IDC_2_BYTES), false);
                    signal_new_size();
                    {rv = true; break; }
                case IDC_1_BYTE:
                    rs_type_size = Score::Ram::SizeID::_8bits;
                    signal_new_size();
                    {rv = true; break; }
                case IDC_2_BYTES:
                    rs_type_size = Score::Ram::SizeID::_16bits;
                    signal_new_size();
                    {rv = true; break; }
                case IDC_4_BYTES:
                    rs_type_size = Score::Ram::SizeID::_32bits;
                    signal_new_size();
                    {rv = true; break; }
                case IDC_8_BYTES:
                    rs_type_size = Score::Ram::SizeID::_64bits;
                    signal_new_size();
                    {rv = true; break; }
                case IDC_MISALIGN:
                    noMisalign = !noMisalign;
                    //CompactAddrs();
                    signal_new_size();
                    {rv = true; break; }
                    //				case IDC_ENDIAN:
                    ////					littleEndian = !littleEndian;
                    ////					signal_new_size();
                    //					{rv = true; break;}				
                case IDC_LESSTHAN:
                    EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DIFFBY), false);
                    EnableWindow(GetDlgItem(hDlg, IDC_EDIT_MODBY), false);
                    rs_o = '<';
                    {rv = true; break; }
                case IDC_MORETHAN:
                    EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DIFFBY), false);
                    EnableWindow(GetDlgItem(hDlg, IDC_EDIT_MODBY), false);
                    rs_o = '>';
                    {rv = true; break; }
                case IDC_NOMORETHAN:
                    EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DIFFBY), false);
                    EnableWindow(GetDlgItem(hDlg, IDC_EDIT_MODBY), false);
                    rs_o = 'l';
                    {rv = true; break; }
                case IDC_NOLESSTHAN:
                    EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DIFFBY), false);
                    EnableWindow(GetDlgItem(hDlg, IDC_EDIT_MODBY), false);
                    rs_o = 'm';
                    {rv = true; break; }
                case IDC_EQUALTO:
                    EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DIFFBY), false);
                    EnableWindow(GetDlgItem(hDlg, IDC_EDIT_MODBY), false);
                    rs_o = '=';
                    {rv = true; break; }
                case IDC_DIFFERENTFROM:
                    EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DIFFBY), false);
                    EnableWindow(GetDlgItem(hDlg, IDC_EDIT_MODBY), false);
                    rs_o = '!';
                    {rv = true; break; }
                case IDC_DIFFERENTBY:
                    {
                        rs_o = 'd';
                        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DIFFBY), true);
                        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_MODBY), false);
                        if (!SelectingByKeyboard())
                            SelectEditControl(IDC_EDIT_DIFFBY);
                    }	{rv = true; break; }
                case IDC_MODULO:
                    {
                        rs_o = '%';
                        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_DIFFBY), false);
                        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_MODBY), true);
                        if (!SelectingByKeyboard())
                            SelectEditControl(IDC_EDIT_MODBY);
                    }	{rv = true; break; }
                case IDC_PREVIOUSVALUE:
                    rs_c = 'r';
                    EnableWindow(GetDlgItem(hDlg, IDC_EDIT_COMPAREVALUE), false);
                    EnableWindow(GetDlgItem(hDlg, IDC_EDIT_COMPAREADDRESS), false);
                    EnableWindow(GetDlgItem(hDlg, IDC_EDIT_COMPARECHANGES), false);
                    {rv = true; break; }
                case IDC_SPECIFICVALUE:
                    {
                        rs_c = 's';
                        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_COMPAREVALUE), true);
                        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_COMPAREADDRESS), false);
                        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_COMPARECHANGES), false);
                        if (!SelectingByKeyboard())
                            SelectEditControl(IDC_EDIT_COMPAREVALUE);
                        {rv = true; break; }
                    }
                case IDC_SPECIFICADDRESS:
                    {
                        rs_c = 'a';
                        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_COMPAREADDRESS), true);
                        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_COMPAREVALUE), false);
                        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_COMPARECHANGES), false);
                        if (!SelectingByKeyboard())
                            SelectEditControl(IDC_EDIT_COMPAREADDRESS);
                    }	{rv = true; break; }
                case IDC_NUMBEROFCHANGES:
                    {
                        rs_c = 'n';
                        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_COMPARECHANGES), true);
                        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_COMPAREVALUE), false);
                        EnableWindow(GetDlgItem(hDlg, IDC_EDIT_COMPAREADDRESS), false);
                        if (!SelectingByKeyboard())
                            SelectEditControl(IDC_EDIT_COMPARECHANGES);
                    }	{rv = true; break; }
                case IDC_C_ADDCHEAT:
                    {
                        //HWND ramListControl = GetDlgItem(hDlg,IDC_RAMLIST);
                        //int cheatItemIndex = ListView_GetNextItem(ramListControl, -1, LVNI_SELECTED);
                        //while (cheatItemIndex >= 0)
                        //{
                        //	u32 address = CALL_WITH_T_SIZE_TYPES(GetHardwareAddressFromItemIndex, rs_type_size,rs_type,noMisalign, cheatItemIndex);
                        //	u8 size = (rs_type_size==Score::Ram::SizeID::_8bits) ? 1 : (rs_type_size==Score::Ram::SizeID::_16bits ? 2 : 4);
                        //	u32 value = CALL_WITH_T_SIZE_TYPES(GetCurValueFromItemIndex, rs_type_size,rs_type,noMisalign, cheatItemIndex);
                        //	CheatsAddDialog(hDlg, address, value, size);
                        //	cheatItemIndex = ListView_GetNextItem(ramListControl, cheatItemIndex, LVNI_SELECTED);
                        //}
                        //{rv = true; break;}
                    }
                case IDC_C_RESET:
                    {
                        //RamSearchSaveUndoStateIfNotTooBig(RamSearchHWnd);
                        int prevNumItems = last_rs_possible;

                        soft_reset_address_info();

                        if (prevNumItems == last_rs_possible)
                            SetRamSearchUndoType(RamSearchHWnd, 0); // nothing to undo

                        ListView_SetItemState(GetDlgItem(hDlg, IDC_RAMLIST), -1, 0, LVIS_SELECTED); // deselect all
                        //ListView_SetItemCount(GetDlgItem(hDlg,IDC_RAMLIST),ResultCount);
                        ListView_SetSelectionMark(GetDlgItem(hDlg, IDC_RAMLIST), 0);
                        RefreshRamListSelectedCountControlStatus(hDlg);
                        {rv = true; break; }
                    }
                case IDC_C_RESET_CHANGES:
                    theActiveMemoryRegions.ResetChanges();
                    ListView_Update(GetDlgItem(hDlg, IDC_RAMLIST), -1);
                    //SetRamSearchUndoType(hDlg, 0);
                    {rv = true; break; }
                //case IDC_C_UNDO:
                //    if (s_undoType > 0)
                //    {
                //        //						Clear_Sound_Buffer();
                //        EnterCriticalSection(&s_activeMemoryRegionsCS);
                //        if (theActiveMemoryRegions.Count() < tooManyRegionsForUndo)
                //        {
                //            MemoryList tempMemoryList = s_activeMemoryRegions;
                //            s_activeMemoryRegions = s_activeMemoryRegionsBackup;
                //            s_activeMemoryRegionsBackup = tempMemoryList;
                //            LeaveCriticalSection(&s_activeMemoryRegionsCS);
                //            SetRamSearchUndoType(hDlg, 3 - s_undoType);
                //        }
                //        else
                //        {
                //            s_activeMemoryRegions = s_activeMemoryRegionsBackup;
                //            LeaveCriticalSection(&s_activeMemoryRegionsCS);
                //            SetRamSearchUndoType(hDlg, -1);
                //        }
                //        CompactAddrs();
                //        ListView_SetItemState(GetDlgItem(hDlg, IDC_RAMLIST), -1, 0, LVIS_SELECTED); // deselect all
                //        ListView_SetSelectionMark(GetDlgItem(hDlg, IDC_RAMLIST), 0);
                //        RefreshRamListSelectedCountControlStatus(hDlg);
                //    }
                //    {rv = true; break; }
                case IDC_C_AUTOSEARCH:
                    AutoSearch = SendDlgItemMessage(hDlg, IDC_C_AUTOSEARCH, BM_GETCHECK, 0, 0) != 0;
                    AutoSearchAutoRetry = false;
                    if (!AutoSearch) { rv = true; break; }
                case IDC_C_SEARCH:
                    {
                        //					Clear_Sound_Buffer();

                        if (!rs_val_valid && !(rs_val_valid = Set_RS_Val()))
                            goto invalid_field;

                        if (ResultCount)
                        {
                            //RamSearchSaveUndoStateIfNotTooBig(hDlg);

                            prune(rs_c, rs_o, rs_type, rs_val, rs_param);

                            RefreshRamListSelectedCountControlStatus(hDlg);
                        }

                        if (!ResultCount)
                        {

                            MessageBox(RamSearchHWnd, "Resetting search.", "Out of results.", MB_OK | MB_ICONINFORMATION);
                            soft_reset_address_info();
                        }

                    {rv = true; break; }

                invalid_field:
                    MessageBox(RamSearchHWnd, "Invalid or out-of-bound entered value.", "Error", MB_OK | MB_ICONSTOP);
                    if (AutoSearch) // stop autosearch if it just started
                    {
                        SendDlgItemMessage(hDlg, IDC_C_AUTOSEARCH, BM_SETCHECK, BST_UNCHECKED, 0);
                        SendMessage(hDlg, WM_COMMAND, IDC_C_AUTOSEARCH, 0);
                    }
                    {rv = true; break; }
                    }
                case IDC_C_WATCH:
                    {
                        HWND ramListControl = GetDlgItem(hDlg, IDC_RAMLIST);
                        int selCount = ListView_GetSelectedCount(ramListControl);

                        bool inserted = false;
                        int watchItemIndex = ListView_GetNextItem(ramListControl, -1, LVNI_SELECTED);
                        while (watchItemIndex >= 0)
                        {
                            Score::Ram::Watch::Watcher tempWatch;
                            tempWatch.myAddress = CALL_WITH_T_SIZE_TYPES(GetHardwareAddressFromItemIndex, rs_type_size, rs_type, noMisalign, watchItemIndex);
                            tempWatch.mySizeID = rs_type_size;
                            tempWatch.myTypeID = rs_type;
                            tempWatch.myEndianness = Score::Ram::Endianness::Little; //Replace when I get little endian working
                            //tempWatch.myDescription.clear();

                            if (selCount == 1)
                            {
                                inserted |= InsertWatch(tempWatch, hDlg);
                            }
                            else
                            {
                                inserted |= InsertWatch(tempWatch, "");
                            }

                            watchItemIndex = ListView_GetNextItem(ramListControl, watchItemIndex, LVNI_SELECTED);
                        }
                        // bring up the ram watch window if it's not already showing so the user knows where the watch went
                        if (inserted && !RamWatchHWnd)
                            SendMessage(hWnd, WM_COMMAND, ID_RAM_WATCH, 0);
                        SetForegroundWindow(RamSearchHWnd);
                        {rv = true; break; }
                    }

                //    // eliminate all selected items
                //case IDC_C_ELIMINATE:
                //    {
                //        //RamSearchSaveUndoStateIfNotTooBig(hDlg);

                //        HWND ramListControl = GetDlgItem(hDlg, IDC_RAMLIST);
                //        int size = noMisalign ? sizeTypeIDToSize(rs_type_size) : 1;
                //        int selCount = ListView_GetSelectedCount(ramListControl);
                //        int watchIndex = -1;

                //        // time-saving trick #1:
                //        // condense the selected items into an array of address ranges
                //        std::vector<AddrRange> selHardwareAddrs;
                //        for (int i = 0, j = 1024; i < selCount; ++i, --j)
                //        {
                //            watchIndex = ListView_GetNextItem(ramListControl, watchIndex, LVNI_SELECTED);
                //            auto addr = CALL_WITH_T_SIZE_TYPES(GetHardwareAddressFromItemIndex, rs_type_size, rs_type, noMisalign, watchIndex);
                //            if (!selHardwareAddrs.empty() && addr == selHardwareAddrs.back().End())
                //            {
                //                selHardwareAddrs.back().size += size;
                //            }
                //            else
                //            {
                //                selHardwareAddrs.push_back(AddrRange(addr, size));
                //            }

                //            if (!j) UpdateRamSearchProgressBar(i * 50 / selCount), j = 1024;
                //        }

                //        // now deactivate the ranges

                //        // time-saving trick #2:
                //        // take advantage of the fact that the listbox items must be in the same order as the regions
                //        MemoryList::iterator iter = s_activeMemoryRegions.begin();
                //        int numHardwareAddrRanges = selHardwareAddrs.size();
                //        for (int i = 0, j = 16; i < numHardwareAddrRanges; ++i, --j)
                //        {
                //            auto addr = selHardwareAddrs[i].addr;
                //            int size = selHardwareAddrs[i].size;
                //            bool affected = false;
                //            while (iter != s_activeMemoryRegions.end())
                //            {
                //                MemoryRegion& region = *iter;
                //                int affNow = DeactivateRegion(region, iter, addr, size);
                //                if (affNow)
                //                    affected = true;
                //                else if (affected)
                //                    break;
                //                if (affNow != 2)
                //                    ++iter;
                //            }

                //            if (!j) UpdateRamSearchProgressBar(50 + (i * 50 / selCount)), j = 16;
                //        }
                //        UpdateRamSearchTitleBar();

                //        // careful -- if the above two time-saving tricks aren't working,
                //        // the runtime can absolutely explode (seconds -> hours) when there are lots of regions

                //        ListView_SetItemState(ramListControl, -1, 0, LVIS_SELECTED); // deselect all
                //        signal_new_size();
                //        {rv = true; break; }
                //    }
                    //case IDOK:
                case IDCANCEL:
                    RamSearchHWnd = nullptr;
                    EndDialog(hDlg, true);
                    {rv = true; break; }
                }

                // check refresh for comparison preview color update
                // also, update rs_val if needed
                bool needRefresh = false;
                switch (LOWORD(wParam))
                {
                case IDC_LESSTHAN:
                case IDC_MORETHAN:
                case IDC_NOMORETHAN:
                case IDC_NOLESSTHAN:
                case IDC_EQUALTO:
                case IDC_DIFFERENTFROM:
                case IDC_DIFFERENTBY:
                case IDC_MODULO:
                case IDC_PREVIOUSVALUE:
                case IDC_SPECIFICVALUE:
                case IDC_SPECIFICADDRESS:
                case IDC_NUMBEROFCHANGES:
                case IDC_SIGNED:
                case IDC_UNSIGNED:
                case IDC_HEX:
                case IDC_FLOAT:
                    rs_val_valid = Set_RS_Val();
                    needRefresh = true;
                    break;
                case IDC_EDIT_COMPAREVALUE:
                case IDC_EDIT_COMPAREADDRESS:
                case IDC_EDIT_COMPARECHANGES:
                case IDC_EDIT_DIFFBY:
                case IDC_EDIT_MODBY:
                    if (HIWORD(wParam) == EN_CHANGE)
                    {
                        rs_val_valid = Set_RS_Val();
                        needRefresh = true;
                    }
                    break;
                }
                if (needRefresh)
                    ListView_Update(GetDlgItem(hDlg, IDC_RAMLIST), -1);


                return rv;
            }	break;

        case WM_CLOSE:
            RamSearchHWnd = nullptr;
            EndDialog(hDlg, true);
            return true;
        }

        return false;
    }

    void UpdateRamSearchTitleBar(int percent)
    {
#define HEADER_STR " RAM Search - "
#define PROGRESS_STR " %d%% ... "
#define STATUS_STR "%d Possibilit%s (%d Region%s)"

        int poss = last_rs_possible;
        int regions = last_rs_regions;
        if (poss <= 0)
            strcpy(Str_Tmp_RS, " RAM Search");
        else if (percent <= 0)
            sprintf(Str_Tmp_RS, HEADER_STR STATUS_STR, poss, poss == 1 ? "y" : "ies", regions, regions == 1 ? "" : "s");
        else
            sprintf(Str_Tmp_RS, PROGRESS_STR STATUS_STR, percent, poss, poss == 1 ? "y" : "ies", regions, regions == 1 ? "" : "s");
        SetWindowText(RamSearchHWnd, Str_Tmp_RS);
    }

    void UpdatePossibilities(int rs_possible, int regions)
    {
        if (rs_possible != last_rs_possible)
        {
            last_rs_possible = rs_possible;
            last_rs_regions = regions;
            UpdateRamSearchTitleBar();
        }
    }

    void SetRamSearchUndoType(HWND hDlg, int type)
    {
        if (s_undoType != type)
        {
            if ((s_undoType != 2 && s_undoType != -1) != (type != 2 && type != -1))
                SendDlgItemMessage(hDlg, IDC_C_UNDO, WM_SETTEXT, 0, (LPARAM)((type == 2 || type == -1) ? "Redo" : "Undo"));
            if ((s_undoType > 0) != (type > 0))
                EnableWindow(GetDlgItem(hDlg, IDC_C_UNDO), type > 0);
            s_undoType = type;
        }
    }

    //void RamSearchSaveUndoStateIfNotTooBig(HWND hDlg)
    //{
    //    EnterCriticalSection(&s_activeMemoryRegionsCS);
    //    if (s_activeMemoryRegions.size() < tooManyRegionsForUndo)
    //    {
    //        s_activeMemoryRegionsBackup = s_activeMemoryRegions;
    //        LeaveCriticalSection(&s_activeMemoryRegionsCS);
    //        SetRamSearchUndoType(hDlg, 1);
    //    }
    //    else
    //    {
    //        LeaveCriticalSection(&s_activeMemoryRegionsCS);
    //        SetRamSearchUndoType(hDlg, 0);
    //    }
    //}

    void InitRamSearch()
    {
        InitializeCriticalSection(&s_activeMemoryRegionsCS);
    }


    void init_list_box(HWND Box, const char* Strs[], int numColumns, int *columnWidths) //initializes the ram search and/or ram watch listbox
    {
        LVCOLUMN Col;
        Col.mask = LVCF_FMT | LVCF_ORDER | LVCF_SUBITEM | LVCF_TEXT | LVCF_WIDTH;
        Col.fmt = LVCFMT_CENTER;
        for (int i = 0; i < numColumns; i++)
        {
            Col.iOrder = i;
            Col.iSubItem = i;
            Col.pszText = (LPSTR)(Strs[i]);
            Col.cx = columnWidths[i];
            ListView_InsertColumn(Box, i, &Col);
        }

        ListView_SetExtendedListViewStyle(Box, LVS_EX_FULLROWSELECT);
    }

    void DeallocateRamSearch()
    {
        if (RamSearchHWnd)
        {
            ListView_SetItemCount(GetDlgItem(RamSearchHWnd, IDC_RAMLIST), 0);
            RefreshRamListSelectedCountControlStatus(RamSearchHWnd);
            last_rs_possible--;
            UpdatePossibilities(0, 0);
        }

        theActiveMemoryRegions.FreeAll();

        s_prevValuesNeedUpdate = true;

        s_undoType = 0;

        //EnterCriticalSection(&s_activeMemoryRegionsCS);
        //MemoryList temp1; s_activeMemoryRegions.swap(temp1);
        //MemoryList temp2; s_activeMemoryRegionsBackup.swap(temp2);
        //LeaveCriticalSection(&s_activeMemoryRegionsCS);
    }

