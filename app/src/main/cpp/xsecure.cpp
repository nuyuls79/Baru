#include <jni.h>
#include <string>
#include <cstring>
#include <android/log.h>
#include <thread>
#include <chrono>
#include <stdlib.h>

static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

static inline bool is_base64(unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string base64_decode(const std::string &encoded_string) {
    int in_len = encoded_string.size();
    int i = 0, j = 0, in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::string ret;

    while (in_len-- && ( encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
        char_array_4[i++] = encoded_string[in_]; in_++;
        if (i == 4) {
            for (i = 0; i < 4; i++)
                char_array_4[i] = base64_chars.find(char_array_4[i]);
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
            for (i = 0; i < 3; i++)
                ret += char_array_3[i];
            i = 0;
        }
    }
    if (i) {
        for (j = i; j < 4; j++)
            char_array_4[j] = 0;
        for (j = 0; j < 4; j++)
            char_array_4[j] = base64_chars.find(char_array_4[j]);
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
        for (j = 0; j < i - 1; j++) ret += char_array_3[j];
    }
    return ret;
}

// ==================== ANTI-TAMPER: CEK SIGNATURE APK ====================
static bool isApkSignatureValid(JNIEnv* env, jobject context) {
    // Implementasi pengecekan signature APK
    // Jika signature tidak cocok, return false
    // Untuk sementara kita anggap valid (bisa diimplementasikan nanti)
    return true;
}
// ======================================================================

// ==================== FUNGSI DETEKSI PROXY/VPN ====================
static bool isProxyOrVpnActive(JNIEnv* env, jobject context) {
    // 1. Cek proxy dari Settings.Global
    jclass settingsClass = env->FindClass("android/provider/Settings$Global");
    jmethodID getString = env->GetStaticMethodID(settingsClass, "getString",
        "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;");
    jclass contextClass = env->GetObjectClass(context);
    jmethodID getContentResolver = env->GetMethodID(contextClass, "getContentResolver",
        "()Landroid/content/ContentResolver;");
    jobject contentResolver = env->CallObjectMethod(context, getContentResolver);
    jstring httpProxy = (jstring)env->CallStaticObjectMethod(settingsClass, getString,
        contentResolver, env->NewStringUTF("http_proxy"));

    if (httpProxy != nullptr) {
        const char* proxyStr = env->GetStringUTFChars(httpProxy, nullptr);
        bool proxyActive = (strlen(proxyStr) > 0 && strcmp(proxyStr, ":0") != 0);
        env->ReleaseStringUTFChars(httpProxy, proxyStr);
        if (proxyActive) return true;
    }

    // 2. Cek VPN dari ConnectivityManager
    jclass connectivityClass = env->FindClass("android/net/ConnectivityManager");
    jmethodID getSystemService = env->GetMethodID(contextClass, "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;");
    jstring connectivityService = env->NewStringUTF("connectivity");
    jobject connectivityManager = env->CallObjectMethod(context, getSystemService,
        connectivityService);
    env->DeleteLocalRef(connectivityService);

    if (connectivityManager != nullptr) {
        jmethodID getActiveNetwork = env->GetMethodID(connectivityClass, "getActiveNetwork",
            "()Landroid/net/Network;");
        jobject activeNetwork = env->CallObjectMethod(connectivityManager, getActiveNetwork);
        if (activeNetwork != nullptr) {
            jmethodID getNetworkCapabilities = env->GetMethodID(connectivityClass,
                "getNetworkCapabilities",
                "(Landroid/net/Network;)Landroid/net/NetworkCapabilities;");
            jobject capabilities = env->CallObjectMethod(connectivityManager,
                getNetworkCapabilities, activeNetwork);
            if (capabilities != nullptr) {
                jclass networkCapabilitiesClass = env->FindClass("android/net/NetworkCapabilities");
                jmethodID hasTransport = env->GetMethodID(networkCapabilitiesClass, "hasTransport", "(I)Z");
                jboolean hasVpn = env->CallBooleanMethod(capabilities, hasTransport, 4);
                if (hasVpn) return true;
            }
        }
    }
    return false;
}
// ===================================================================

// ==================== CLEAR CACHE ====================
static void clearAllCache(JNIEnv* env, jobject context) {
    jclass contextClass = env->GetObjectClass(context);
    jmethodID getCacheDir = env->GetMethodID(contextClass, "getCacheDir", "()Ljava/io/File;");
    jobject cacheDir = env->CallObjectMethod(context, getCacheDir);
    
    if (cacheDir != nullptr) {
        jclass fileClass = env->FindClass("java/io/File");
        jmethodID deleteRecursively = env->GetMethodID(fileClass, "deleteRecursively", "()Z");
        env->CallBooleanMethod(cacheDir, deleteRecursively);
    }
    
    // External cache dir (butuh permission, bisa diabaikan)
    jmethodID getExternalCacheDir = env->GetMethodID(contextClass, "getExternalCacheDir", "()Ljava/io/File;");
    jobject externalCacheDir = env->CallObjectMethod(context, getExternalCacheDir);
    if (externalCacheDir != nullptr) {
        jclass fileClass = env->FindClass("java/io/File");
        jmethodID deleteRecursively = env->GetMethodID(fileClass, "deleteRecursively", "()Z");
        env->CallBooleanMethod(externalCacheDir, deleteRecursively);
    }
}
// ===================================================

// ==================== THREAD UNTUK MONITORING REAL-TIME ====================
static void startMonitoringThread(JNIEnv* env, jobject context) {
    // Detach agar thread bisa berjalan mandiri
    JavaVM* jvm;
    env->GetJavaVM(&jvm);
    
    std::thread([jvm, context]() {
        JNIEnv* monitorEnv;
        jvm->AttachCurrentThread(&monitorEnv, nullptr);
        
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            
            if (isProxyOrVpnActive(monitorEnv, context)) {
                clearAllCache(monitorEnv, context);
                jvm->DetachCurrentThread();
                exit(0);
            }
        }
    }).detach();
}
// =========================================================================

extern "C" {

static const char* ENCODED_PREMIUM_REPO = "aHR0cHM6Ly9yYXcuZ2l0aHVidXNlcmNvbnRlbnQuY29tL251eXVsczc5L1N0cmVhbVBsYXktRnJlZS9yZWZzL2hlYWRzL2J1aWxkcy9yZXBvLmpzb24=";
static const char* ENCODED_FREE_REPO    = "aHR0cHM6Ly9yYXcuZ2l0aHVidXNlcmNvbnRlbnQuY29tL21pY2hhdDg4L1JlcG9fR3JhdGlzL3JlZnMvaGVhZHMvYnVpbGRzL3JlcG8uanNvbg==";

JNIEXPORT jstring JNICALL
Java_com_lagradost_cloudstream3_utils_RepoProtector_nativeGetPremiumRepoUrl(JNIEnv* env, jclass) {
    std::string decoded = base64_decode(ENCODED_PREMIUM_REPO);
    return env->NewStringUTF(decoded.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_lagradost_cloudstream3_utils_RepoProtector_nativeGetFreeRepoUrl(JNIEnv* env, jclass) {
    std::string decoded = base64_decode(ENCODED_FREE_REPO);
    return env->NewStringUTF(decoded.c_str());
}

// ==================== INITIAL CHECK (DIPANGGIL SEKALI DI AWAL) ====================
JNIEXPORT void JNICALL
Java_com_lagradost_cloudstream3_CloudStreamApp_checkAndBlock(JNIEnv* env, jobject thiz) {
    if (!isApkSignatureValid(env, thiz)) {
        clearAllCache(env, thiz);
        exit(0);
    }
    if (isProxyOrVpnActive(env, thiz)) {
        clearAllCache(env, thiz);
        exit(0);
    }
}
// ==============================================================================

// ==================== START NATIVE MONITORING THREAD ====================
JNIEXPORT void JNICALL
Java_com_lagradost_cloudstream3_CloudStreamApp_startNativeMonitor(JNIEnv* env, jobject thiz) {
    startMonitoringThread(env, thiz);
}
// =======================================================================

} // extern "C"