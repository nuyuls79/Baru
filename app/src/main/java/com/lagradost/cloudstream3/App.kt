package com.lagradost.cloudstream3

import android.app.Application
import com.lagradost.cloudstream3.utils.NativeSecurity

class App : Application() {
    override fun onCreate() {
        super.onCreate()
        if (!NativeSecurity.validate(this)) {
            // Paksa keluar jika signature tidak cocok
            android.os.Process.killProcess(android.os.Process.myPid())
            Runtime.getRuntime().exit(0)
        }
    }
}
