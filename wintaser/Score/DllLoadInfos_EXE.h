#include <shared/Score/DllLoadInfos_SHARED.h>

namespace Score
{
    namespace Exe
    {
        class DllLoadInfos final : public LoadedDllList
        {
        public:
            auto    AddAndSend(const char* filename, bool loaded) -> void;
        };

    }
    extern Exe::DllLoadInfos theDllLoadInfos;
}