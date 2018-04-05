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

#ifndef OPENRV_ORV_LOGGING_H
#define OPENRV_ORV_LOGGING_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if defined(__GNUC__)
#define ORV_LOGGING_FUNCINFO __PRETTY_FUNCTION__
#else /* __GNUC__ */
#define ORV_LOGGING_FUNCINFO 0
#endif /* __GNUC__ */

#ifndef NO_OPENRV_DEBUG_MACROS
#define ORV_DEBUG(ctx, ...) orv_log(ctx, ORV_LOGGING_SEVERITY_DEBUG, ORV_LOGGING_FUNCINFO, __FILE__, __LINE__, __VA_ARGS__)
#define ORV_INFO(ctx, ...) orv_log(ctx, ORV_LOGGING_SEVERITY_INFO, ORV_LOGGING_FUNCINFO, __FILE__, __LINE__, __VA_ARGS__)
#define ORV_WARNING(ctx, ...) orv_log(ctx, ORV_LOGGING_SEVERITY_WARNING, ORV_LOGGING_FUNCINFO, __FILE__, __LINE__, __VA_ARGS__)
#define ORV_ERROR(ctx, ...) orv_log(ctx, ORV_LOGGING_SEVERITY_ERROR, ORV_LOGGING_FUNCINFO, __FILE__, __LINE__, __VA_ARGS__)
#endif /* NO_OPENRV_DEBUG_MACROS */

struct orv_context_t;

typedef enum
{
    ORV_LOGGING_SEVERITY_DEBUG = 0,
    ORV_LOGGING_SEVERITY_INFO = 1,
    ORV_LOGGING_SEVERITY_WARNING = 2,
    ORV_LOGGING_SEVERITY_ERROR = 3
} orv_logging_severity_t;

/**
 * @param msg The message to print. Is guaranteed to end in NUL (\0) and should NOT provide a
 *        newline (\n) before the terminating NUL.
 * @param func A human readable string describing the function that this message occurred in.
 *        May be NULL if the function name could not be determined (platform specific).
 *
 * Callback type that can receives internal logging information from the library.
 **/
typedef void (*orv_log_callback_t)(int severity, const char* func, const char* file, int line, const char* msg);

/**
 * Convenience function that accepts a message with parameters, formats the message accordingly and
 * forwards it to the logging callback configured for @p ctx.
 **/
extern void orv_log(const struct orv_context_t* ctx, int severity, const char* func, const char* file, int line, const char* msg, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 6, 7)))
#endif
    ;

/**
 * Simple implementation for a @ref orv_log_callback callback, that simply prints the data to stdout
 * and stderr.
 **/
extern void orv_log_stdoutstderr(int severity, const char* func, const char* file, int line, const char* msg);


#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif

