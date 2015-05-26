#pragma once

#include <string>
#include <vector>

#include <Score/Ram/Value.h>

namespace Score
{
    namespace Ram
    {
        // Variant value: Do we really need variant value?

        namespace Watch
        {
            class Watcher
            {
            public:
                                Watcher()
                                    : myTypeID(TypeID::UnsignedDecimal)
                                    , mySizeID(SizeID::_8bits)
                                    , myEndianness(Endianness::Little)
                                    , myHasChanged(false)
                                {
                                }
                inline  auto    operator== (const Watcher& anOther) const -> bool;
                inline  auto    operator!= (const Watcher& anOther) const -> bool;

                        auto    ReadValue  () const -> Value;

                        auto    UpdateCachedValuesAndHasChangedFlags() -> void;

                        auto    Deserialize(const char* aString) -> void;
                        auto    Serialize() const -> std::string;

                inline  auto    sprint(char * outBuffer) const -> void;

                std::string     myDescription;
                Value           myCachedValue;
                HardwareAddress myAddress;
                TypeID          myTypeID;
                SizeID          mySizeID;
                Endianness      myEndianness;
                bool            myHasChanged;
            };

            inline auto Watcher::operator == (const Watcher& anOther) const -> bool
            {
                return mySizeID == anOther.mySizeID
                    && myTypeID == anOther.myTypeID
                    //&& myEndianness == anOther.myEndianness
                    && myAddress == anOther.myAddress;
            }
            
            inline auto Watcher::operator != (const Watcher& anOther) const -> bool
            {
                return mySizeID != anOther.mySizeID
                    || myTypeID != anOther.myTypeID
                    //|| myEndianness != anOther.myEndianness
                    || myAddress != anOther.myAddress;
            }

            inline auto Watcher::sprint(char * outBuffer) const -> void
            {
                myCachedValue.sprint(outBuffer, mySizeID, myTypeID);
            }
        }
    }
}