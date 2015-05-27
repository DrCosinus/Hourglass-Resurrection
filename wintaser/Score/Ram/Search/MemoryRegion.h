#pragma once

#include <Score/Ram/HardwareAddress.h>

namespace Score
{
    namespace Ram
    {
        namespace Search
        {
            class MemoryRegion final
            {
            public:
                HardwareAddress hardwareAddress;    // hardware address of the start of this region
                size_t          size;               // number of bytes to the end of this region

                unsigned int    virtualIndex;       // index into s_prevValues, s_curValues, and s_numChanges, valid after being initialized in ResetMemoryRegions()
                unsigned int    itemIndex;          // index into listbox items, valid when s_itemIndicesInvalid is false
            };
        } /* namespace Search */
    } /* namespace Ram */
} /* namespace Score */
