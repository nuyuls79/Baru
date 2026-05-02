#include <jni.h>
#include <string>
#include <cstring>
#include <android/log.h>
#include <thread>
#include <chrono>
#include <stdlib.h>

// ==================== UTILITY: BASE64 DECODE ====================
// Digunakan untuk menyembunyikan URL Repo agar tidak terbaca sebagai teks biasa
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

// ==================== SECURITY CORE: SIGNATURE VALIDATION ====================
// Hash SHA-256 dalam format huruf kecil tanpa pemisah sesuai data Termux Anda
static const char* EXPECTED_SIGNATURE = "b115983ab9dffa173ee350fee7a6eef515cbb16d0d06c4054579cdc6487e68fc";

extern "C" JNIEXPORT jboolean JNICALL
Java_com_lagradost_cloudstream3_CloudStreamApp_isSignatureValidNative(JNIEnv* env, jobject thiz, jobject context) {
    jclass contextClass = env->GetObjectClass(context);
    jmethodID getPackageManager = env->GetMethodID(contextClass, "getPackageManager", "()Landroid/content/pm/PackageManager;");
    jobject packageManager = env->CallObjectMethod(context, getPackageManager);

    jmethodID getPackageName = env->GetMethodID(contextClass, "getPackageName", "()Ljava/lang/String;");
    jstring packageName = (jstring)env->CallObjectMethod(context, getPackageName);

    jclass pmClass = env->GetObjectClass(packageManager);
    jmethodID getPackageInfo = env->GetMethodID(pmClass, "getPackageInfo", "(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;");
    
    // 0x08000000 = GET_SIGNING_CERTIFICATES (Wajib untuk Android 9+)
    jobject packageInfo = env->CallObjectMethod(packageManager, getPackageInfo, packageName, 0x08000000);
    if (packageInfo == nullptr) return JNI_FALSE;

    jclass piClass = env->GetObjectClass(packageInfo);
    jfieldID siField = env->GetFieldID(piClass, "signingInfo", "Landroid/content/pm/SigningInfo;");
    jobject signingInfo = env->GetObjectField(packageInfo, siField);
    
    jclass siClass = env->GetObjectClass(signingInfo);
    jmethodID getSigners = env->GetMethodID(siClass, "getApkContentsSigners", "()[Landroid/content/pm/Signature;");
    jobjectArray signatures = (jobjectArray)env->CallObjectMethod(signingInfo, getSigners);

    jobject sig = env->GetObjectArrayElement(signatures, 0);
    jclass sigClass = env->GetObjectClass(sig);
    jmethodID toByteAddr = env->GetMethodID(sigClass, "toByteArray", "()[B");
    jbyteArray sigBytes = (jbyteArray)env->CallObjectMethod(sig, toByteAddr);

    // Proses hashing SHA-256 melalui JNI
    jclass mdClass = env->FindClass("java/security/MessageDigest");
    jmethodID getInstance = env->GetStaticMethodID(mdClass, "getInstance", "(Ljava/lang/String;)Ljava/security/MessageDigest;");
    jobject md = env->CallStaticObjectMethod(mdClass, getInstance, env->NewStringUTF("SHA-256"));
    jmethodID update = env->GetMethodID(mdClass, "update", "([B)V");
    env->CallVoidMethod(md, update, sigBytes);
    jmethodID digest = env->GetMethodID(mdClass, "digest", "()[B");
    jbyteArray hashBytes = (jbyteArray)env->CallObjectMethod(md, digest);

    // Konversi byte array ke Hex String (Kecil)
    jint hashLen = env->GetArrayLength(hashBytes);
    jbyte* buffer = env->GetByteArrayElements(hashBytes, nullptr);
    char hexResult[65];
    for (int i = 0; i < hashLen; i++) {
        sprintf(&hexResult[i * 2], "%02x", (unsigned char)buffer[i]);
    }
    hexResult[64] = '\0';
    env->ReleaseByteArrayElements(hashBytes, buffer, JNI_ABORT);

    return (strcmp(hexResult, EXPECTED_SIGNATURE) == 0) ? JNI_TRUE : JNI_FALSE;
}

