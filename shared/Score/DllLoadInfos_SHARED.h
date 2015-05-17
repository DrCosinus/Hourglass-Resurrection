#pragma once

#include <Windows.h> // for HANDLE

#include <functional>
#include <assert.h>

namespace Score
{
    namespace SharedMemory
    {
        template<typename T>
        class Object
        {
        protected:
            // For now, I assume that CreateFileMapping and MapViewOfFile will not fail.
            Object(const char* aFileMappingName)
                : myHandle(CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(T), aFileMappingName))
                , myDataPtr(reinterpret_cast<T*>(MapViewOfFile(myHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(T))))
                , myData(*myDataPtr)
            {
            }
            ~Object() // /!\ for now the destructor does not need to be virtual but we must forget to make it virtual if we add a virtual method
            {
                UnmapViewOfFile(myDataPtr);
                myDataPtr = nullptr;

                CloseHandle(myHandle);
                myHandle = INVALID_HANDLE_VALUE;
            }

            // WARNING: the order of the variable is very important
            HANDLE myHandle = INVALID_HANDLE_VALUE;

            T* myDataPtr;
            T& myData;
        };
    }

    struct DllInfo
    {
        //bool        myIsLoaded; // true=load, false=unload
        char        myDllName[64];
    };

    struct DllInfos
    {
        size_t      myInfoCount = 0;
        DllInfo     myDllInfos[128];
    };

    class LoadedDllList : SharedMemory::Object<DllInfos>
    {
        typedef SharedMemory::Object<DllInfos> Super;
        static const char* ourFileMappingName;
    protected:
        LoadedDllList() : Super(ourFileMappingName)
        {
        }

        auto GetInfoCount() -> int
        {
            return myData.myInfoCount;
        }

        auto PushFirst(const char* aDllName) -> void
        {
            assert(myData.myInfoCount < ARRAYSIZE(myData.myDllInfos));
            memmove(&myData.myDllInfos[1], &myData.myDllInfos[0], sizeof(*myData.myDllInfos)*myData.myInfoCount);

            strncpy(myData.myDllInfos[0].myDllName, aDllName, ARRAYSIZE(myData.myDllInfos[0].myDllName));
            ++myData.myInfoCount;
        }

        auto PopLast() -> DllInfo&
        {
            assert(myData.myInfoCount);
            return myData.myDllInfos[--myData.myInfoCount];
        }

        auto Last() -> DllInfo&
        {
            assert(myData.myInfoCount);
            return myData.myDllInfos[myData.myInfoCount - 1];
        }
    };
}

