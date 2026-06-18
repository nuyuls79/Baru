package com.lagradost.cloudstream3.utils

object RepoProtector {

    init {
        System.loadLibrary("xsecure")
    }

    // 🔐 Hanya native method – tidak ada string di sini
    @JvmStatic
    external fun getFirebaseUrl(): String

    @JvmStatic
    external fun getPremiumRepoUrl(): String

    // Jika masih perlu Firebase, tambahkan native juga, atau simpan di native sekalian
    @JvmStatic
    external fun getFreeRepoUrl(): String
}

