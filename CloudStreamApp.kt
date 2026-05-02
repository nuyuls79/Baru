package com.lagradost.cloudstream3

import android.app.Activity
import android.app.Application
import android.content.Context
import android.content.ContextWrapper
import android.content.Intent
import android.content.pm.PackageManager
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import android.os.Build
import android.os.Bundle
import android.os.Process
import android.provider.Settings
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentActivity
import coil3.ImageLoader
import coil3.PlatformContext
import coil3.SingletonImageLoader
import com.lagradost.api.setContext
import com.lagradost.cloudstream3.plugins.PluginManager
import com.lagradost.cloudstream3.ui.settings.Globals.EMULATOR
import com.lagradost.cloudstream3.ui.settings.Globals.TV
import com.lagradost.cloudstream3.ui.settings.Globals.isLayout
import com.lagradost.cloudstream3.utils.AppContextUtils.openBrowser
import com.lagradost.cloudstream3.utils.DataStore.getKey
import com.lagradost.cloudstream3.utils.DataStore.getKeys
import com.lagradost.cloudstream3.utils.DataStore.removeKey
import com.lagradost.cloudstream3.utils.DataStore.removeKeys
import com.lagradost.cloudstream3.utils.DataStore.setKey
import com.lagradost.cloudstream3.utils.ImageLoader.buildImageLoader
import java.io.File
import java.io.FileNotFoundException
import java.io.PrintStream
import java.lang.ref.WeakReference
import java.security.MessageDigest
import kotlin.system.exitProcess

class ExceptionHandler(
    val errorFile: File,
    val onError: (() -> Unit)
) : Thread.UncaughtExceptionHandler {

    override fun uncaughtException(thread: Thread, error: Throwable) {
        try {
            val threadId = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.BAKLAVA) {
                thread.threadId()
            } else {
                @Suppress("DEPRECATION")
                thread.id
            }

            PrintStream(errorFile).use { ps ->
                ps.println("Currently loading extension: ${PluginManager.currentlyLoading ?: "none"}")
                ps.println("Fatal exception on thread ${thread.name} ($threadId)")
                error.printStackTrace(ps)
            }
        } catch (_: FileNotFoundException) {
        }
        try {
            onError()
        } catch (_: Exception) {
        }
        exitProcess(1)
    }
}

@Prerelease
class CloudStreamApp : Application(), SingletonImageLoader.Factory {

    private var activityCount = 0

    // 🔒 ORIGINAL SIGNATURE SESUAI FILE YANG BERJALAN (Huruf kecil, tanpa titik dua)
    private val ORIGINAL_SIGNATURE = "b115983ab9dffa173ee350fee7a6eef515cbb16d0d06c4054579cdc6487e68fc"

    override fun onCreate() {
        super.onCreate()

        // === PROTEKSI KEAMANAN ===
        
        // Cek Signature hanya pada versi RELEASE (GitHub Actions)
        if (!BuildConfig.DEBUG) {
            // 1. Validasi Tanda Tangan (Format Hex Kecil)
            if (!isSignatureValid()) {
                performSilentKill()
                return
            }

            // 2. Deteksi Alat Modifikasi (Anti MT/NP Manager)
            if (isModifiedByTool()) {
                performSilentKill()
                return
            }
        }

        // 3. Deteksi VPN/Proxy
        if (isProxyOrVpnActive()) {
            performSilentKill()
            return
        }

        registerActivityLifecycleCallbacks(object : ActivityLifecycleCallbacks {
            override fun onActivityCreated(activity: Activity, savedInstanceState: Bundle?) {}
            override fun onActivityStarted(activity: Activity) {
                activityCount++
            }
            override fun onActivityResumed(activity: Activity) {}
            override fun onActivityPaused(activity: Activity) {}
            override fun onActivityStopped(activity: Activity) {
                activityCount--
                if (activityCount <= 0) {
                    clearAllCache()
                }
            }
            override fun onActivitySaveInstanceState(activity: Activity, outState: Bundle) {}
            override fun onActivityDestroyed(activity: Activity) {}
        })

        ExceptionHandler(filesDir.resolve("last_error")) {
            val intent = context!!.packageManager.getLaunchIntentForPackage(context!!.packageName)
            startActivity(Intent.makeRestartActivityTask(intent!!.component))
        }.also {
            exceptionHandler = it
            Thread.setDefaultUncaughtExceptionHandler(it)
        }
    }

    /**
     * Memverifikasi tanda tangan APK sesuai format yang Anda gunakan (Hex kecil tanpa titik dua)
     */
    private fun isSignatureValid(): Boolean {
        if (ORIGINAL_SIGNATURE.isBlank()) return true

        try {
            val packageInfo = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                packageManager.getPackageInfo(packageName, PackageManager.GET_SIGNING_CERTIFICATES)
            } else {
                @Suppress("DEPRECATION")
                packageManager.getPackageInfo(packageName, PackageManager.GET_SIGNATURES)
            }

            val signatures = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                packageInfo.signingInfo?.apkContentsSigners
            } else {
                @Suppress("DEPRECATION")
                packageInfo.signatures
            }

