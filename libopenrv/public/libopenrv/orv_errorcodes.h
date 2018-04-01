#ifndef OPENRV_ORV_ERRORCODES_H
#define OPENRV_ORV_ERRORCODES_H

typedef enum orv_error_code_t
{
    ORV_ERR_GENERIC = -1,
    ORV_ERR_NO_ERROR = 0,
    ORV_ERR_UNUSED = 1, /* "1" is traditionally used like -1, as a "generic" error condition. To avoid confusion due to incorrect error codes, we avoid this value. */
    ORV_ERR_USER_INTERRUPTION = 2, /* orv_disconnect() was called */

    /* 10..99: ConnectError */
    ORV_ERR_CONNECT_ERROR_GENERIC = 10,
    ORV_ERR_CONNECT_ERROR_NO_SUCH_HOST = 11,
    ORV_ERR_CONNECT_ERROR_CONNECTION_REFUSED = 12,
    ORV_ERR_CONNECT_ERROR_HOST_UNREACHABLE = 13,
    ORV_ERR_CONNECT_ERROR_TIMEOUT = 14,
    ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR = 15,
    ORV_ERR_CONNECT_ERROR_AUTHENTICATION_FAILED = 16,
    ORV_ERR_CONNECT_ERROR_SERVER_REJECTS_CONNECTION = 17,
    ORV_ERR_CONNECT_ERROR_SSL_HANDSHAKE = 18, /* Sub-errorcode may contain SSL-library specific error code. For OpenSSL, this is SSL_get_error() */

    /* 100..9999: Errors during established connections */
    /**
     * Failed to read/receive data from the socket. This typically means the connection was lost.
     **/
    ORV_ERR_READ_FAILED = 100,
    /**
     * Failed to write/send data to the socket. This typically means the connection was lost.
     **/
    ORV_ERR_WRITE_FAILED = 101,
    /**
     * Protocol error. Unexpected data was received from the server.
     **/
    ORV_ERR_PROTOCOL_ERROR = 102,
    ORV_ERR_CLOSED_BY_REMOTE = 103,
    ORV_ERR_UNSUPPORTED_ENCODING = 120,
    ORV_ERR_INVALID_ENCODING = 121,
} orv_error_code_t;

#endif
