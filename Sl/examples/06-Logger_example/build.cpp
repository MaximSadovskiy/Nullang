#define SL_IMPLEMENTATION
#include "../../Sl.hpp"

using namespace Sl;

void custom(LogLevel level, const char* const format, va_list args)
{
    if (level == LOG_ERROR) {
        // Write something to file..
        return;
    }

    printf("---------------\n");
    printf("{CustomLog} ");
      vprintf(format, args);
    printf("---------------\n");
}

int main(int argc, char **argv)
{
    {
        // You can set your custom logger:
        log_set_current(custom);

        log("test\n");

        log_set_current(logger_default);
    }

    {
        // Or you can enable colored logging in this scope
        ScopedLogger _(logger_colored);

        int count = 0;
        log("Colored logger example:\n");
        log("  "); log_trace("Trace message! %d\n", ++count);
        log("  "); log_info("Info message! %d\n", ++count);
        log("  "); log_warning("Warning message! %d\n", ++count);
        log("  "); log_error("Error message! %d\n", ++count);
        log("  "); log_level(LOG_TRACE, "Custom level warning %d\n", ++count);
    }

    {
        // Disable logging in this scope
        ScopedLogger _(logger_muted);
        log_error("You cannot see this message\n");
    }

    // Logger handler returned to logger_default()
}
