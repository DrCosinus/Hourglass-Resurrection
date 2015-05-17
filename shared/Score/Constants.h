#pragma once

namespace Score
{
#if defined(_USRDLL)
    enum { IsDll = 1 };
#else
    enum { IsDll = 0 };
#endif
}