package com.lagradost.cloudstream3

import android.app.Activity
import android.app.Dialog
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.*
import androidx.appcompat.app.AlertDialog
import com.lagradost.cloudstream3.ui.settings.Globals.EMULATOR
import com.lagradost.cloudstream3.ui.settings.Globals.TV
import com.lagradost.cloudstream3.ui.settings.Globals.isLayout
import com.lagradost.cloudstream3.utils.ImageLoader.loadImage
import com.lagradost.cloudstream3.utils.UIHelper.toPx

object PremiumDialogManager {

fun showPremiumUnlockDialog(activity: Activity) {

    val isTv = isLayout(TV or EMULATOR)

    val gradient = android.graphics.drawable.GradientDrawable(
        android.graphics.drawable.GradientDrawable.Orientation.TOP_BOTTOM,
        intArrayOf(
            android.graphics.Color.parseColor("#141414"),
            android.graphics.Color.parseColor("#000000")
        )
    )
    gradient.cornerRadius = 16f.toPx

    val mainLayout = LinearLayout(activity).apply {
        orientation = if (isTv) LinearLayout.HORIZONTAL else LinearLayout.VERTICAL
        setPadding(30, if (isTv) 30 else 60, 30, if (isTv) 30 else 60)
        background = gradient
        gravity = Gravity.CENTER
    }

    val icon = TextView(activity).apply {
        text = "🍿"
        textSize = if (isTv) 40f else 50f
        gravity = Gravity.CENTER
    }

    val title = TextView(activity).apply {
        text = "PREMIUM ACCESS"
        textSize = if (isTv) 20f else 24f
        setTextColor(android.graphics.Color.WHITE)
        gravity = Gravity.CENTER
    }

    val subTitle = TextView(activity).apply {
        text = "Fitur ini terkunci.\nSilakan hubungi admin untuk berlangganan."
        textSize = 14f
        setTextColor(android.graphics.Color.parseColor("#B3B3B3"))
        gravity = Gravity.CENTER
        setPadding(0, 0, 0, if (isTv) 15 else 30)
    }

    // DEVICE ID
    val deviceIdVal = PremiumManager.getDeviceId(activity)

    val idBackground = android.graphics.drawable.GradientDrawable().apply {
        setColor(android.graphics.Color.parseColor("#222222"))
        cornerRadius = 8f.toPx
    }

    val idContainer = LinearLayout(activity).apply {
        orientation = LinearLayout.VERTICAL
        setPadding(20, 20, 20, 20)
        background = idBackground
        layoutParams = LinearLayout.LayoutParams(-1, -2).apply {
            setMargins(30.toPx, 0, 30.toPx, if (isTv) 15 else 30)
        }
    }

    val idLabel = TextView(activity).apply {
        text = "DEVICE ID ANDA (Tap to copy):"
        textSize = 12f
        setTextColor(android.graphics.Color.parseColor("#B3B3B3"))
        gravity = Gravity.CENTER
    }

    val idValueRow = LinearLayout(activity).apply {
        orientation = LinearLayout.HORIZONTAL
        gravity = Gravity.CENTER
    }

    val idValue = TextView(activity).apply {
        text = deviceIdVal
        textSize = if (isTv) 20f else 24f
        setTextColor(android.graphics.Color.WHITE)
    }

    val copyIcon = TextView(activity).apply {
        text = "⎘"
        setTextColor(android.graphics.Color.parseColor("#E50914"))
    }

    idValueRow.addView(idValue)
    idValueRow.addView(copyIcon)

    idContainer.addView(idLabel)
    idContainer.addView(idValueRow)

    idContainer.setOnClickListener {
        val clipboard =
            activity.getSystemService(Context.CLIPBOARD_SERVICE) as android.content.ClipboardManager
        val clip = android.content.ClipData.newPlainText("Device ID", deviceIdVal)
        clipboard.setPrimaryClip(clip)
        Toast.makeText(activity, "ID Disalin!", Toast.LENGTH_SHORT).show()
    }

    // INPUT CODE
    val inputCode = EditText(activity).apply {
        hint = "Masukkan KODE di sini"
        setHintTextColor(android.graphics.Color.parseColor("#8C8C8C"))
        setTextColor(android.graphics.Color.WHITE)
        gravity = Gravity.CENTER
        setPadding(20, 30, 20, 30)
    }

    // BUTTON UNLOCK
    val btnUnlock = Button(activity).apply {
        text = "UNLOCK NOW"
        setBackgroundColor(android.graphics.Color.parseColor("#E50914"))
        setTextColor(android.graphics.Color.WHITE)
    }

    // TELEGRAM BUTTON
    val btnAdminRow = LinearLayout(activity).apply {
        orientation = LinearLayout.HORIZONTAL
        gravity = Gravity.CENTER

        setOnClickListener {
            try {
                val intent =
                    Intent(Intent.ACTION_VIEW, Uri.parse("https://t.me/wadadah12"))
                activity.startActivity(intent)
            } catch (e: Exception) {
                Toast.makeText(activity, "Telegram tidak ditemukan", Toast.LENGTH_SHORT)
                    .show()
            }
        }
    }

    val textAdmin = TextView(activity).apply {
        text = "HUBUNGI ADMIN "
        setTextColor(android.graphics.Color.WHITE)
    }

    val iconAdmin = ImageView(activity).apply {
        layoutParams = LinearLayout.LayoutParams(18.toPx, 18.toPx)
        loadImage("https://raw.githubusercontent.com/michat88/AdiXtream/master/asset/telegram.png")
    }

    btnAdminRow.addView(textAdmin)
    btnAdminRow.addView(iconAdmin)

    // UNLOCK LOGIC
    btnUnlock.setOnClickListener {

        val code = inputCode.text.toString().trim().uppercase()

        val isSuccess =
            PremiumManager.activatePremiumWithCode(activity, code, deviceIdVal)

        if (isSuccess) {

            (btnUnlock.tag as? Dialog)?.dismiss()

            val expiryDate = PremiumManager.getExpiryDateString(activity)

            AlertDialog.Builder(activity)
                .setTitle("📢 CATATAN PEMBAHARUAN")
                .setCancelable(false)
                .setMessage(
                    """

Selamat datang di XStream

XStream adalah kumpulan ekstensi XStream yang digabungkan agar lebih fokus pada konten Indonesia.

🚀 Pembaruan Terbaru:
✔ Sinkronisasi dengan source terbaru
✔ Perbaikan bug
✔ Optimalisasi performa
✔ Peningkatan kecepatan loading
✔ Penyempurnaan sistem plugin

📅 Masa Aktif Premium:
$expiryDate
""".trimIndent()
)
.setPositiveButton("OK") { _, _ ->

                    val packageManager = activity.packageManager
                    val intent =
                        packageManager.getLaunchIntentForPackage(activity.packageName)

                    val componentName = intent?.component
                    val mainIntent =
                        Intent.makeRestartActivityTask(componentName)

                    activity.startActivity(mainIntent)

                    Runtime.getRuntime().exit(0)
                }
                .show()

        } else {

            Toast.makeText(
                activity,
                "⛔ Kode Salah / Sudah Expired!",
                Toast.LENGTH_SHORT
            ).show()
        }
    }

    mainLayout.addView(icon)
    mainLayout.addView(title)
    mainLayout.addView(subTitle)
    mainLayout.addView(idContainer)
    mainLayout.addView(inputCode)
    mainLayout.addView(btnUnlock)
    mainLayout.addView(btnAdminRow)

    val dialog =
        Dialog(activity, android.R.style.Theme_DeviceDefault_Dialog_NoActionBar)

    dialog.window?.setBackgroundDrawable(
        android.graphics.drawable.ColorDrawable(
            android.graphics.Color.TRANSPARENT
        )
    )

    dialog.setContentView(mainLayout)

    btnUnlock.tag = dialog

    dialog.show()
}

}