            signatures?.forEach { sig ->
                val md = MessageDigest.getInstance("SHA-256")
                md.update(sig.toByteArray())
                val digest = md.digest()
                
                // ✅ FORMAT HEX HURUF KECIL (Sesuai file yang sudah work)
                val currentSignature = digest.joinToString("") { 
                    String.format("%02x", it)
                }

                if (ORIGINAL_SIGNATURE.trim().equals(currentSignature, ignoreCase = true)) {
                    return true
                }
            }
        } catch (e: Exception) {
            return false
        }
        return false
    }

    /**
     * Deteksi tambahan untuk menangkal MT Manager & NP Manager
     */
    private fun isModifiedByTool(): Boolean {
        // Cek residu file yang sering ditinggalkan alat modifikasi
        val suspiciousFiles = listOf("assets/pms", "assets/mg.pms", "assets/mt.pms", "assets/np.pms")
        for (filePath in suspiciousFiles) {
            try {
                assets.open(filePath).use { it.close() }
                return true 
            } catch (_: Exception) {}
        }

        // Cek class yang sering disuntikkan oleh MT/NP Manager
        val suspiciousClasses = listOf(
            "com.mt.mt_pms.PmsHook",
            "bin.mt.plus.Main",
            "com.np.pms.PmsHook"
        )
        for (className in suspiciousClasses) {
            try {
                Class.forName(className)
                return true
            } catch (_: Exception) {}
        }
        return false
    }

    private fun isProxyOrVpnActive(): Boolean {
        val proxyAddress = System.getProperty("http.proxyHost") ?: System.getProperty("https.proxyHost")
        val proxyPort = System.getProperty("http.proxyPort") ?: System.getProperty("https.proxyPort")
        if (!proxyAddress.isNullOrBlank() && !proxyPort.isNullOrBlank()) return true

        try {
            val httpProxy = Settings.Global.getString(contentResolver, Settings.Global.HTTP_PROXY)
            if (!httpProxy.isNullOrBlank() && httpProxy != ":0") return true
        } catch (e: Exception) {}

        val connectivityManager = getSystemService(CONNECTIVITY_SERVICE) as ConnectivityManager
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            val activeNetwork = connectivityManager.activeNetwork ?: return false
            val capabilities = connectivityManager.getNetworkCapabilities(activeNetwork) ?: return false
            if (capabilities.hasTransport(NetworkCapabilities.TRANSPORT_VPN)) return true
        }
        return false
    }

    private fun performSilentKill() {
        clearAllCache()
        Process.killProcess(Process.myPid())
        exitProcess(0)
    }

    private fun clearAllCache() {
        try {
            cacheDir?.deleteRecursively()
            externalCacheDir?.deleteRecursively()
            val tempDir = File(filesDir, "temp")
            if (tempDir.exists()) tempDir.deleteRecursively()
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    override fun attachBaseContext(base: Context?) {
        super.attachBaseContext(base)
        context = base
        AcraApplication.context = context
    }

    override fun newImageLoader(context: PlatformContext): ImageLoader {
        return buildImageLoader(applicationContext)
    }

    companion object {
        var exceptionHandler: ExceptionHandler? = null

        tailrec fun Context.getActivity(): Activity? {
            return when (this) {
                is Activity -> this
                is ContextWrapper -> baseContext.getActivity()
                else -> null
            }
        }

        private var _context: WeakReference<Context>? = null
        var context
            get() = _context?.get()
            private set(value) {
                _context = WeakReference(value)
                setContext(WeakReference(value))
            }

        fun <T : Any> getKeyClass(path: String, valueType: Class<T>): T? = context?.getKey(path, valueType)
        fun <T : Any> setKeyClass(path: String, value: T) { context?.setKey(path, value) }
        fun removeKeys(folder: String): Int? = context?.removeKeys(folder)
        fun <T> setKey(path: String, value: T) { context?.setKey(path, value) }
        fun <T> setKey(folder: String, path: String, value: T) { context?.setKey(folder, path, value) }
        inline fun <reified T : Any> getKey(path: String, defVal: T?): T? = context?.getKey(path, defVal)
        inline fun <reified T : Any> getKey(path: String): T? = context?.getKey(path)
        inline fun <reified T : Any> getKey(folder: String, path: String): T? = context?.getKey(folder, path)
        inline fun <reified T : Any> getKey(folder: String, path: String, defVal: T?): T? = context?.getKey(folder, path, defVal)
        fun getKeys(folder: String): List<String>? = context?.getKeys(folder)
        fun removeKey(folder: String, path: String) { context?.removeKey(folder, path) }
        fun removeKey(path: String) { context?.removeKey(path) }

        fun openBrowser(url: String, fallbackWebView: Boolean = false, fragment: Fragment? = null) {
            context?.openBrowser(url, fallbackWebView, fragment)
        }

        fun openBrowser(url: String, activity: FragmentActivity?) {
            openBrowser(
                url,
                isLayout(TV or EMULATOR),
                activity?.supportFragmentManager?.fragments?.lastOrNull()
            )
        }
    }
}
