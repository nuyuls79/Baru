package com.lagradost.cloudstream3

import android.content.Context
import android.provider.Settings
import com.lagradost.cloudstream3.utils.RepoProtector

object PremiumManager {
    // URL repo – tetap dari RepoProtector (native + XOR fallback)
    val PREMIUM_REPO_URL = RepoProtector.getPremiumRepoUrl()
    val FREE_REPO_URL = RepoProtector.getFreeRepoUrl()

    // Native methods (implementasi di xsecure.cpp)
    private external fun nativeActivatePremium(code: String, deviceId: String, context: Context): Boolean
    private external fun nativeIsPremium(context: Context): Boolean
    private external fun nativeGetExpiryMillis(context: Context): Long
    private external fun nativeGetExpiryString(context: Context): String
    private external fun nativeDeactivatePremium(context: Context)

    // Load library saat class pertama diakses
    init {
        System.loadLibrary("xsecure")
    }

    // Fungsi getDeviceId – tetap di Kotlin (sama seperti asli)
    fun getDeviceId(context: Context): String {
        val androidId = Settings.Secure.getString(context.contentResolver, Settings.Secure.ANDROID_ID) ?: "00000000"
        return kotlin.math.abs(androidId.hashCode()).toString().take(8)
    }

    // API publik – semua memanggil native
    fun activatePremiumWithCode(context: Context, code: String, deviceId: String): Boolean {
        return nativeActivatePremium(code, deviceId, context)
    }

    fun isPremium(context: Context): Boolean = nativeIsPremium(context)
    fun deactivatePremium(context: Context) = nativeDeactivatePremium(context)
    fun getExpiryDateString(context: Context): String = nativeGetExpiryString(context)
    fun getExpiryDateMillis(context: Context): Long = nativeGetExpiryMillis(context)
}