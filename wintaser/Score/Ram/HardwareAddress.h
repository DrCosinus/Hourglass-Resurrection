#pragma once

namespace Score
{
    namespace Ram
    {
        // replacing HWAddressType
        //typedef unsigned long long HardwareAddress;

        // HardwareAddress is designed to be a small object, a copy must be cheap
        class HardwareAddress final
        {
        public:
            explicit HardwareAddress(unsigned long long anAddress = 0)
                : myAddress(anAddress)
            {
            }
            explicit HardwareAddress(void* anAddress)
                : HardwareAddress(reinterpret_cast<unsigned long long>(anAddress))
            {
            }
            explicit operator unsigned int() const
            {
                return static_cast<unsigned int>(myAddress);
            }
            explicit operator void*() const
            {
                return reinterpret_cast<void*>(myAddress);
            }
            auto operator==(const HardwareAddress& anOther) const -> bool
            {
                return myAddress == anOther.myAddress;
            }
            auto operator!=(const HardwareAddress& anOther) const -> bool
            {
                return myAddress != anOther.myAddress;
            }
            auto operator>(const HardwareAddress& anOther) const -> bool
            {
                return myAddress > anOther.myAddress;
            }
            auto operator<(const HardwareAddress& anOther) const -> bool
            {
                return myAddress < anOther.myAddress;
            }
            auto operator<=(const HardwareAddress& anOther) const -> bool
            {
                return myAddress <= anOther.myAddress;
            }
            auto operator>=(const HardwareAddress& anOther) const -> bool
            {
                return myAddress >= anOther.myAddress;
            }
            auto operator+(int anOffset) const -> HardwareAddress
            {
                return HardwareAddress(myAddress + anOffset);
            }
            auto operator-(int anOffset) const -> HardwareAddress
            {
                return HardwareAddress(myAddress - anOffset);
            }
            auto operator--() -> HardwareAddress& // pre decrement
            {
                myAddress--;
                return *this;
            }
            auto operator--(int) -> HardwareAddress // post decrement
            {
                auto copy = HardwareAddress(*this);
                myAddress--;
                return copy;
            }
            auto operator+=(int anOffset) -> HardwareAddress&
            {
                myAddress += anOffset;
                return *this;
            }
            //auto operator-(unsigned int anOffset) const -> HardwareAddress
            //{
            //    return HardwareAddress(myAddress - anOffset);
            //}
            auto operator-(const HardwareAddress& anOther) const -> size_t
            {
                return size_t(myAddress - anOther.myAddress);
            }
            auto SkipSize(size_t aStepSize) const -> size_t
            {
                return (0U - myAddress) % aStepSize;
            }

            auto    IsValid() const -> bool;
            static  auto    IsValid(void* anAddress) -> bool;

        private:
            typedef unsigned long long  Storage_t;

            Storage_t   myAddress;
        };

        /**/

        //inline auto operator-(size_t lhs, HardwareAddress rhs) -> 
        //{

        //}

        /**/


    } /* namespace Ram */
} /* namespace Score */