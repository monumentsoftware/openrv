#include <libopenrv/orv_error.h>
#include <libopenrv/orv_logging.h>
#include "orv_context.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * Reset @p e, setting @ref orv_error_t::mHasError to 0 and clearing any error message from memory.
 **/
void orv_error_reset(orv_error_t* e)
{
    e->mHasError = 0;
    e->mErrorCode = 0;
    e->mSubErrorCode = 0;
    memset(e->mErrorMessage, 0, ORV_MAX_ERROR_MESSAGE_LEN + 1);
}

/**
 * Similar to @ref orv_error_reset(), but the error message is not explicitly cleared from memory
 * (only the first byte is set to the NUL terminator).
 *
 * This function can be used in somewhat performance critical situations.
 *
 * Note that if you use this, you may want to consider simply setting @ref orv_error_t::mHasError,
 * and possibly (if required) @ref orv_error_t::mErrorCode, @ref orv_error_t::mSubErrorCode and @ref
 * orv_error_t::mErrorMessage manually in @p e instead of calling this function.
 **/
void orv_error_reset_minimal(orv_error_t* e)
{
    e->mHasError = 0;
    e->mErrorCode = 0;
    e->mSubErrorCode = 0;
    e->mErrorMessage[0] = '\0';
}

/**
 * Copy the error @p src to @p dst.
 **/
void orv_error_copy(orv_error_t* dst, const orv_error_t* src)
{
    memcpy(dst, src, sizeof(orv_error_t));
}

/**
 * Print the @p error using the log callback in @p ctx.
 **/
void orv_error_print_to_log(const struct orv_context_t* ctx, const orv_error_t* error)
{
    if (error->mErrorCode == ORV_ERR_NO_ERROR) {
        ORV_DEBUG(ctx, "No error");
    }
    else {
        ORV_ERROR(ctx, "Error code: %d, sub-error-code: %d, message: %s\n", error->mErrorCode, error->mSubErrorCode, error->mErrorMessage);
    }
}

/**
 * Set the error code/message to the specified values. This function is normally used internally by
 * the library only, but may be used by the user as well.
 *
 * The @p subErrorCode is meant as a domain-specific error code that is specific to the environment
 * where this Error object is used. Note that the @p code should be set to a value that identifies
 * that environment, otherwise the @p subErrorCode is of little use to the user.
 *
 * This function performs a deep copy on the specified @p message.
 **/
void orv_error_set(orv_error_t* error, int code, int subErrorCode, const char* message, ...)
{
    va_list list;
    va_start(list, message);
    orv_error_vset(error, code, subErrorCode, message, list);
    va_end(list);
}

/**
 * Similar to @ref orv_error_set, but takes a va_list argument @p list containing the arguments.
 **/
void orv_error_vset(orv_error_t* error, int code, int subErrorCode, const char* message, va_list list)
{
    if (code == ORV_ERR_NO_ERROR) {
        orv_error_reset(error);
        return;
    }
    error->mHasError = 1;
    error->mErrorCode = code;
    error->mSubErrorCode = subErrorCode;
    size_t s = vsnprintf(error->mErrorMessage, ORV_MAX_ERROR_MESSAGE_LEN, message, list);
    error->mErrorMessage[s] = 0;
}

