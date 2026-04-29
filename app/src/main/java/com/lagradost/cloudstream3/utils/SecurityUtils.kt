package com.lagradost.cloudstream3.utils

import android.content.Context
import android.net.ConnectivityManager
import android.net.NetworkCapabilities

object SecurityUtils {

    /**
     * Detect VPN (HTTP Canary biasanya menggunakan VPN)
     */
    fun isVpnActive(context: Context): Boolean {
        return try {

            val cm = context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
            val networks = cm.allNetworks

            for (network in networks) {
                val caps = cm.getNetworkCapabilities(network)

                if (caps?.hasTransport(NetworkCapabilities.TRANSPORT_VPN) == true) {
                    return true
                }
            }

            false

        } catch (e: Exception) {
            false
        }
    }

}
