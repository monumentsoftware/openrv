package com.monument_software.libopenrv;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import java.io.UnsupportedEncodingException;

public class OpenRVJNI {
    static {
        System.loadLibrary("openrv_android");
    }

    /**
     * Helper function that obtains the UTF-8 representation of the provided string.
     *
     * This bytearray can easily be provided to C functions.
     */
    @Nullable static byte[] byteArrayFromString(@Nullable String s) {
        if (s == null) {
            return null;
        }
        final byte[] b;
        try {
            return s.getBytes("UTF-8");
        } catch (UnsupportedEncodingException ex) {
            // cannot happen for UTF-8 on android.
            return null;
        }
    }
    @NonNull static byte[] byteArrayFromStringNonNull(@NonNull String s) {
        final byte[] b;
        try {
            return s.getBytes("UTF-8");
        } catch (UnsupportedEncodingException ex) {
            // cannot happen for UTF-8 on android.
            return new byte[0];
        }
    }

    public static native long init();
    public static native void destroy(long context);
    public static native boolean connectToHost(long context, @NonNull byte[] host, int port, @Nullable byte[] password, boolean viewOnly);
    public static native void setViewOnly(long context, boolean viewOnly);
    public static native boolean isViewOnly(long context);
}