// ==================== SECURITY CORE: PROXY/VPN DETECTION ====================
static bool isProxyOrVpnActive(JNIEnv* env, jobject context) {
    jclass settingsClass = env->FindClass("android/provider/Settings$Global");
    jmethodID getString = env->GetStaticMethodID(settingsClass, "getString",
        "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;");
    jclass contextClass = env->GetObjectClass(context);
    jmethodID getContentResolver = env->GetMethodID(contextClass, "getContentResolver", "()Landroid/content/ContentResolver;");
    jobject contentResolver = env->CallObjectMethod(context, getContentResolver);
    
    jstring httpProxy = (jstring)env->CallStaticObjectMethod(settingsClass, getString,
        contentResolver, env->NewStringUTF("http_proxy"));

    if (httpProxy != nullptr) {
        const char* proxyStr = env->GetStringUTFChars(httpProxy, nullptr);
        bool proxyActive = (strlen(proxyStr) > 0 && strcmp(proxyStr, ":0") != 0);
        env->ReleaseStringUTFChars(httpProxy, proxyStr);
        if (proxyActive) return true;
    }

    jclass connectivityClass = env->FindClass("android/net/ConnectivityManager");
    jmethodID getSystemService = env->GetMethodID(contextClass, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jstring connectivityService = env->NewStringUTF("connectivity");
    jobject connectivityManager = env->CallObjectMethod(context, getSystemService, connectivityService);

    if (connectivityManager != nullptr) {
        jmethodID getActiveNetwork = env->GetMethodID(connectivityClass, "getActiveNetwork", "()Landroid/net/Network;");
        jobject activeNetwork = env->CallObjectMethod(connectivityManager, getActiveNetwork);
        if (activeNetwork != nullptr) {
            jmethodID getNetworkCapabilities = env->GetMethodID(connectivityClass, "getNetworkCapabilities", "(Landroid/net/Network;)Landroid/net/NetworkCapabilities;");
            jobject capabilities = env->CallObjectMethod(connectivityManager, getNetworkCapabilities, activeNetwork);
            if (capabilities != nullptr) {
                jclass netCapClass = env->FindClass("android/net/NetworkCapabilities");
                jmethodID hasTransport = env->GetMethodID(netCapClass, "hasTransport", "(I)Z");
                if (env->CallBooleanMethod(capabilities, hasTransport, 4)) return true; // 4 = TRANSPORT_VPN
            }
        }
    }
    return false;
}

// ==================== ACTIONS: CACHE MANAGEMENT ====================
static void clearAllCache(JNIEnv* env, jobject context) {
    jclass contextClass = env->GetObjectClass(context);
    jmethodID getCacheDir = env->GetMethodID(contextClass, "getCacheDir", "()Ljava/io/File;");
    jobject cacheDir = env->CallObjectMethod(context, getCacheDir);
    if (cacheDir != nullptr) {
        jclass fileClass = env->FindClass("java/io/File");
        jmethodID deleteRec = env->GetMethodID(fileClass, "deleteRecursively", "()Z");
        env->CallBooleanMethod(cacheDir, deleteRec);
    }
}

// ==================== JNI EXPORTS ====================
extern "C" {

// Proteksi URL Repo (Tetap dipertahankan fungsinya)
static const char* ENC_PREMIUM = "aHR0cHM6Ly9yYXcuZ2l0aHVidXNlcmNvbnRlbnQuY29tL251eXVsczc5L1N0cmVhbVBsYXktRnJlZS9yZWZzL2hlYWRzL2J1aWxkcy9yZXBvLmpzb24=";
static const char* ENC_FREE    = "aHR0cHM6Ly9yYXcuZ2l0aHVidXNlcmNvbnRlbnQuY29tL21pY2hhdDg4L1JlcG9fR3JhdGlzL3JlZnMvaGVhZHMvYnVpbGRzL3JlcG8uanNvbg==";

JNIEXPORT jstring JNICALL
Java_com_lagradost_cloudstream3_utils_RepoProtector_nativeGetPremiumRepoUrl(JNIEnv* env, jclass) {
    return env->NewStringUTF(base64_decode(ENC_PREMIUM).c_str());
}

JNIEXPORT jstring JNICALL
Java_com_lagradost_cloudstream3_utils_RepoProtector_nativeGetFreeRepoUrl(JNIEnv* env, jclass) {
    return env->NewStringUTF(base64_decode(ENC_FREE).c_str());
}

// Fungsi pengecekan sekaligus pemblokiran (Integrasi Signature & Proxy)
JNIEXPORT void JNICALL
Java_com_lagradost_cloudstream3_CloudStreamApp_checkAndBlock(JNIEnv* env, jobject thiz) {
    if (!Java_com_lagradost_cloudstream3_CloudStreamApp_isSignatureValidNative(env, thiz, thiz)) {
        clearAllCache(env, thiz);
        exit(0);
    }
    if (isProxyOrVpnActive(env, thiz)) {
        clearAllCache(env, thiz);
        exit(0);
    }
}

// Monitoring Real-time di background thread (3 detik sekali)
JNIEXPORT void JNICALL
Java_com_lagradost_cloudstream3_CloudStreamApp_startNativeMonitor(JNIEnv* env, jobject thiz) {
    JavaVM* jvm;
    env->GetJavaVM(&jvm);
    jobject globalContext = env->NewGlobalRef(thiz);

    std::thread([jvm, globalContext]() {
        JNIEnv* mEnv;
        jvm->AttachCurrentThread(&mEnv, nullptr);
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            if (isProxyOrVpnActive(mEnv, globalContext)) {
                clearAllCache(mEnv, globalContext);
                exit(0);
            }
        }
    }).detach();
}

} // extern "C"
