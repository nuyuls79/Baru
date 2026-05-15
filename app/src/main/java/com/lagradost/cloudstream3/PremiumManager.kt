package com.lagradost.cloudstream3

import android.content.Context
import android.provider.Settings
import androidx.preference.PreferenceManager
import java.security.MessageDigest
import java.text.SimpleDateFormat
import java.util.Calendar
import java.util.Date
import java.util.Locale
import com.lagradost.cloudstream3.utils.RepoProtector

object PremiumManager {

    init {
        try {
            System.loadLibrary("xsecure")
        } catch (_: Exception) {
        }
    }

    private const val PREF_IS_PREMIUM = "is_premium_user"
    private const val PREF_EXPIRY_DATE = "premium_expiry_date"

    private const val SALT = "ADIXTREAM_SECRET_KEY_2026_SECURE"
    private const val EPOCH_YEAR = 2025

    val PREMIUM_REPO_URL = RepoProtector.getPremiumRepoUrl()
    val FREE_REPO_URL = RepoProtector.getFreeRepoUrl()

    external fun nativeSetPremium(
        premium: Boolean,
        expiry: Long,
        deviceId: String
    )

    external fun nativeIsPremium(
        currentTime: Long,
        deviceId: String
    ): Boolean

    external fun nativeGetExpiry(): Long

    external fun nativeClearPremium()

    private fun getPrefs(context: Context) =
        PreferenceManager.getDefaultSharedPreferences(context)

    fun getDeviceId(context: Context): String {
        return try {
            val androidId = Settings.Secure.getString(
                context.contentResolver,
                Settings.Secure.ANDROID_ID
            ) ?: "00000000"

            androidId
                .replace("[^A-Za-z0-9]".toRegex(), "")
                .uppercase()
                .take(16)

        } catch (_: Exception) {
            "00000000"
        }
    }

    fun activatePremiumWithCode(
        context: Context,
        code: String,
        deviceId: String
    ): Boolean {

        if (code.length != 6) return false

        return try {

            val inputCode = code.uppercase()

            val datePartHex = inputCode.substring(0, 3)
            val sigPartHex = inputCode.substring(3, 6)

            val checkInput = "$deviceId$datePartHex$SALT"

            val md = MessageDigest.getInstance("MD5")

            val digest = md.digest(
                checkInput.toByteArray(Charsets.UTF_8)
            )

            val expectedSig = digest
                .joinToString("") { "%02x".format(it) }
                .substring(0, 3)
                .uppercase()

            if (sigPartHex != expectedSig) {
                return false
            }

            val daysFromEpoch = datePartHex.toInt(16)

            val expiryCal = Calendar.getInstance().apply {

                set(EPOCH_YEAR, Calendar.JANUARY, 1, 0, 0, 0)

                set(Calendar.MILLISECOND, 0)

                add(Calendar.DAY_OF_YEAR, daysFromEpoch)

                set(Calendar.HOUR_OF_DAY, 23)
                set(Calendar.MINUTE, 59)
                set(Calendar.SECOND, 59)
            }

            val expiryTime = expiryCal.timeInMillis

            if (System.currentTimeMillis() > expiryTime) {
                return false
            }

            getPrefs(context)
                .edit()
                .putBoolean(PREF_IS_PREMIUM, true)
                .putLong(PREF_EXPIRY_DATE, expiryTime)
                .apply()

            nativeSetPremium(
                true,
                expiryTime,
                deviceId
            )

            true

        } catch (_: Exception) {
            false
        }
    }

    fun isPremium(context: Context): Boolean {

        return try {

            val deviceId = getDeviceId(context)

            val nativePremium = nativeIsPremium(
                System.currentTimeMillis(),
                deviceId
            )

            val nativeExpiry = nativeGetExpiry()

            if (nativePremium && nativeExpiry > System.currentTimeMillis()) {

                getPrefs(context)
                    .edit()
                    .putBoolean(PREF_IS_PREMIUM, true)
                    .putLong(PREF_EXPIRY_DATE, nativeExpiry)
                    .apply()

                true

            } else {

                deactivatePremium(context)

                false
            }

        } catch (_: Exception) {

            val prefs = getPrefs(context)

            val expiryDate =
                prefs.getLong(PREF_EXPIRY_DATE, 0)

            expiryDate > System.currentTimeMillis()
        }
    }

    fun deactivatePremium(context: Context) {

        try {
            nativeClearPremium()
        } catch (_: Exception) {
        }

        getPrefs(context)
            .edit()
            .putBoolean(PREF_IS_PREMIUM, false)
            .putLong(PREF_EXPIRY_DATE, 0)
            .apply()
    }

    fun getExpiryDateString(context: Context): String {

        val expiry = try {
            nativeGetExpiry()
        } catch (_: Exception) {
            getPrefs(context).getLong(PREF_EXPIRY_DATE, 0)
        }

        return if (expiry <= 0L) {
            "Non-Premium"
        } else {
            SimpleDateFormat(
                "dd MMM yyyy",
                Locale.getDefault()
            ).format(Date(expiry))
        }
    }

    fun getExpiryDateMillis(context: Context): Long {

        return try {
            nativeGetExpiry()
        } catch (_: Exception) {
            getPrefs(context).getLong(PREF_EXPIRY_DATE, 0)
        }
    }

    fun getCurrentRepo(context: Context): String {
        return if (isPremium(context)) {
            PREMIUM_REPO_URL
        } else {
            FREE_REPO_URL
        }
    }
}