#include "Watcher.h"

#include <Windows.h>

#pragma message ("DrCos: EXE// HANDLE hGameProcess")
extern HANDLE hGameProcess;

namespace Score
{
    namespace Ram
    {
        namespace Watch
        {
            static const char DELIM = '\t';

            auto Watcher::ReadValue() const -> Value
            {
                Value value;
                ReadProcessMemory(hGameProcess, (void*)myAddress, (void*)&value, GetSizeInBytes(mySizeID), nullptr);
                if (myTypeID == TypeID::Float)
                {
                    if (mySizeID == SizeID::_64bits)
                    {
                        value.myType = Value::Type::Double;
                    }
                    else
                    {
                        value.myType = Value::Type::Float;
                    }
                }
                else
                {
                    if (mySizeID == SizeID::_64bits)
                    {
                        value.myType = Value::Type::Int64;
                    }
                    else
                    {
                        value.myType = Value::Type::Int32;
                    }
                }
                return value;
            }

            auto Watcher::UpdateCachedValuesAndHasChangedFlags() -> void
            {
                auto newCachedValue = ReadValue();
                if (myHasChanged = !myCachedValue.CheckBinaryEquality(newCachedValue))
                {
                    myCachedValue = newCachedValue;
                }
            }


            auto Watcher::Deserialize(const char* aString) -> void
            {
                sscanf(aString, "%*05X%*c%08X%*c%c%*c%c%*c%d", &myAddress, &mySizeID, &myTypeID, &myEndianness);
                myEndianness = Endianness::Little;
                const char* Comment = strrchr(aString, DELIM) + 1;
                if (Comment != (char*)nullptr + 1)
                {
                    const char* newline = strrchr(Comment, '\n');
                    if (newline)
                    {
                        myDescription = std::string(Comment, newline);
                    }
                    else
                    {
                        myDescription = newline;
                    }
                    return;
                }
            }

            auto Watcher::Serialize() const -> std::string
            {
                char buff[1024];
                sprintf(buff, "%05X%c%08X%c%c%c%c%c%d%c%s\n", 0x12345, DELIM, myAddress, DELIM, mySizeID, DELIM, myTypeID, DELIM, myEndianness, DELIM, myDescription);
                return buff;
            }

        }
    }
}
