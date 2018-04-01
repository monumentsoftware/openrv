#ifndef OPENRV_ORV_LATENCYTESTERCLIENT_H
#define OPENRV_ORV_LATENCYTESTERCLIENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct orv_latency_tester_client_t;

typedef struct orv_latency_tester_update_response_t
{
    int32_t mPreviousTopLeftX;
    int32_t mPreviousTopLeftY;
    int32_t mPreviousBottomRightX;
    int32_t mPreviousBottomRightY;
    int32_t mNewTopLeftX;
    int32_t mNewTopLeftY;
    int32_t mNewBottomRightX;
    int32_t mNewBottomRightY;
    /*
     * The timestamp when the client sent the update request to the server.
     */
    int64_t mClientSendTimestampSec;
    int64_t mClientSendTimestampUSec;
    /*
     * The timestamp when the client received the update response from the server.
     * At this time, the server has already performed the paint event.
     *
     * Combined with the send timestamp, this is the *full* ping, including the actual painting at
     * the server. Also, this is the earliest moment any server running at this client could
     * receive the update (assuming both, this client and the server use tcp. with udp the
     * latency could be reduced slightly more).
     */
    int64_t mClientReceiveTimestampSec;
    int64_t mClientReceiveTimestampUSec;
    /*
     * The timestamp when the server received the update request from the client
     **/
    int64_t mServerRequestReceiveTimestampSec;
    int64_t mServerRequestReceiveTimestampUSec;
    /*
     * The timestamp when the server *performed* the update that was requested by the client
     */
    int64_t mServerUpdateTimestampSec;
    int64_t mServerUpdateTimestampUSec;
    /*
     * The timestamp when the server sent the response to the update request to the client.
     * This is normally roughly equal to the update timestamp and is in particular guaranteed to
     * occur after the paint event.
     */
    int64_t mServerSendTimestampSec;
    int64_t mServerSendTimestampUSec;
} orv_latency_tester_update_response_t;

typedef enum orv_latency_tester_event_type_t
{
    /**
     * Result of a connect attempt. eventData is a orv_connect_result_t.
     **/
    ORV_LATENCY_TESTER_CONNECT_RESULT,
    /**
     * Disconnected from remote. May also be sent if connection failed (in addition to
     * ORV_LATENCY_TESTER_CONNECT_RESULT).
     * eventData is a orv_disconnected_t.
     **/
    ORV_LATENCY_TESTER_DISCONNECTED,

    /**
     * Result of an update request. eventData is a orv_latency_tester_update_response_t.
     **/
    ORV_LATENCY_TESTER_UPDATE_RESPONSE,
} orv_latency_tester_event_type_t;

typedef void (*orv_latency_tester_event_callback_t)(struct orv_latency_tester_client_t* client, orv_latency_tester_event_type_t event, const void* eventData, void* userData);

struct orv_latency_tester_client_t* orv_latency_tester_connect(struct orv_context_t* orvContext, orv_latency_tester_event_callback_t callback, void* userData, const char* hostName, int port);
void orv_latency_tester_disconnect(struct orv_latency_tester_client_t* client);
int orv_latency_tester_request_update(struct orv_latency_tester_client_t* client);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

