#pragma once

#include <list>
#include "MemoryRegion.h"

namespace Score
{
    namespace Ram
    {
        namespace Search
        {
            class MemoryRegionCollection final
            {
            public:
                using Container_t = std::list<MemoryRegion>;
                using ContainerIterator_t = Container_t::iterator;

                template<typename compareType>
                using ComparisonCallback_t = bool(*)(compareType, compareType, compareType);

                enum class DeactivateResult
                {
                    NoEffect,
                    Changed,
                    ChangedAndMoved
                };

                inline  auto    Empty                   () const -> bool;
                inline  auto    Clear                   () -> void;
                inline  auto    Count                   () const -> size_t;
                /*  */  auto    ResetAll                () -> void;
                /*  */  auto    ResetChanges            () -> void;
                /*  */  auto    FreeAll                 () -> void;
                /*  */  auto    CopyCurrentToPrevious   () -> void;
                inline  auto    IsNumChangesValid       () const -> bool;
                // warning: can be slow
                /*  */  auto    CalculateItemIndices(int anItemSize) -> void;
                /*  */  auto    Deactivate(MemoryRegion& region, ContainerIterator_t& iter, HardwareAddress hardwareAddress, size_t size)->DeactivateResult;
                inline  auto    GetNumChangesFromVirtualIndex(unsigned int virtualIndex) -> unsigned short;

                template<size_t stepSize, typename>
                /*  */  auto    CountRegionItemsT() -> size_t;

                template<size_t stepSize, typename compareType>
                /*  */  auto    UpdateRegionsT() -> void;

#pragma region Compare to type functions
                template<size_t stepSize, typename compareType>
                /*  */  auto    SearchRelative(ComparisonCallback_t<compareType> cmpFun, compareType, compareType param) -> void;
                template<size_t stepSize, typename compareType>
                /*  */  auto    SearchSpecific(ComparisonCallback_t<compareType> cmpFun, compareType value, compareType param) -> void;
                template<size_t stepSize, typename compareType>
                /*  */  auto    SearchAddress(ComparisonCallback_t<compareType> cmpFun, compareType address, compareType param) -> void;
                template<size_t stepSize, typename compareType>
                /*  */  auto    SearchChanges(ComparisonCallback_t<compareType> cmpFun, compareType changes, compareType param) -> void;
#pragma endregion Compare to type functions

            private:
                template<size_t stepSize, size_t compareSize>
                /*  */  auto    UpdateRegionT(const MemoryRegion& region, const MemoryRegion* nextRegionPtr) -> void;

                std::list<MemoryRegion>     myContainer;
                size_t                      myMaxRamSize = 0;
                size_t                      myMaxItemIndex = 0;                     // static unsigned int s_maxItemIndex = 0; //  max currently valid item index, the listbox sometimes tries to update things past the end of the list so we need to know this to ignore those attempts
                unsigned char*              myPrevValues = nullptr;                 // static unsigned char* s_prevValues = nullptr; // values at last search or reset
                unsigned char*              myCurValues = nullptr;                  // static unsigned char* s_curValues = nullptr; // values at last frame update
                unsigned short*             myNumChanges = nullptr;                 // static unsigned short* s_numChanges = nullptr; // number of changes of the item starting at this virtual index address
                MemoryRegion**              myItemIndexToRegionPointer = nullptr;   // static MemoryRegion** s_itemIndexToRegionPointer = nullptr; // used for random access into the memory list (trading memory size to get speed here, too bad it's so much memory), only valid when s_itemIndicesInvalid is false
                bool                        myPrevValuesNeedUpdate = true;          // static bool s_prevValuesNeedUpdate = true; // if true, the "prev" values should be updated using the "cur" values on the next frame update signaled

                bool                        myItemIndicesInvalid = true; //static bool s_itemIndicesInvalid = true; // if true, the link from listbox items to memory regions (s_itemIndexToRegionPointer) and the link from memory regions to list box items (MemoryRegion::itemIndex) both need to be recalculated
            };

            inline auto MemoryRegionCollection::Empty() const -> bool
            {
                return myContainer.empty();
            }

            inline auto MemoryRegionCollection::Clear() -> void
            {
                myContainer.clear();
            }

            inline auto MemoryRegionCollection::Count() const -> size_t
            {
                return myContainer.size();
            }

            inline auto MemoryRegionCollection::IsNumChangesValid() const -> bool
            {
                return !!myNumChanges;
            }

            inline  auto MemoryRegionCollection::GetNumChangesFromVirtualIndex(size_t aVirtualIndex) -> unsigned short
            {
                return myNumChanges[aVirtualIndex];
            }

        } /* namespace Search */
    } /* namespace Ram */
} /* namespace Score */
