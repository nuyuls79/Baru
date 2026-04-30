package com.lagradost.cloudstream3.utils

object RepoProtector {

    init {
        System.loadLibrary("xsecure")
    }

    // Utamakan native – jika gagal, fallback ke XOR (opzional)
    @JvmStatic
    fun getPremiumRepoUrl(): String {
        return try {
            nativeGetPremiumRepoUrl()
        } catch (e: UnsatisfiedLinkError) {
            RepoDecryptor.getPremiumRepoUrl()
        }
    }

    @JvmStatic
    fun getFreeRepoUrl(): String {
        return try {
            nativeGetFreeRepoUrl()
        } catch (e: UnsatisfiedLinkError) {
            RepoDecryptor.getFreeRepoUrl()
        }
    }

    // Native methods (private)
    @JvmStatic
    private external fun nativeGetPremiumRepoUrl(): String

    @JvmStatic
    private external fun nativeGetFreeRepoUrl(): String
}
