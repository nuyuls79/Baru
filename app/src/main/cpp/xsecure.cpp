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

// ==================== DETEKSI PROXY/VPN ====================
static bool isProxyOrVpnActive(JNIEnv* env, jobject context) {
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

// ==================== DETEKSI ALAT MODIFIKASI (MT/NP Manager) ====================
static bool isModifiedByTool(JNIEnv* env, jobject context) {
    jclass contextClass = env->GetObjectClass(context);
    jmethodID getAssets = env->GetMethodID(contextClass, "getAssets", "()Landroid/content/res/AssetManager;");
    jobject assetManager = env->CallObjectMethod(context, getAssets);
    jclass assetManagerClass = env->GetObjectClass(assetManager);
    jmethodID openMethod = env->GetMethodID(assetManagerClass, "open", "(Ljava/lang/String;)Ljava/io/InputStream;");

    const char* suspiciousFiles[] = {"assets/pms", "assets/mg.pms", "assets/mt.pms"};
    for (int i = 0; i < 3; i++) {
        jstring fileName = env->NewStringUTF(suspiciousFiles[i]);
        jobject inputStream = env->CallObjectMethod(assetManager, openMethod, fileName);
        env->DeleteLocalRef(fileName);
        if (inputStream != nullptr) {
            jclass inputStreamClass = env->GetObjectClass(inputStream);
            jmethodID closeMethod = env->GetMethodID(inputStreamClass, "close", "()V");
            env->CallVoidMethod(inputStream, closeMethod);
            return true;
        }
        // Jika terjadi exception, lanjutkan ke file berikutnya
        env->ExceptionClear();
    }
    return false;
}

// ==================== SIGNATURE CHECK ====================
static bool isSignatureValid(JNIEnv* env, jobject context) {
    jclass contextClass = env->GetObjectClass(context);
    jmethodID getPackageName = env->GetMethodID(contextClass, "getPackageName", "()Ljava/lang/String;");
    jstring packageName = (jstring)env->CallObjectMethod(context, getPackageName);
    const char* packageNameStr = env->GetStringUTFChars(packageName, nullptr);

    jmethodID getPackageManager = env->GetMethodID(contextClass, "getPackageManager", "()Landroid/content/pm/PackageManager;");
    jobject packageManager = env->CallObjectMethod(context, getPackageManager);

    jclass pmClass = env->GetObjectClass(packageManager);
    jmethodID getPackageInfo = env->GetMethodID(pmClass, "getPackageInfo", "(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;");
    
    jint flags = 0x00000001;
    if (env->GetStaticIntField(env->FindClass("android/os/Build$VERSION"), env->GetStaticFieldID(env->FindClass("android/os/Build$VERSION"), "SDK_INT", "I")) >= 28) {
        flags = 0x08000000;
    }
    
    jobject packageInfo = env->CallObjectMethod(packageManager, getPackageInfo, packageName, flags);
    env->ReleaseStringUTFChars(packageName, packageNameStr);
    
    if (packageInfo == nullptr) return false;

    jclass piClass = env->GetObjectClass(packageInfo);
    const char* ORIGINAL_SIGNATURE = "b115983ab9dffa173ee350fee7a6eef515cbb16d0d06c4054579cdc6487e68fc";
    
    if (flags == 0x08000000) {
        jfieldID signingInfoField = env->GetFieldID(piClass, "signingInfo", "Landroid/content/pm/SigningInfo;");
        jobject signingInfo = env->GetObjectField(packageInfo, signingInfoField);
        if (signingInfo == nullptr) return false;
        
        jclass siClass = env->GetObjectClass(signingInfo);
        jmethodID getApkContentsSigners = env->GetMethodID(siClass, "getApkContentsSigners", "()[Landroid/content/pm/Signature;");
        jobjectArray signatures = (jobjectArray)env->CallObjectMethod(signingInfo, getApkContentsSigners);
        if (signatures == nullptr) return false;
        
        jint len = env->GetArrayLength(signatures);
        for (int i = 0; i < len; i++) {
            jobject sig = env->GetObjectArrayElement(signatures, i);
            jclass sigClass = env->GetObjectClass(sig);
            jmethodID toByteArray = env->GetMethodID(sigClass, "toByteArray", "()[B");
            jbyteArray sigBytes = (jbyteArray)env->CallObjectMethod(sig, toByteArray);
            
            jclass mdClass = env->FindClass("java/security/MessageDigest");
            jmethodID getInstance = env->GetStaticMethodID(mdClass, "getInstance", "(Ljava/lang/String;)Ljava/security/MessageDigest;");
            jobject md = env->CallStaticObjectMethod(mdClass, getInstance, env->NewStringUTF("SHA-256"));
            
            jmethodID update = env->GetMethodID(mdClass, "update", "([B)V");
            env->CallVoidMethod(md, update, sigBytes);
            
            jmethodID digest = env->GetMethodID(mdClass, "digest", "()[B");
            jbyteArray digestBytes = (jbyteArray)env->CallObjectMethod(md, digest);
            
            // Konversi ke hex lowercase
            char hexStr[65];
            for (int j = 0; j < 32; j++) {
                snprintf(hexStr + j*2, 3, "%02x", (unsigned char)digestBytes[j]);
            }
            hexStr[64] = '\0';
            
            if (strcmp(hexStr, ORIGINAL_SIGNATURE) == 0) return true;
        }
        return false;
    } else {
        jfieldID signaturesField = env->GetFieldID(piClass, "signatures", "[Landroid/content/pm/Signature;");
        jobjectArray signatures = (jobjectArray)env->GetObjectField(packageInfo, signaturesField);
        if (signatures == nullptr) return false;
        
        jint len = env->GetArrayLength(signatures);
        for (int i = 0; i < len; i++) {
            jobject sig = env->GetObjectArrayElement(signatures, i);
            jclass sigClass = env->GetObjectClass(sig);
            jmethodID toByteArray = env->GetMethodID(sigClass, "toByteArray", "()[B");
            jbyteArray sigBytes = (jbyteArray)env->CallObjectMethod(sig, toByteArray);
            
            jclass mdClass = env->FindClass("java/security/MessageDigest");
            jmethodID getInstance = env->GetStaticMethodID(mdClass, "getInstance", "(Ljava/lang/String;)Ljava/security/MessageDigest;");
            jobject md = env->CallStaticObjectMethod(mdClass, getInstance, env->NewStringUTF("SHA-256"));
            
            jmethodID update = env->GetMethodID(mdClass, "update", "([B)V");
            env->CallVoidMethod(md, update, sigBytes);
            
            jmethodID digest = env->GetMethodID(mdClass, "digest", "()[B");
            jbyteArray digestBytes = (jbyteArray)env->CallObjectMethod(md, digest);
            
            char hexStr[65];
            for (int j = 0; j < 32; j++) {
                snprintf(hexStr + j*2, 3, "%02x", (unsigned char)digestBytes[j]);
            }
            hexStr[64] = '\0';
            
            if (strcmp(hexStr, ORIGINAL_SIGNATURE) == 0) return true;
        }
        return false;
    }
}

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
    jmethodID getExternalCacheDir = env->GetMethodID(contextClass, "getExternalCacheDir", "()Ljava/io/File;");
    jobject externalCacheDir = env->CallObjectMethod(context, getExternalCacheDir);
    if (externalCacheDir != nullptr) {
        jclass fileClass = env->FindClass("java/io/File");
        jmethodID deleteRecursively = env->GetMethodID(fileClass, "deleteRecursively", "()Z");
        env->CallBooleanMethod(externalCacheDir, deleteRecursively);
    }
}

