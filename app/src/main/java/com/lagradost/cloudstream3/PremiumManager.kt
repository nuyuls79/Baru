package com.lagradost.cloudstream3

import android.content.Context
import android.provider.Settings
import androidx.security.crypto.EncryptedSharedPreferences
import androidx.security.crypto.MasterKeys
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import com.lagradost.cloudstream3.utils.RepoProtector

object PremiumManager {
    private const val PREF_NAME = "adixtream_premium_encrypted"
    private const val PREF_IS_PREMIUM = "is_premium_user"
    private const val PREF_EXPIRY_DATE = "premium_expiry_date"

    private fun getPrefs(context: Context) = EncryptedSharedPreferences.create(
        PREF_NAME,
        MasterKeys.getOrCreate(MasterKeys.AES256_GCM_SPEC),
        context,
        EncryptedSharedPreferences.PrefKeyEncryptionScheme.AES256_SIV,
        EncryptedSharedPreferences.PrefValueEncryptionScheme.AES256_GCM
    )

    val PREMIUM_REPO_URL = RepoProtector.getPremiumRepoUrl()
    val FREE_REPO_URL = RepoProtector.getFreeRepoUrl()

    fun getDeviceId(context: Context): String {
        val androidId = Settings.Secure.getString(context.contentResolver, Settings.Secure.ANDROID_ID) ?: "00000000"
        return kotlin.math.abs(androidId.hashCode()).toString().take(8)
    }

    fun activatePremiumWithCode(context: Context, code: String, deviceId: String): Boolean {
        return nativeActivatePremium(code, deviceId)
    }

    fun isPremium(context: Context): Boolean {
        val prefs = getPrefs(context)
        val expiryDate = prefs.getLong(PREF_EXPIRY_DATE, 0)
        return if (expiryDate > System.currentTimeMillis()) {
            true
        } else {
            if (prefs.getBoolean(PREF_IS_PREMIUM, false)) deactivatePremium(context)
            false
        }
    }

    fun deactivatePremium(context: Context) {
        getPrefs(context).edit().apply {
            putBoolean(PREF_IS_PREMIUM, false)
            putLong(PREF_EXPIRY_DATE, 0)
            apply()
        }
    }

    fun getExpiryDateString(context: Context): String {
        val date = getPrefs(context).getLong(PREF_EXPIRY_DATE, 0)
        return if (date == 0L) "Non-Premium" else SimpleDateFormat("dd MMM yyyy", Locale.getDefault()).format(Date(date))
    }

    fun getExpiryDateMillis(context: Context): Long {
        return getPrefs(context).getLong(PREF_EXPIRY_DATE, 0)
    }

    // Dipanggil dari native
    @JvmStatic
    fun persistPremiumData(isPremium: Boolean, expiryMillis: Long) {
        // Ini akan dipanggil oleh native setelah validasi
        // Kita perlu context, tapi native tidak punya. Solusinya gunakan callback di aktivasi.
        // Untuk native langsung memanggil ini, dibutuhkan context global. Nanti akan kita atur.
    }

    // Native methods
    @JvmStatic
    external fun nativeActivatePremium(code: String, deviceId: String): Boolean

    @JvmStatic
    external fun isSignatureValid(): Boolean
}