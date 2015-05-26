#pragma once

#include <vector>
#include <functional>
#include "Watcher.h"

namespace Score
{
    namespace Ram
    {
        namespace Watch
        {
            class WatcherCollection final
            {
            public:
                typedef std::function<void()> Callback_t;

                        auto    Insert(const Watcher& aTemplate, Callback_t anAlreadyPresentCallback = nullptr, Callback_t aBeforeAddCallback = nullptr,
                                        Callback_t anAfterAddCallback = nullptr, const char* aDescription = nullptr) -> bool;
                        auto    UpdateCachedValuesAndHasChangedFlags()    -> void;

                inline  auto    Empty() -> bool;
                inline  auto    Count() -> size_t;
                inline  auto    operator[](size_t anIndex)->Watcher&;

                        auto    ReadFile(const char* aFileName, Callback_t anFileOpenFailedCallback) -> bool;
                        auto    WriteFile(const char* aFileName) -> void;
                        auto    Clear() -> void;
                        auto    Remove(size_t anIndex) -> void;
                        auto    Remove(const Watcher& aWatcher) -> void;

                        auto    CheckIfAlreadyPresent(const Watcher& aWatcher) -> bool;

            protected:
            private:

                std::vector<Watcher> myData;
            };

            inline auto WatcherCollection::Empty() -> bool
            {
                return myData.empty();
            }

            inline auto WatcherCollection::Count()->size_t
            {
                return myData.size();
            }

            inline  auto WatcherCollection::operator[](size_t anIndex)->Watcher&
            {
                return myData[anIndex];
            }

        }
    }
}