// ==================== MONITORING REAL-TIME (1 detik) ====================
static void startMonitoringThread(JNIEnv* env, jobject context) {
    JavaVM* jvm;
    env->GetJavaVM(&jvm);
    std::thread([jvm, context]() {
        JNIEnv* monitorEnv;
        jvm->AttachCurrentThread(&monitorEnv, nullptr);
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            if (isProxyOrVpnActive(monitorEnv, context) || isModifiedByTool(monitorEnv, context)) {
                clearAllCache(monitorEnv, context);
                jvm->DetachCurrentThread();
                exit(0);
            }
        }
    }).detach();
}

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

// ==================== ALL-IN-ONE SECURITY CHECK ====================
JNIEXPORT void JNICALL
Java_com_lagradost_cloudstream3_CloudStreamApp_nativeSecurityCheck(JNIEnv* env, jobject thiz) {
    // 1. Cek signature
    if (!isSignatureValid(env, thiz)) {
        clearAllCache(env, thiz);
        exit(0);
    }
    // 2. Cek mod tools
    if (isModifiedByTool(env, thiz)) {
        clearAllCache(env, thiz);
        exit(0);
    }
    // 3. Cek proxy/VPN
    if (isProxyOrVpnActive(env, thiz)) {
        clearAllCache(env, thiz);
        exit(0);
    }
}

// ==================== NATIVE MONITORING ====================
JNIEXPORT void JNICALL
Java_com_lagradost_cloudstream3_CloudStreamApp_startNativeMonitor(JNIEnv* env, jobject thiz) {
    startMonitoringThread(env, thiz);
}

} // extern "C"