package com.lagradost.cloudstream3

import android.util.Log

object SignatureUtils {
    private var loaded = false

    init {
        try {
            System.loadLibrary("signature_check")
            loaded = true
            Log.e("SignatureUtils", "Library loaded")
        } catch (e: UnsatisfiedLinkError) {
            Log.e("SignatureUtils", "Failed to load library", e)
        }
    }

    external fun getSignatureNative(): String

    fun getValidationInfoBlocking(): String {
        if (!loaded) return "Library not loaded"
        return try {
            getSignatureNative()
        } catch (e: Exception) {
            "Exception: ${e.message}"
        }
    }
}