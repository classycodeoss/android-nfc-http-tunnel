package com.classycode.nfchttptun;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

/**
 * Helper class to store and retrieve a user-defined URL.
 *
 * @author Alex Suzuki, Classy Code GmbH, 2017
 */
public class TunnelSettings {

    private static final String TAG = Constants.LOG_TAG;

    private static final String PREFS_NAME = "nfchttptun";
    private static final String PREF_KEY_URL = "url";

    private static final String DEFAULT_URL = "https://www.classycode.com/en/";

    static void setUrl(Context context, String url) {
        final SharedPreferences prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        prefs.edit().putString(PREF_KEY_URL, url).apply();
        Log.i(TAG, "Stored url: " + url);
    }

    static String getUrl(Context context) {
        final SharedPreferences prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        return prefs.getString("url", DEFAULT_URL);
    }
}
