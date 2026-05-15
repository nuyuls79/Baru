package com.lagradost.cloudstream3

import android.content.Context
import android.provider.Settings
import androidx.preference.PreferenceManager
import java.security.MessageDigest
import java.util.Calendar

object PremiumManager {

    init {
        System.loadLibrary("xsecure")
    }

    private const val PREF_IS_PREMIUM = "is_premium_user"
    private const val PREF_EXPIRY_DATE = "premium_expiry_date"

    private const val SALT =
        "ADIXTREAM_SECRET_KEY_2026_SECURE"

    private const val EPOCH_YEAR = 2025

    // ==================== NATIVE ====================
    external fun nativeGetPremiumRepoUrl(): String
    external fun nativeGetFreeRepoUrl(): String
    external fun nativeIsPremium(context: Context): Boolean

    // ==================== REPO ====================
    fun getPremiumRepoUrl(): String {
        return nativeGetPremiumRepoUrl()
    }

    fun getFreeRepoUrl(): String {
        return nativeGetFreeRepoUrl()
    }

    // ==================== PREFS ====================
    private fun getPrefs(context: Context) =
        PreferenceManager.getDefaultSharedPreferences(context)

    // ==================== DEVICE ID ====================
    fun getDeviceId(context: Context): String {

        val androidId =
            Settings.Secure.getString(
                context.contentResolver,
                Settings.Secure.ANDROID_ID
            ) ?: "00000000"

        return kotlin.math.abs(
            androidId.hashCode()
        ).toString().take(8)
    }

    // ==================== ACTIVATE ====================
    fun activatePremiumWithCode(
        context: Context,
        code: String,
        deviceId: String
    ): Boolean {

        if (code.length != 6) {
            return false
        }

        return try {

            val inputCode =
                code.uppercase()

            val datePartHex =
                inputCode.substring(0, 3)

            val sigPartHex =
                inputCode.substring(3, 6)

            val checkInput =
                "$deviceId$datePartHex$SALT"

            val md =
                MessageDigest.getInstance("MD5")

            val digest =
                md.digest(
                    checkInput.toByteArray(
                        Charsets.UTF_8
                    )
                )

            val expectedSig =
                digest.joinToString("") {
                    "%02x".format(it)
                }.substring(0, 3).uppercase()

            if (sigPartHex != expectedSig) {
                return false
            }

            val daysFromEpoch =
                datePartHex.toInt(16)

            val expiryCal =
                Calendar.getInstance().apply {

                    set(
                        EPOCH_YEAR,
                        Calendar.JANUARY,
                        1,
                        0,
                        0,
                        0
                    )

                    set(Calendar.MILLISECOND, 0)

                    add(
                        Calendar.DAY_OF_YEAR,
                        daysFromEpoch
                    )

                    set(Calendar.HOUR_OF_DAY, 23)
                    set(Calendar.MINUTE, 59)
                    set(Calendar.SECOND, 59)
                }

            val expiryTime =
                expiryCal.timeInMillis

            if (
                System.currentTimeMillis() >
                expiryTime
            ) {
                return false
            }

            // ==================== SAVE ====================
            getPrefs(context)
                .edit()
                .apply {

                    putBoolean(
                        PREF_IS_PREMIUM,
                        true
                    )

                    putLong(
                        PREF_EXPIRY_DATE,
                        expiryTime
                    )

                    apply()
                }

            true

        } catch (e: Exception) {

            false
        }
    }

    // ==================== LOCAL CHECK ====================
    private fun isPremiumLocal(
        context: Context
    ): Boolean {

        return try {

            val prefs =
                getPrefs(context)

            val expiry =
                prefs.getLong(
                    PREF_EXPIRY_DATE,
                    0
                )

            val enabled =
                prefs.getBoolean(
                    PREF_IS_PREMIUM,
                    false
                )

            enabled &&
            expiry > System.currentTimeMillis()

        } catch (e: Exception) {

            false
        }
    }

    // ==================== MAIN PREMIUM CHECK ====================
    fun isPremium(context: Context): Boolean {

        // local prefs prioritas utama
        if (isPremiumLocal(context)) {
            return true
        }

        // fallback native anti-patch
        return try {

            nativeIsPremium(context)

        } catch (e: Exception) {

            false
        }
    }

    // ==================== DEACTIVATE ====================
    fun deactivatePremium(
        context: Context
    ) {

        getPrefs(context)
            .edit()
            .apply {

                putBoolean(
                    PREF_IS_PREMIUM,
                    false
                )

                putLong(
                    PREF_EXPIRY_DATE,
                    0
                )

                apply()
            }
    }

    // ==================== EXPIRY STRING ====================
    fun getExpiryDateString(
        context: Context
    ): String {

        val date =
            getPrefs(context)
                .getLong(
                    PREF_EXPIRY_DATE,
                    0
                )

        return if (date == 0L) {

            "Non-Premium"

        } else {

            java.text.SimpleDateFormat(
                "dd MMM yyyy",
                java.util.Locale.getDefault()
            ).format(
                java.util.Date(date)
            )
        }
    }

    // ==================== EXPIRY MILLIS ====================
    fun getExpiryDateMillis(
        context: Context
    ): Long {

        return getPrefs(context)
            .getLong(
                PREF_EXPIRY_DATE,
                0
            )
    }
}