# ============================================================
# ATURAN R8 UNTUK ADIXTREAM (PLUGIN-SAFE)
# ============================================================

# --- Dasar ---
-keepattributes Signature
-keepattributes *Annotation*

# --- Native JNI ---
-keep class com.lagradost.cloudstream3.utils.RepoProtector {
    native <methods>;
    public static *;
}

# --- Model JSON (Jackson) ---
-keep class com.lagradost.cloudstream3.ui.settings.SettingsGeneral$CustomSite { <fields>; }
-keepclassmembers class * {
    @com.fasterxml.jackson.annotation.JsonProperty <fields>;
}

# --- View/Data Binding ---
-keep class * implements androidx.viewbinding.ViewBinding {
    public static * bind(android.view.View);
    public static * inflate(android.view.LayoutInflater);
}

# --- PENTING: Pertahankan SEMUA class dalam package utama ---
-keep class com.lagradost.cloudstream3.** { *; }

# --- Plugin system (jangan disentuh) ---
-keep class com.lagradost.cloudstream3.plugins.** { *; }
-keep class com.lagradost.cloudstream3.APIHolder { *; }
-keep class com.lagradost.cloudstream3.MainAPI { *; }
-keep class com.lagradost.cloudstream3.SearchResponse { *; }
-keep class com.lagradost.cloudstream3.TvType { *; }
-keep class com.lagradost.cloudstream3.DubStatus { *; }

# --- Library pihak ketiga yang mungkin diakses lewat refleksi ---
-keep class org.jsoup.** { *; }
-keep class org.mozilla.javascript.** { *; }

# --- Abaikan class yang hilang (java.beans, javax.script, dll.) ---
-dontwarn com.google.re2j.**
-dontwarn java.beans.**
-dontwarn javax.script.**
-dontwarn okhttp3.**
-dontwarn okio.**

# --- Jangan hapus class yang dipanggil dengan Class.forName ---
-keepclassmembers class * {
    *** forName(...);
}

# --- Opsional: Abaikan warning agar build tidak gagal ---
-ignorewarnings
