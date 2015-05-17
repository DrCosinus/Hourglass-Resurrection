#pragma once

#include <stdio.h>
#include <functional>
#include <vector>
#include <stdarg.h>

namespace Score
{
    class Logger final
    {
    public:
        enum class Category
        {
            Debug,
            InterProcess,
        };

        enum class Severity
        {
            Message,
            Warning,
            Error,
            FatalError,
        };

        typedef std::function<void(Category, Severity, const char*, size_t, const char*)> Callback_t;

        inline static auto Log(Category aCategory, Severity aSeverity, const char* aFilename, size_t aLineNumber, const char* aFormat, ...) -> void
        {
            if (!myRegistrees.empty())
            {
                va_list args;
                va_start(args, aFormat);
                char buff[2048];
                vsprintf(buff, aFormat, args);
                va_end(args);
                auto p = *myRegistrees.begin();
                for (auto& registree : myRegistrees)
                {
                    registree(aCategory, aSeverity, aFilename, aLineNumber, buff);
                }
            }
        }

        inline static auto Register(Callback_t&& aCallback) -> void
        {
            myRegistrees.push_back(aCallback);
        }

        template<typename F>
        inline static auto Register(F aCallback) -> void
        {
            myRegistrees.push_back(Callback_t(aCallback));
        }

        inline static auto Register(void(*aFunction)(Category, Severity, const char*, size_t, const char*)) -> void
        {
            Register(Callback_t(aFunction));
        }

        template<typename T>
        inline static auto Register(T* anObject, void(T::*aMemberFunction)(Category, Severity, const char*, size_t, const char*)) -> void
        {
            Register(Callback_t(std::bind(aMemberFunction, anObject, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5)));
        }
    private:
        static std::vector<Callback_t> myRegistrees;
    };

}
#define MODERN_LOG(FORMAT, ...) ::Score::Logger::Log(::Score::Logger::Category::Debug, ::Score::Logger::Severity::Message, __FILE__, __LINE__, FORMAT, __VA_ARGS__)


