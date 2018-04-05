/*
 * Copyright (C) 2018 Monument-Software GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <libopenrv/orv_logging.h>
#include "orv_context.h"

#include <stdio.h>
#include <stdarg.h>
#define TIME_STRING_LEN 100
#ifndef WIN32
#include <sys/time.h>
#include <time.h>
#else // WIN32
struct timeval
{
    int tv_usec = 0;
};
#endif // WIN32

extern "C" {

void orv_log(const orv_context_t* ctx, int severity, const char* func, const char* file, int line, const char* msg, ...)
{
    if (!ctx || !ctx->mConfig.mLogCallback) {
        return;
    }
    static const size_t bufferSize = 1024;
    char buffer[bufferSize + 1];
    va_list list;
    va_start(list, msg);
    size_t s = vsnprintf(buffer, bufferSize, msg, list);
    buffer[s] = '\0';
    ctx->mConfig.mLogCallback(severity, func, file, line, buffer);
    va_end(list);
}

/* TODO: make thread-safe */
void orv_log_stdoutstderr(int severity, const char* func, const char* file, int line, const char* msg)
{
    FILE* f = stdout;
    if (severity >= ORV_LOGGING_SEVERITY_WARNING) {
        f = stderr;
    }
#ifndef WIN32
    struct timeval t;
    char timeString[TIME_STRING_LEN + 1] = {};
    gettimeofday(&t, nullptr);
    strftime(timeString, TIME_STRING_LEN, "%Y-%m-%d %H:%M:%S", localtime((const time_t*)&t.tv_sec));
#else
    const char* timeString = "";
#endif // WIN32
    switch (severity) {
        case ORV_LOGGING_SEVERITY_DEBUG:
            fprintf(f, "DEBUG[%s:%03d]: ", timeString, (int)(t.tv_usec/1000));
            break;
        case ORV_LOGGING_SEVERITY_INFO:
            fprintf(f, "INFO[%s:%03d]: ", timeString, (int)(t.tv_usec/1000));
            break;
        case ORV_LOGGING_SEVERITY_WARNING:
            fprintf(f, "WARNING[%s:%03d]: ", timeString, (int)(t.tv_usec/1000));
            break;
        default:
        case ORV_LOGGING_SEVERITY_ERROR:
            fprintf(f, "ERRROR[%s:%03d]: ", timeString, (int)(t.tv_usec/1000));
            break;
    }
    if (func) {
        fprintf(f, "%s (%s in %s:%d)\n", msg, func, file, line);
    }
    else {
        fprintf(f, "%s (%s:%d)\n", msg, file, line);
    }
    fflush(f);
}

} // extern "C"


