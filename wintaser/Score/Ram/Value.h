#pragma once

#include "HardwareAddress.h"

namespace Score
{
    namespace Ram
    {
        enum class SizeID : unsigned char
        {
            _8bits,
            _16bits,
            _32bits,
            _64bits
        };

        enum class TypeID : unsigned char
        {
            SignedDecimal,
            UnsignedDecimal,
            Hexadecimal,
            Float
        };

        enum class Endianness : unsigned char
        {
            Little,
            Big
        };

        inline auto GetSizeInBytes(SizeID aSizeID) -> size_t
        {
            static const size_t theSizeID2SizeInBytes[] = { 1, 2, 4, 8 };
            return theSizeID2SizeInBytes[static_cast<int>(aSizeID)];
        }

        // Variant value: Do we really need variant value?
        struct Value
        {
            enum Type : unsigned char
            {
                Int32,
                Int64,
                Float,
                Double,
            };

            union Union
            {
                int         Int32;
                long long   Int64;
                float       Float;
                double      Double;
            };

            Value() : myType(Type::Int32)
            {
                myValue.Int64 = 0;
            }
            Value(int anInt32) : myType(Type::Int32)
            {
                myValue.Int32 = anInt32;
            }
            Value(unsigned long anUInt32) : myType(Type::Int32)
            {
                myValue.Int32 = anUInt32;
            }
            Value(long long anInt64) : myType(Type::Int64)
            {
                myValue.Int64 = anInt64;
            }
            Value(unsigned long long anUInt64) : myType(Type::Int64)
            {
                myValue.Int64 = anUInt64;
            }
            Value(float aFloat) : myType(Type::Float)
            {
                myValue.Float = aFloat;
            }
            Value(double aDouble) : myType(Type::Double)
            {
                myValue.Double = aDouble;
            }
            Value(const Value& anOther)
            {
                myValue = anOther.myValue;
                myType = anOther.myType;
            }

            //auto operator=(const Value&) -> Value& = delete;

            template<typename CAST>
            operator CAST() const
            {
                switch (myType)
                {
                case Type::Int32:
                    return CAST(myValue.Int32);
                case Type::Int64:
                    return CAST(myValue.Int64);
                case Type::Float:
                    return CAST(myValue.Float);
                case Type::Double:
                    return CAST(myValue.Double);
                default:
                    return CAST();
                }
            }

            template<typename TYPE>
            auto operator<(TYPE anOther) const -> bool
            {
                switch (myType)
                {
                default:
                case Type::Int32:
                    return myValue.Int32 < anOther;
                case Type::Int64:
                    return myValue.Int64 < anOther;
                case Type::Float:
                    return myValue.Float < anOther;
                case Type::Double:
                    return myValue.Double < anOther;
                }
            }

            auto operator-() -> Value
            {
                switch (myType)
                {
                default:
                case Type::Int32:
                    return Value(-myValue.Int32);
                case Type::Int64:
                    return Value(-myValue.Int64);
                case Type::Float:
                    return Value(-myValue.Float);
                case Type::Double:
                    return Value(-myValue.Double);
                }
            }

            //bool CheckBinaryEquality(const Value& r) const
            //{
            //    if (myValue.i32s.i1 != r.myValue.i32s.i1) return false;
            //    if (myType != t_ll && t != t_d && r.myType != t_ll && r.myType != t_d) return true;
            //    return (myValue.i32s.i2 == r.myValue.i32s.i2);
            //}
            auto CheckBinaryEquality(const Value& anOther) const -> bool
            {
                if (myType == Type::Int32 || myType == Type::Float)
                {
                    return (anOther.myType == Type::Int32 || anOther.myType == Type::Float) && myValue.Int32 == anOther.myValue.Int32;
                }
                else
                {
                    return (anOther.myType == Type::Int64 || anOther.myType == Type::Double) && myValue.Int64 == anOther.myValue.Int64;
                }
            }

            Type myType;
            Union myValue;

            auto sprint(char* outBuffer, SizeID sizeTypeID, TypeID typeID) const -> void;
            auto sscan(const char* inBuffer, SizeID sizeTypeID, TypeID typeID) -> bool;
        };

    } /* namespace Ram */
} /* namespace Score */
