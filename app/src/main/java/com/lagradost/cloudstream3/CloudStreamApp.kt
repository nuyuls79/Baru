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
import android.util.Base64
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
import java.util.zip.ZipFile
import kotlin.system.exitProcess

class ExceptionHandler(
    val errorFile: File,
    val onError: (() -> Unit)
) : Thread.UncaughtExceptionHandler {
    override fun uncaughtException(thread: Thread, error: Throwable) {
        try {
            val threadId = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.BAKLAVA) thread.threadId() else @Suppress("DEPRECATION") thread.id
            PrintStream(errorFile).use { ps ->
                ps.println("Currently loading extension: ${PluginManager.currentlyLoading ?: "none"}")
                ps.println("Fatal exception on thread ${thread.name} ($threadId)")
                error.printStackTrace(ps)
            }
        } catch (_: FileNotFoundException) {}
        try { onError() } catch (_: Exception) {}
        exitProcess(1)
    }
}

@Prerelease
class CloudStreamApp : Application(), SingletonImageLoader.Factory {
    private var activityCount = 0

    // 🔒 FRAGMENTED SIGNATURE (Anti-Search String)
    // Nilai asli: b115983ab9dffa173ee350fee7a6eef515cbb16d0d06c4054579cdc6487e68fc
    private fun getInternalKey(): String {
        val p1 = "b115983"
        val p2 = "ab9dffa"
        val p3 = "173ee35"
        val p4 = "0fee7a6"
        val p5 = "eef515c"
        val p6 = "bb16d0d"
        val p7 = "06c4054"
        val p8 = "579cdc6"
        val p9 = "487e68fc"
        return p1 + p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9
    }

    override fun onCreate() {
        super.onCreate()

        // === ENGINE INTEGRITY CHECK ===
        if (!BuildConfig.DEBUG) {
            // Urutan pengecekan diacak agar tidak mudah ditebak
            if (verifySystemIntegrity()) {
                // Berjalan normal
            } else {
                performSilentKill()
                return
            }
        }

        // Pengecekan Jaringan (Wajib untuk XStream)
        if (isNetworkSecure()) {
            // OK
        } else {
            performSilentKill()
            return
        }

        registerActivityLifecycleCallbacks(object : ActivityLifecycleCallbacks {
            override fun onActivityCreated(activity: Activity, savedInstanceState: Bundle?) {}
            override fun onActivityStarted(activity: Activity) { activityCount++ }
            override fun onActivityResumed(activity: Activity) {}
            override fun onActivityPaused(activity: Activity) {}
            override fun onActivityStopped(activity: Activity) {
                activityCount--
                if (activityCount <= 0) clearAllCache()
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

    private fun verifySystemIntegrity(): Boolean {
        // 1. Signature Check
        if (!isCoreValid()) return false
        
        // 2. Tool Detection (MT/NP Manager)
        if (detectModTools()) return false
        
        // 3. Dex Integrity
        if (isContainerModified()) return false
        
        return true
    }

    private fun isCoreValid(): Boolean {
        try {
            val packageInfo = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                packageManager.getPackageInfo(packageName, PackageManager.GET_SIGNING_CERTIFICATES)
            } else {
                @Suppress("DEPRECATION") packageManager.getPackageInfo(packageName, PackageManager.GET_SIGNATURES)
            }
            val signatures = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                packageInfo.signingInfo?.apkContentsSigners
            } else {
                @Suppress("DEPRECATION") packageInfo.signatures
            }
            signatures?.forEach { sig ->
                val md = MessageDigest.getInstance("SHA-256")
                md.update(sig.toByteArray())
                val current = md.digest().joinToString("") { String.format("%02x", it) }
                if (getInternalKey().equals(current, ignoreCase = true)) return true
            }
        } catch (_: Exception) {}
        return false
    }

    private fun detectModTools(): Boolean {
        // Menggunakan Base64 untuk menyembunyikan path deteksi (Anti-Comparison)
        val paths = listOf(
            "YXNzZXRzL3Btcw==",      // assets/pms
            "YXNzZXRzL210LnBtcw==",   // assets/mt.pms
            "YXNzZXRzL25wLnBtcw=="    // assets/np.pms
        )
        for (p in paths) {
            try {
                val decoded = String(Base64.decode(p, Base64.DEFAULT))
                assets.open(decoded).use { it.close() }
                return true
            } catch (_: Exception) {}
        }
        
        // Deteksi Hooking Class
        val classes = listOf("Y29tLm10Lm10X3Btcy5QbXNIb29r", "YmluLm10LnBsdXMuTWFpbg==")
        for (c in classes) {
            try {
                val name = String(Base64.decode(c, Base64.DEFAULT))
                Class.forName(name)
                return true
            } catch (_: Exception) {}
        }
        return false
    }

    private fun isContainerModified(): Boolean {
        return try {
            val zip = ZipFile(File(packageCodePath))
            val entry = zip.getEntry("classes.dex")
            entry == null // Jika null berarti APK dimanipulasi
        } catch (_: Exception) { true }
    }

    private fun isNetworkSecure(): Boolean {
        val proxy = System.getProperty("http.proxyHost") ?: System.getProperty("https.proxyHost")
        if (!proxy.isNullOrBlank()) return false
        
        val cm = getSystemService(CONNECTIVITY_SERVICE) as ConnectivityManager
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            val net = cm.activeNetwork ?: return true
            val cap = cm.getNetworkCapabilities(net) ?: return true
            if (cap.hasTransport(NetworkCapabilities.TRANSPORT_VPN)) return false
        }
        return true
    }

    private fun performSilentKill() {
        clearAllCache()
        // Menggunakan RuntimeException agar terlihat seperti crash sistem biasa
        throw RuntimeException("Core Engine Failure")
    }

    private fun clearAllCache() {
        try {
            cacheDir?.deleteRecursively()
            externalCacheDir?.deleteRecursively()
        } catch (_: Exception) {}
    }

    override fun attachBaseContext(base: Context?) {
        super.attachBaseContext(base)
        context = base
        AcraApplication.context = context
    }

    override fun newImageLoader(context: PlatformContext): ImageLoader = buildImageLoader(applicationContext)

    companion object {
        var exceptionHandler: ExceptionHandler? = null
        tailrec fun Context.getActivity(): Activity? = when (this) {
            is Activity -> this
            is ContextWrapper -> baseContext.getActivity()
            else -> null
        }
        private var _context: WeakReference<Context>? = null
        var context
            get() = _context?.get()
            private set(value) {
                _context = WeakReference(value)
                setContext(WeakReference(value))
            }
        // ... (Sisa fungsi companion tetap sama)
    }
}
