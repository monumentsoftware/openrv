package com.monument_software.libopenrv;

// TODO: how to destroy the context properly? does the user have to call dispose() or so?

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

public class ORVContext {
    public static class ConnectOptions {
        /**
         * Initial value of {@link ORVContext#isViewOnly}.
         *
         * Can be changed after connecting using {@link ORVContext#setViewOnly(boolean)}.
         **/
        public boolean viewOnly = false;

        // TODO: mCommunicationQualityProfile
        // TODO: mCommunicationPixelFormat?
    }
    private long mContext = 0;

    public ORVContext() {
        mContext = OpenRVJNI.init();
    }

    // TODO: must be called when finished with the context. can we somehow automate this?
    public void dispose() {
        OpenRVJNI.destroy(mContext);
        mContext = 0;
    }

    public boolean connectToHost(@NonNull String host, int port, @Nullable String password) {
        return connectToHost(host, port, password, null);
    }
    public boolean connectToHost(@NonNull String host, int port, @Nullable String password, @Nullable ConnectOptions connectOptions) {
        if (connectOptions == null) {
            connectOptions = new ConnectOptions();
        }
        return OpenRVJNI.connectToHost(mContext, OpenRVJNI.byteArrayFromStringNonNull(host), port, OpenRVJNI.byteArrayFromString(password), connectOptions.viewOnly);
    }

    public void setViewOnly(boolean viewOnly) {
        OpenRVJNI.setViewOnly(mContext, viewOnly);
    }
    public boolean isViewOnly() {
        if (mContext == 0) {
            return true;
        }
        return OpenRVJNI.isViewOnly(mContext);
    }

}
