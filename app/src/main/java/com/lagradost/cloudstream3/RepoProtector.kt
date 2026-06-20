package com.lagradost.cloudstream3.utils

object RepoProtector {

    init {
        System.loadLibrary("xsecure")
    }

    @JvmStatic
    external fun getPremiumRepoUrl(): String

    @JvmStatic
    external fun getFreeRepoUrl(): String

    @JvmStatic
    external fun getFirebaseUrl(): String
}