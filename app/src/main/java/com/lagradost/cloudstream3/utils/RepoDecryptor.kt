package com.lagradost.cloudstream3.utils

import android.util.Base64
import com.lagradost.cloudstream3.BuildConfig

object RepoDecryptor {

    fun getPremiumRepoUrl(): String = decrypt(BuildConfig.PREMIUM_REPO_XOR)
    fun getFreeRepoUrl(): String = decrypt(BuildConfig.FREE_REPO_XOR)

    private fun decrypt(hexEncrypted: String): String {
        val key = BuildConfig.OBFUSCATED_KEY.map { (it - 7).toChar() }.joinToString("")
        val bytes = hexEncrypted.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
        val keyBytes = key.toByteArray(Charsets.UTF_8)
        val decrypted = ByteArray(bytes.size)
        for (i in bytes.indices) {
            decrypted[i] = (bytes[i].toInt() xor keyBytes[i % keyBytes.size].toInt()).toByte()
        }
        val base64String = String(decrypted, Charsets.UTF_8)
        val urlBytes = Base64.decode(base64String, Base64.DEFAULT)
        return String(urlBytes, Charsets.UTF_8)
    }
}
