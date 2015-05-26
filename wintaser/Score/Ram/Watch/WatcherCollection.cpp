#include "WatcherCollection.h"

#include <algorithm>
#include <windows.h>

extern CRITICAL_SECTION g_processMemCS;

namespace Score
{
    namespace Ram
    {
        namespace Watch
        {
            auto WatcherCollection::CheckIfAlreadyPresent(const Watcher& aWatcher) -> bool
            {
                return std::any_of(myData.begin(), myData.end(),
                    [aWatcher](const Watcher& anOther)
                {
                    return (&anOther != &aWatcher) && (anOther == aWatcher);
                });
            }

            auto WatcherCollection::Insert(const Watcher& aTemplate, Callback_t anAlreadyPresentCallback, Callback_t aBeforeAddCallback, Callback_t anAfterAddCallback, const char* aDescription) -> bool
            {
                if (CheckIfAlreadyPresent(aTemplate))
                {
                    if (anAlreadyPresentCallback)
                    {
                        anAlreadyPresentCallback();
                    }
                    return false;
                }
                else
                {
                    if (aBeforeAddCallback)
                    {
                        aBeforeAddCallback();
                    }

                    myData.emplace_back(aTemplate);
                    if (aDescription)
                    {
                        myData.back().myDescription = aDescription;
                    }
                    EnterCriticalSection(&g_processMemCS);
#pragma message ("DrCos: Turn to a Watcher method 'ReadAndCacheValue'.")
                    myData.back().myCachedValue = myData.back().ReadValue();
                    LeaveCriticalSection(&g_processMemCS);

                    if (anAfterAddCallback)
                    {
                        anAfterAddCallback();
                    }

                    return true;
                }
            }

            auto WatcherCollection::UpdateCachedValuesAndHasChangedFlags() -> void
            {
                if (!myData.empty())
                {
                    EnterCriticalSection(&g_processMemCS);

                    for (auto& watcher : myData)
                    {
                        watcher.UpdateCachedValuesAndHasChangedFlags();
                    }

                    LeaveCriticalSection(&g_processMemCS);
                }
            }

            auto WatcherCollection::ReadFile(const char* aFileName, Callback_t anFileOpenFailedCallback) -> bool
            {
                auto file = fopen(aFileName, "rb");
                if (!file)
                {
                    if (anFileOpenFailedCallback)
                    {
                        anFileOpenFailedCallback();
                    }
                    return false;
                }
                char buff[1024];
                fgets(buff, ARRAYSIZE(buff), file);
                //char mode;
                //sscanf(buff, "%c%*s", &mode);
                fgets(buff, ARRAYSIZE(buff), file);
                unsigned int watchCount;
                sscanf(buff, "%d%*s", &watchCount);

                Score::Ram::Watch::Watcher Temp;
                for (; watchCount; --watchCount)
                {
                    do
                    {
                        fgets(buff, 1024, file);
                    } while (buff[0] == '\n');

                    Temp.Deserialize(buff);
                    if (!CheckIfAlreadyPresent(Temp))
                    {
                        myData.emplace_back(Temp);
                        EnterCriticalSection(&g_processMemCS);
#pragma message ("DrCos: Turn to a Watcher method 'ReadAndCacheValue'.")
                        myData.back().myCachedValue = myData.back().ReadValue();
                        LeaveCriticalSection(&g_processMemCS);
                    }
                }
                fclose(file);

                return true;
            }

            auto WatcherCollection::WriteFile(const char* aFileName) -> void
            {
                auto file = fopen(aFileName, "r+b");
                if (!file) file = fopen(aFileName, "w+b");

                fputc('0', file);
                fputc('\n', file);

                char buff[1024];
                sprintf(buff, "%d\n", Count());
                fputs(buff, file);

                for (auto& watcher : myData)
                {
                    fputs(watcher.Serialize().c_str(), file);
                }

                fclose(file);
            }

            auto WatcherCollection::Clear() -> void
            {
                myData.clear();
            }

            auto WatcherCollection::Remove(size_t anIndex) -> void
            {
                myData.erase(myData.begin() + anIndex);
            }

            auto WatcherCollection::Remove(const Watcher& aWatcher) -> void
            {
                auto it = std::find(myData.begin(), myData.end(), aWatcher);
                auto& k = *it;
                if (it != myData.end() && *it != aWatcher)
                {
                    myData.erase(it);
                }
            }


        } /* namespace Watch */
    } /* namespace Ram */
} /* namespace Score */