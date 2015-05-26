#include "Value.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <string>

namespace Score
{
    namespace Ram
    {
        //const size_t theSizeID2SizeInBytes[] =
        //{
        //    1,
        //    2,
        //    4,
        //    8
        //};

        auto Value::sprint(char* output, SizeID sizeTypeID, TypeID typeID) const -> void
        {
            switch (typeID)
            {
            case TypeID::Float:
            {
                int len = sprintf(output, "%g", myValue.Double); // don't use %f, too long
                // now, I want whole numbers to still show a .0 at the end so they look like floats.
                // I'd check LOCALE_SDECIMAL, but sprintf doesn't seem to use the current locale's decimal separator setting anyway.
                bool floaty = false;
                for (int i = 0; i < len; i++)
                {
                    if (output[i] == '.' || output[i] == 'e' || output[i] == ',')
                    {
                        floaty = true;
                        break;
                    }
                }
                if (!floaty)
                {
                    strcpy(output + len, ".0");
                }
                break;
            }

            case TypeID::SignedDecimal:
                switch (sizeTypeID)
                {
                default:
                case SizeID::_8bits:
                {
                    auto c = static_cast<char>(myValue.Int32 & 0xff);
                    output += sprintf(output, "%d", c);
                    if ((unsigned int)(c - 32) < (127 - 32))
                    {
                        sprintf(output, " ('%c')", c);
                    }
                    break;
                }
                case SizeID::_16bits:
                    sprintf(output, "%d", static_cast<short>(myValue.Int32 & 0xffff));
                    break;
                case SizeID::_32bits:
                    sprintf(output, "%d", myValue.Int32);
                    break;
                case SizeID::_64bits:
                    sprintf(output, "%I64d", myValue.Int64);
                    break;
                }
                break;
            case TypeID::UnsignedDecimal:
                switch (sizeTypeID)
                {
                default:
                case SizeID::_8bits:
                {
                    auto c = static_cast<unsigned char>(myValue.Int32 & 0xFF);
                    output += sprintf(output, "%u", c);
                    if ((unsigned int)(c - 32) < (127 - 32))
                    {
                        sprintf(output, " ('%c')", c);
                    }
                    break;
                }
                case SizeID::_16bits:
                    sprintf(output, "%u", static_cast<unsigned short>(myValue.Int32 & 0xffff));
                    break;
                case SizeID::_32bits:
                    sprintf(output, "%u", static_cast<unsigned long>(myValue.Int32));
                    break;
                case SizeID::_64bits:
                    sprintf(output, "%I64u", static_cast<unsigned long long>(myValue.Int64));
                    break;
                }
                break;
            default:
            case TypeID::Hexadecimal:
                switch (sizeTypeID)
                {
                default:
                case SizeID::_8bits:
                    sprintf(output, "%02x", (myValue.Int32 & 0xff));
                    break;
                case SizeID::_16bits:
                    sprintf(output, "%04x", (myValue.Int32 & 0xffff));
                    break;
                case SizeID::_32bits:
                    sprintf(output, "%08x", myValue.Int32);
                    break;
                case SizeID::_64bits:
                    sprintf(output, "%016I64x", myValue.Int64);
                    break;
                }
                break;
            }
        }

        auto Value::sscan(const char* inBuffer, SizeID sizeTypeID, TypeID typeID) -> bool
        {
            int inputLen = strlen(inBuffer) + 1;
            inputLen = std::min(inputLen, 32);
            char* temp = (char*)_alloca(inputLen);
            strncpy(temp, inBuffer, inputLen);
            temp[inputLen - 1] = '\0';
            for (int i = 0; temp[i]; i++)
            {
                if (toupper(temp[i]) == 'O')
                {
                    temp[i] = '0';
                }
            }

            bool forceHex = (typeID == TypeID::Hexadecimal);
            bool readFloat = (typeID == TypeID::Float);
            bool readLongLong = (sizeTypeID == SizeID::_64bits);
            bool negate = false;

            char* strPtr = temp;
            while (strPtr[0] == '-')
            {
                strPtr++;
                negate = !negate;
            }
            if (strPtr[0] == '+')
            {
                strPtr++;
            }
            if (strPtr[0] == '0' && tolower(strPtr[1]) == 'x')
            {
                strPtr += 2;
                forceHex = true;
            }
            if (strPtr[0] == '$')
            {
                strPtr++;
                forceHex = true;
            }
            if (strPtr[0] == '\'' && strPtr[1] && strPtr[2] == '\'')
            {
                if (readFloat)
                {
                    forceHex = true;
                }
                sprintf(strPtr, forceHex ? "%X" : "%u", (int)strPtr[1]);
            }
            if (!forceHex && !readFloat)
            {
                const char* strSearchPtr = strPtr;
                while (*strSearchPtr)
                {
                    int c = tolower(*strSearchPtr++);
                    if (c >= 'a' && c <= 'f')
                    {
                        forceHex = true;
                        break;
                    }
                    if (c == '.')
                    {
                        readFloat = true;
                        break;
                    }
                }
            }
            bool ok = false;
            if (readFloat)
            {
                if (!readLongLong)
                {
                    float f = 0.0f;
                    if (sscanf(strPtr, forceHex ? "%x" : "%f", &f) > 0)
                    {
                        ok = true;
                    }
                    if (negate)
                    {
                        f = -f;
                    }
                    myValue.Float = f;
                }
                else
                {
                    double d = 0.0;
                    if (sscanf(strPtr, forceHex ? "%I64x" : "%lf", &d) > 0)
                    {
                        ok = true;
                    }
                    if (negate)
                    {
                        d = -d;
                    }
                    myValue.Double = d;
                }
            }
            else
            {
                if (!readLongLong)
                {
                    int i32 = 0;
                    const char* formatString = forceHex ? "%x" : ((typeID == TypeID::SignedDecimal) ? "%d" : "%u");
                    if (sscanf(strPtr, formatString, &i32) > 0)
                    {
                        ok = true;
                    }
                    if (negate)
                    {
                        i32 = -i32;
                    }
                    myValue.Int32 = i32;
                }
                else
                {
                    long long i64 = 0;
                    const char* formatString = forceHex ? "%I64x" : ((typeID == TypeID::SignedDecimal) ? "%I64d" : "%I64u");
                    if (sscanf(strPtr, formatString, &i64) > 0)
                    {
                        ok = true;
                    }
                    if (negate)
                    {
                        i64 = -i64;
                    }
                    myValue.Int64 = i64;
                }
            }
            return ok;
        }
    } /* namespace Ram */
} /* namespace Score */
