#include <jni.h>

#include <libopenrv/libopenrv.h>

#define UNUSED(x) (void)x

extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved)
{
    UNUSED(reserved);
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved)
{
    UNUSED(reserved);
}

JNICALL void Java_com_monument_1software_openrvclient_OpenRVClientJNI_nativeInit(JNIEnv* env, jclass cl)
{
    UNUSED(cl);
    // TODO
}

JNICALL void Java_com_monument_1software_openrvclient_OpenRVClientJNI_init(JNIEnv* env, jclass cl)
{
    UNUSED(cl);
    // TODO
}

} // extern "C"
