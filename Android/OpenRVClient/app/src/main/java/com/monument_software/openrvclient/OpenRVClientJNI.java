package com.monument_software.openrvclient;

import android.util.Log;

import com.monument_software.libopenrv.OpenRVJNI;


public class OpenRVClientJNI {
    private static final String LOG_TAG = "openrvclient";
    static {
        try {
            // Force loading of OpenRVJNI class by jvm.
            // This ensures the static section (which includes System.loadLibrary("openrv")) is
            // run before we call loadLibrary() ourselves.
            OpenRVJNI.class.newInstance();
        } catch (Exception ex) {
            // ignore
        }
        try {
            System.loadLibrary("openrvclient");
        }
        catch (UnsatisfiedLinkError ex) {
            Log.e(LOG_TAG, "Failed loading openrvclient shared library (link error). Startup failed. Exception: " + ex);
            System.exit(1);
        }
        catch (Exception ex) {
            Log.e(LOG_TAG, "Failed loading openrvclient shared library. Startup failed. Exception: " + ex);
            System.exit(1);

        }
        nativeInit();
    }

    private static native void nativeInit();

    public static native void init();
}
