#include <jni.h>
#include <android/log.h>
#include <string>
#include <string.h>

#include <libopenrv/libopenrv.h>

#define ORV_UNUSED(x) (void)x

namespace openrv {
namespace android {

static const size_t gLogTagMaxize = 100;
static char gLogTag[gLogTagMaxize] = "openrv";

void ovr_log_android(int androidLogPriority, const char* file, int line, const char* msg, ...) __attribute__((format(printf, 4, 5)));

static jmethodID getStaticMethodID(const char* name, const char* sig);
bool checkJavaException(JNIEnv* env, const char* file, int line, bool fatal = true, const std::string& additionalInfo = std::string());
JNIEnv* getEnvForCurrentThread();
bool checkThreadIsMainThreadFatal(const char* func, const char* file, int line);

struct OrvAndroidUserData
{

};

#define OVR_ERROR_ANDROID(...) ovr_log_android(ANDROID_LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)

void ovr_log_android(int androidLogPriority, const char* file, int line, const char* msg, ...)
{
    const char* fileName = strrchr(file, '/');
    if (fileName == nullptr) {
        fileName = file;
    }
    else {
        fileName++;
    }
    const size_t bufSize = 2048;
    char buf[bufSize + 1];
    buf[bufSize] = '\0';
    va_list lst;
    va_start(lst, msg);
    vsnprintf(buf, bufSize, msg, lst);
    va_end(lst);
    __android_log_print(androidLogPriority, gLogTag, "%s:%d: %s", fileName, line, buf);
}


/**
 * @pre Called on the main thread (exactly the same that called JNI_OnLoad()).
 **/
/*
jmethodID getStaticMethodID(const char* name, const char* sig)
{
    jmethodID id = g_MainThreadEnv->GetStaticMethodID(g_PyramidUIClass, name, sig);
    checkJavaException(g_MainThreadEnv, __FILE__, __LINE__); // GetStaticMethodID() may throw
    if (id == nullptr) {
        OVR_ERROR_ANDROID("GetStaticMethodID() failed for %s with sig %s", name, sig);
    }
    return id;
}*/


} // namespace openrv::android
} // namespace openrv


using namespace openrv::android;

extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved)
{
    ORV_UNUSED(reserved);
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved)
{
    ORV_UNUSED(reserved);
}

JNIEXPORT jlong JNICALL Java_com_monument_software_libopenrv_OpenRVJNI_init()
{
    orv_config_t config;
    orv_config_zero(&config);
    config.mLogCallback = nullptr; // TODO
    config.mEventCallback = nullptr; // TODO
    config.mUserData[0] = (void*)0; // TODO vm + thread pointers, maybe also log tag etc.
    orv_context_t* context = orv_init(&config);
    return (jlong)context;
}

JNIEXPORT void JNICALL Java_com_monument_software_libopenrv_OpenRVJNI_destroy(jlong context)
{
    orv_context_t* ctx = (orv_context_t*)context;
    if (!ctx) {
        return;
    }
    OrvAndroidUserData* userData = static_cast<OrvAndroidUserData*>(orv_get_user_data(ctx, ORV_USER_DATA_0));
    delete userData;
    orv_destroy(ctx);
}
JNIEXPORT jboolean JNICALL Java_com_monument_software_libopenrv_OpenRVJNI_connectToHost(jlong context, jbyteArray host, jint port, jbyteArray password, jboolean viewOnly)
{
    orv_context_t* ctx = (orv_context_t*)context;
    if (!ctx) {
        return (jboolean)false;
    }
    orv_connect_options_t connectOptions;
    orv_connect_options_default(&connectOptions);
    connectOptions.mViewOnly = viewOnly;
    orv_error_t error;

    std::string hostString; // TODO
    std::string passwordString; // TODO

    //return (jboolean)orv_connect(ctx, hostString.c_str(), (int)port, passwordString.c_str(), &connectOptions, &error);

    // TODO: return an error object instead of a boolean on error?
    // -> probably add an OrvError class to java (we probably need one anyway)
    return (jboolean)false;
}

JNIEXPORT void JNICALL Java_com_monument_software_libopenrv_OpenRVJNI_setViewOnly(jlong context, jboolean viewOnly)
{
    orv_context_t* ctx = (orv_context_t*)context;
    if (!ctx) {
        return;
    }
    orv_set_viewonly(ctx, (uint8_t)viewOnly);
}

JNIEXPORT jboolean JNICALL Java_com_monument_software_libopenrv_OpenRVJNI_isViewOnly(jlong context)
{
    orv_context_t* ctx = (orv_context_t*)context;
    if (!ctx) {
        return (jboolean)true;
    }
    return (jboolean)orv_is_viewonly(ctx);
}

} // extern "C"
