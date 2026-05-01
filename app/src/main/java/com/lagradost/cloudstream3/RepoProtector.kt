package com.lagradost.cloudstream3.utils

import android.util.Base64
import com.lagradost.cloudstream3.BuildConfig

object RepoProtector {

    init {
        try {
            System.loadLibrary("xsecure")
        } catch (e: UnsatisfiedLinkError) {
            // native tidak tersedia, fallback ke XOR
        }
    }

    // Native method (private)
    @JvmStatic
    private external fun nativeGetPremiumRepoUrl(): String

    @JvmStatic
    private external fun nativeGetFreeRepoUrl(): String

    // XOR fallback
    private val xorKey: String by lazy {
        BuildConfig.OBFUSCATED_KEY.map { (it - 7).toChar() }.joinToString("")
    }

    private fun xorDecrypt(hex: String): String {
        val bytes = hex.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
        val keyBytes = xorKey.toByteArray(Charsets.UTF_8)
        val dec = ByteArray(bytes.size)
        for (i in bytes.indices) dec[i] = (bytes[i].toInt() xor keyBytes[i % keyBytes.size].toInt()).toByte()
        val b64 = String(dec, Charsets.UTF_8)
        val url = Base64.decode(b64, Base64.DEFAULT)
        return String(url, Charsets.UTF_8)
    }

    // Public getter — coba native dulu, fallback XOR
    fun getPremiumRepoUrl(): String = try {
        nativeGetPremiumRepoUrl()
    } catch (e: Throwable) {
        xorDecrypt(BuildConfig.PREMIUM_REPO_XOR)   // <-- UBAH KE PREMIUM_REPO_XOR
    }

    fun getFreeRepoUrl(): String = try {
        nativeGetFreeRepoUrl()
    } catch (e: Throwable) {
        xorDecrypt(BuildConfig.FREE_REPO_XOR)       // <-- UBAH KE FREE_REPO_XOR
    }
}