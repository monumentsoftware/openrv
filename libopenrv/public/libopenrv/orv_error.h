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

#ifndef OPENRV_ORV_ERROR_H
#define OPENRV_ORV_ERROR_H

#include "orv_errorcodes.h"
#include <stdlib.h>
#include <stdarg.h>

#define ORV_MAX_ERROR_MESSAGE_LEN 1024

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct orv_context_t;

/**
 * Simple struct holding an error state, error code and error message.
 *
 * The error message is meant to be displayed to the user (no i18n support at this point though).
 **/
typedef struct orv_error_t
{
    /**
     * 1 if this object holds an error (@ref mErrorCode is not ORV_ERR_NO_ERROR), otherwise 0
     * (@ref mErrorCode is ORV_ERR_NO_ERROR).
     **/
    int mHasError;
    /**
     * Value from orv_error_code_t.
     **/
    int mErrorCode;
    /**
     * Sub-errror-code depending on the domain. 0 if not used.
     **/
    int mSubErrorCode;
    /**
     * Human-readable string containing details on the error.
     **/
    char mErrorMessage[ORV_MAX_ERROR_MESSAGE_LEN + 1];
} orv_error_t;

extern void orv_error_reset(orv_error_t* e);
extern void orv_error_reset_minimal(orv_error_t* e);
extern void orv_error_copy(orv_error_t* dst, const orv_error_t* src);
extern void orv_error_set(orv_error_t* dst, int errorCode, int subErrorCode, const char* msg, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 4, 5)))
#endif
    ;
extern void orv_error_vset(orv_error_t* error, int code, int subErrorCode, const char* message, va_list list)
#ifdef __GNUC__
    __attribute__((format(printf, 4, 0)))
#endif
    ;
extern void orv_error_print_to_log(const struct orv_context_t* ctx, const orv_error_t* error);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif
