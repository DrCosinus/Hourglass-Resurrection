#include "../../shared/DllLoadInfos_SHARED.h"

namespace Score
{
    namespace Dll
    {
        class DllLoadInfos final : public LoadedDllList
        {
        public:
            auto    UpdateHooks() -> void;
        };
    }
    extern Dll::DllLoadInfos theDllLoadInfos;

}
