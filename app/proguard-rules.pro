# Add project specific ProGuard rules here.
# You can control the set of applied configuration files using the
# proguardFiles setting in build.gradle.kts.
#
# For more details, see
#   http://developer.android.com/guide/developing/tools/proguard.html

# If your project uses WebView with JS, uncomment the following
# and specify the fully qualified class name to the JavaScript interface
# class:
#-keepclassmembers class fqcn.of.javascript.interface.for.webview {
#   public *;
#}

# Uncomment this to preserve the line number information for
# debugging stack traces.
#-keepattributes SourceFile,LineNumberTable

# If you keep the line number information, uncomment this to
# hide the original source file name.
#-renamesourcefileattribute SourceFile

# ============================================================
# ATURAN R8 UNTUK ADIXTREAM
# ============================================================

# === 1. Aturan umum untuk atribut dan anotasi ===
-keepattributes Signature
-keepattributes *Annotation*

# === 2. Lindungi Native JNI (RepoProtector / xsecure) ===
-keep class com.lagradost.cloudstream3.utils.RepoProtector {
    native <methods>;
    public static *;
}

# === 3. Lindungi Model Data JSON (Jackson / Gson) ===
-keep class com.lagradost.cloudstream3.ui.settings.SettingsGeneral$CustomSite { <fields>; }
-keepclassmembers class * {
    @com.fasterxml.jackson.annotation.JsonProperty <fields>;
}

# === 4. Lindungi View Binding & Data Binding ===
-keep class * implements androidx.viewbinding.ViewBinding {
    public static * bind(android.view.View);
    public static * inflate(android.view.LayoutInflater);
}

# === 5. Abaikan class yang tidak ada di Android (java.beans, javax.script, dll.) ===
-dontwarn com.google.re2j.**
-dontwarn java.beans.**
-dontwarn javax.script.**

# === 6. Opsional: Abaikan warning agar build tidak gagal ===
-ignorewarnings

# === 7. Library eksternal ===
-dontwarn okhttp3.**
-dontwarn okio.**
