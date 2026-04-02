#ifndef SL_LOG_H
#define SL_LOG_H

#include "Sl_Defines.hpp"
#include <stdarg.h>

namespace Sl
{
    enum LogLevel {
        LOG_EMPTY = 0, // EMPTY = doesn't print it's level (but others do)
        LOG_TRACE,
        LOG_INFO,
        LOG_WARNING,
        LOG_ERROR,
        LOG_DISABLE
    };

    // call this functions
    void log(const char* const format, ...) SL_PRINTF_FORMATER(1, 2); // LOG_EMPTY
    void log_level(LogLevel level, const char* const format, ...) SL_PRINTF_FORMATER(2, 3);
    void log_trace(const char* const format, ...) SL_PRINTF_FORMATER(1, 2);
    void log_info(const char* const format, ...) SL_PRINTF_FORMATER(1, 2);
    void log_warning(const char* const format, ...) SL_PRINTF_FORMATER(1, 2);
    void log_error(const char* const format, ...) SL_PRINTF_FORMATER(1, 2);

    typedef void (*Logger_handler)(LogLevel level, const char* const format, va_list args);

    void log_set_current(Logger_handler log);
    Logger_handler log_get_current();

    // Here's already implemented log function.
    // You can still create a custom one, and then set it using: log_set_current(your_log_function)
    void logger_default(LogLevel level, const char* const format, va_list args);
    void logger_colored(LogLevel level, const char* const format, va_list args);
    void logger_muted(LogLevel level, const char* const format, va_list args);

    // Useful for changing log for current scope, for example: if you want to mute logger.
    struct ScopedLogger
    {
        ScopedLogger(Logger_handler new_logger) {
            saved_logger = log_get_current();
            log_set_current(new_logger);
        }
        ~ScopedLogger() {
            log_set_current(saved_logger);
        }
        Logger_handler saved_logger;
    };
} // namespace Sl
#endif // SL_LOG_H

#ifdef SL_IMPLEMENTATION
namespace Sl
{
    static SL_THREAD_LOCAL Logger_handler logger_handler = logger_default;

    void log_level(LogLevel level, const char* const format, ...)
    {
        va_list args;
        va_start(args, format);
        logger_handler(level, format, args);
        va_end(args);
    }

    void log_set_current(Logger_handler log)
    {
        logger_handler = log;
    }

    Logger_handler log_get_current()
    {
        return logger_handler;
    }

    void logger_default(LogLevel level, const char* const format, va_list args)
    {
        switch (level)
        {
            case LOG_TRACE:
                fprintf(stdout, "[TRACE] ");
                break;
            case LOG_INFO:
                fprintf(stdout, "[INFO] ");
                break;
            case LOG_WARNING:
                fprintf(stdout, "[WARNING] ");
                break;
            case LOG_ERROR:
                fprintf(stdout, "[ERROR] ");
                break;
            case LOG_EMPTY: break;
            case LOG_DISABLE: return;
            default: UNREACHABLE("logger_default");
        }
        vfprintf(stdout, format, args);
    }

    void logger_colored(LogLevel level, const char* const format, va_list args)
    {
        switch (level)
        {
            case LOG_TRACE:
                fprintf(stdout, "\x1b[36m[TRACE] ");
                break;
            case LOG_INFO:
                fprintf(stdout, "\x1b[94m[INFO] ");
                break;
            case LOG_WARNING:
                fprintf(stdout, "\x1b[33m[WARNING] ");
                break;
            case LOG_ERROR:
                fprintf(stdout, "\x1b[31m[ERROR] ");
                break;
            case LOG_EMPTY: break;
            case LOG_DISABLE: return;
            default: UNREACHABLE("logger_colored");
        }

        vfprintf(stdout, format, args);
        printf("\x1b[0m");
    }

    void logger_muted(LogLevel level, const char* const format, va_list args)
    {
        UNUSED(level);
        UNUSED(format);
        UNUSED(args);
    }

    void log(const char* const format, ...)
    {
        va_list args;
        va_start(args, format);
        logger_handler(LOG_EMPTY, format, args);
        va_end(args);
    }
    void log_trace(const char* const format, ...)
    {
        va_list args;
        va_start(args, format);
        logger_handler(LOG_TRACE, format, args);
        va_end(args);
    }
    void log_info(const char* const format, ...)
    {
        va_list args;
        va_start(args, format);
        logger_handler(LOG_INFO, format, args);
        va_end(args);
    }
    void log_warning(const char* const format, ...)
    {
        va_list args;
        va_start(args, format);
        logger_handler(LOG_WARNING, format, args);
        va_end(args);
    }
    void log_error(const char* const format, ...)
    {
        va_list args;
        va_start(args, format);
        logger_handler(LOG_ERROR, format, args);
        va_end(args);
    }
} // namespace Sl
#endif // SL_IMPLEMENTATION
