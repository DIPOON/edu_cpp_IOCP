#pragma once
#include <plog/Logger.h>
#include <plog/Formatters/CsvFormatter.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/RollingFileAppender.h>
#include <cstring>

namespace plog
{
    namespace
    {
        bool isCsv(const util::nchar* fileName)
        {
            const util::nchar* dot = util::findExtensionDot(fileName);
#ifdef _WIN32
            return dot && 0 == std::wcscmp(dot, L".csv");
#else
            return dot && 0 == std::strcmp(dot, ".csv");
#endif
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Empty initializer / one appender

    template<int instanceId>
    inline Logger<instanceId>& Init(Severity maxSeverity = none, IAppender* appender = NULL)
    {
        static Logger<instanceId> logger(maxSeverity);
        return appender ? logger.addAppender(appender) : logger;
    }

    inline Logger<PLOG_DEFAULT_INSTANCE_ID>& Init(Severity maxSeverity = none, IAppender* appender = NULL)
    {
        return Init<PLOG_DEFAULT_INSTANCE_ID>(maxSeverity, appender);
    }

    //////////////////////////////////////////////////////////////////////////
    // RollingFileAppender with any Formatter

    template<class Formatter, int instanceId>
    inline Logger<instanceId>& Init(Severity maxSeverity, const util::nchar* fileName, size_t maxFileSize = 0, int maxFiles = 0)
    {
        static RollingFileAppender<Formatter> rollingFileAppender(fileName, maxFileSize, maxFiles);
        return Init<instanceId>(maxSeverity, &rollingFileAppender);
    }

    template<class Formatter>
    inline Logger<PLOG_DEFAULT_INSTANCE_ID>& Init(Severity maxSeverity, const util::nchar* fileName, size_t maxFileSize = 0, int maxFiles = 0)
    {
        return Init<Formatter, PLOG_DEFAULT_INSTANCE_ID>(maxSeverity, fileName, maxFileSize, maxFiles);
    }

    //////////////////////////////////////////////////////////////////////////
    // RollingFileAppender with TXT/CSV chosen by file extension

    template<int instanceId>
    inline Logger<instanceId>& Init(Severity maxSeverity, const util::nchar* fileName, size_t maxFileSize = 0, int maxFiles = 0)
    {
        return isCsv(fileName) ? Init<CsvFormatter, instanceId>(maxSeverity, fileName, maxFileSize, maxFiles) : Init<TxtFormatter, instanceId>(maxSeverity, fileName, maxFileSize, maxFiles);
    }

    inline Logger<PLOG_DEFAULT_INSTANCE_ID>& Init(Severity maxSeverity, const util::nchar* fileName, size_t maxFileSize = 0, int maxFiles = 0)
    {
        return Init<PLOG_DEFAULT_INSTANCE_ID>(maxSeverity, fileName, maxFileSize, maxFiles);
    }

    //////////////////////////////////////////////////////////////////////////
    // CHAR variants for Windows

#ifdef _WIN32
    template<class Formatter, int instanceId>
    inline Logger<instanceId>& Init(Severity maxSeverity, const char* fileName, size_t maxFileSize = 0, int maxFiles = 0)
    {
        return Init<Formatter, instanceId>(maxSeverity, util::toWide(fileName).c_str(), maxFileSize, maxFiles);
    }

    template<class Formatter>
    inline Logger<PLOG_DEFAULT_INSTANCE_ID>& Init(Severity maxSeverity, const char* fileName, size_t maxFileSize = 0, int maxFiles = 0)
    {
        return Init<Formatter, PLOG_DEFAULT_INSTANCE_ID>(maxSeverity, fileName, maxFileSize, maxFiles);
    }

    template<int instanceId>
    inline Logger<instanceId>& Init(Severity maxSeverity, const char* fileName, size_t maxFileSize = 0, int maxFiles = 0)
    {
        return Init<instanceId>(maxSeverity, util::toWide(fileName).c_str(), maxFileSize, maxFiles);
    }

    inline Logger<PLOG_DEFAULT_INSTANCE_ID>& Init(Severity maxSeverity, const char* fileName, size_t maxFileSize = 0, int maxFiles = 0)
    {
        return Init<PLOG_DEFAULT_INSTANCE_ID>(maxSeverity, fileName, maxFileSize, maxFiles);
    }
#endif
}
