#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <algorithm>

#include <libopenrv/orv_error.h>
#include <libopenrv/orv_logging.h>
#include <libopenrv/libopenrv.h>

struct Options
{
    static const size_t mMaxHostNameLen = 256;
    char mHostName[mMaxHostNameLen + 1] = {};
    uint16_t mPort = 5900;
    char* mPassword = nullptr;

    ~Options()
    {
        free(mPassword);
    }
};

/**
 * Parse the specified command line arguments and store the results in @p options.
 *
 * @return TRUE on success, FALSE on failure
 **/
static bool readArguments(Options* options, orv_error_t* error, int argc, char** argv)
{
    orv_error_reset(error);
    for (int i = 1; i < argc; i++) {
        const char* param = argv[i];
        if (strcmp(param, "--host") == 0) {
            if (i + 1 >= argc) {
                orv_error_set(error, ORV_ERR_GENERIC, 0, "Expected argument for --host");
                return !error->mHasError;
            }
            i++;
            strncpy(options->mHostName, argv[i], Options::mMaxHostNameLen);
        }
        else if (strcmp(param, "--port") == 0) {
            if (i + 1 >= argc) {
                orv_error_set(error, ORV_ERR_GENERIC, 0, "Expected argument for --port");
                return !error->mHasError;
            }
            i++;
            options->mPort = atoi(argv[i]);
        }
        else if (strcmp(param, "--passwordfile") == 0) {
            if (i + 1 >= argc) {
                orv_error_set(error, ORV_ERR_GENERIC, 0, "Expected argument for --passwordfile");
                return !error->mHasError;
            }
            i++;
            struct stat statBuf;
            if (stat(argv[i], &statBuf) != 0) {
                if (errno == ENOENT) {
                    orv_error_set(error, ORV_ERR_GENERIC, 0, "File '%s' does not exist", argv[i]);
                }
                else {
                    orv_error_set(error, ORV_ERR_GENERIC, 0, "Failed to stat file '%s', errno: %d", argv[i], errno);
                }
                return !error->mHasError;
            }
            FILE* file = fopen(argv[i], "r");
            if (!file) {
                orv_error_set(error, ORV_ERR_GENERIC, 0, "Unable to open password file '%s'", argv[i]);
                return !error->mHasError;
            }
            size_t fileSize = std::min(statBuf.st_size, (off_t)ORV_MAX_PASSWORD_LEN);
            free(options->mPassword);
            options->mPassword = (char*)malloc(fileSize + 1);
            size_t read = fread(options->mPassword, 1, fileSize, file);
            options->mPassword[read] = '\0';
            fclose(file);
            if (read != fileSize) {
                fclose(file);
                orv_error_set(error, ORV_ERR_GENERIC, 0, "Error reading password file '%s'", argv[i]);
                return !error->mHasError;
            }
            if (read > 0 && options->mPassword[read - 1] == '\n') {
                options->mPassword[read - 1] = '\0';
            }
        }
        else {
            orv_error_set(error, ORV_ERR_GENERIC, 0, "Unknown argument %s", param);
        }
    }
    return !error->mHasError;
}

int main(int argc, char** argv)
{
    Options options;
    orv_error_t error;
    if (!readArguments(&options, &error, argc, argv)) {
        fprintf(stderr, "Failed to parse arguments. Error message: %s\n", error.mErrorMessage);
        return 1;
    }
    if (options.mHostName[0] == '\0') {
        strncpy(options.mHostName, "localhost", Options::mMaxHostNameLen);
    }

    orv_config_t orvConfig;
    orv_config_default(&orvConfig);
    orv_context_t* orvContext = orv_init(&orvConfig);
    orv_error_t connectError;
    orv_connect_options_t connectOptions;
    orv_connect_options_default(&connectOptions);
    if (orv_connect(orvContext, options.mHostName, options.mPort, options.mPassword, &connectOptions, &connectError) != 0) {
        fprintf(stderr, "Failed to start connecting to host '%s' on port %d\n  Error message: %s\n", options.mHostName, options.mPort, connectError.mErrorMessage);
        fflush(stderr);
        orv_destroy(orvContext);
        return 1;
    }

    // Wait for the connected event...
    bool connected = false;
    uint16_t framebufferWidth = 0;
    uint16_t framebufferHeight = 0;
    while (!connected) {
        orv_event_t* event = orv_poll_event(orvContext);
        if (event) {
            if (event->mEventType == ORV_EVENT_CONNECT_RESULT) {
                orv_connect_result_t* data = (orv_connect_result_t*)event->mEventData;
                if (data->mError.mHasError) {
                    fprintf(stderr, "FAILED to connect to host '%s' on port %d, error code: %d.%d, error message: %s\n", data->mHostName, (int)data->mPort, data->mError.mErrorCode, data->mError.mSubErrorCode, data->mError.mErrorMessage);
                    fflush(stderr);
                }
                else {
                    fprintf(stdout, "Connected to host '%s' on port %d.\n  Reported framebuffer: %dx%d\n  Desktop name: %s\n", data->mHostName, (int)data->mPort, (int)data->mFramebufferWidth, (int)data->mFramebufferHeight, data->mDesktopName);
                    const orv_communication_pixel_format_t* cpf = &data->mCommunicationPixelFormat;
                    fprintf(stdout, "  Reported communication pixel format: TrueColor: %s, BitsPerPixel: %d, Depth: %d, max r/g/b: %d/%d/%d, r/g/b shift: %d/%d/%d, BigEndian: %s\n",
                            cpf->mTrueColor ? "true" : "false",
                            (int)cpf->mBitsPerPixel,
                            (int)cpf->mDepth,
                            (int)cpf->mColorMax[0],
                            (int)cpf->mColorMax[1],
                            (int)cpf->mColorMax[2],
                            (int)cpf->mColorShift[0],
                            (int)cpf->mColorShift[1],
                            (int)cpf->mColorShift[2],
                            cpf->mBigEndian ? "true" : "false");
                    fflush(stdout);
                    connected = true;
                    framebufferWidth = data->mFramebufferWidth;
                    framebufferHeight = data->mFramebufferHeight;
                }
            }
            else {
                fprintf(stderr, "Received unexpected event %d while trying to connect. Assuming connection failed.\n", event->mEventType);
                fprintf(stderr, "Details:\n");
                fflush(stderr);
                orv_event_print_to_log(orvContext, event);
            }
            if (!connected) {
                orv_event_destroy(event);
                orv_destroy(orvContext);
                return 1;
            }
            orv_event_destroy(event);
        }
        else {
            usleep(10000);
        }
    }

    uint8_t x = 0;
    uint8_t y = 0;
    orv_request_framebuffer_update(orvContext, x, y, framebufferWidth, framebufferHeight);

    fprintf(stderr, "Sleeping...\n");
    sleep(2);

    orv_event_t* event = orv_poll_event(orvContext);
    while (event != nullptr) {
        fprintf(stdout, "Received event '%d' from server\n", (int)event->mEventType);
        orv_event_print_to_log(orvContext, event);
        orv_event_destroy(event);
        event = orv_poll_event(orvContext);
    }

    orv_connection_info_t info;
    orv_vnc_server_capabilities_t capabilities;
    orv_get_vnc_connection_info(orvContext, &info, &capabilities);
    orv_connection_info_print_to_log(orvContext, &info);
    orv_vnc_server_capabilities_print_to_log(orvContext, &capabilities);

    fprintf(stderr, "Leaving application.\n");
    fflush(stderr);
    orv_destroy(orvContext);
    return 0;
}

