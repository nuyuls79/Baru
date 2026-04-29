package com.lagradost.cloudstream3.utils

import android.content.Context
import android.util.Log

object NativeSecurity {
    init {
        try {
            System.loadLibrary("native-lib")
            Log.e("NativeSecurity", "Library loaded, forcing crash as test")
            // Paksa crash untuk memastikan library aktif
            throw RuntimeException("Native library loaded successfully")
        } catch (e: UnsatisfiedLinkError) {
            Log.e("NativeSecurity", "FAILED to load native-lib", e)
        }
    }

    external fun checkSignature(context: Context): Boolean
    external fun checkPackageName(context: Context): Boolean

    fun validate(context: Context): Boolean {
        return try {
            checkSignature(context) && checkPackageName(context)
        } catch (e: Exception) {
            Log.e("NativeSecurity", "Validation exception", e)
            false
        }
    }
}
