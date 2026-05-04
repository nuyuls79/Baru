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
# ATURAN R8 UNTUK ADIXTREAM (PLUGIN-SAFE)
# ============================================================

-keepattributes Signature
-keepattributes *Annotation

-keep class com.lagradost.cloudstream3.utils.RepoProtector {
    native <methods>;
    public static *;
}

-keep class com.lagradost.cloudstream3.** { *; }
-keep class org.jsoup.** { *; }
-keep class org.mozilla.javascript.** { *; }

-keepclassmembers class * {
    @com.fasterxml.jackson.annotation.JsonProperty <fields>;
}

-dontwarn com.google.re2j.**
-dontwarn java.beans.**
-dontwarn javax.script.**
-dontwarn okhttp3.**
-dontwarn okio.**
-ignorewarnings