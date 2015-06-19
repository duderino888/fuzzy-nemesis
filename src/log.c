#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>


#define TIMESTAMP_FMT   "[%Y-%m-%d %H:%M:%S]"


static const char *levels[] = {
    [LOG_INFO] = "(INFO)",
    [LOG_WARN] = "(WARN)",
    [LOG_ERROR] = "(ERROR)",
    [LOG_DEBUG] = "(DEBUG)",
};


void
log_msg(int level, const char *fmt, ...)
{
    va_list args;
    time_t now;
    char msg[256];
    char ts[64];


    now = time(NULL);
    strftime(ts, sizeof(ts), TIMESTAMP_FMT, localtime(&now));

    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    fprintf(stderr, "%s %s %s\n", ts, levels[level], msg);
}

