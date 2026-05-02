#include <jni.h>
#include <string>
#include <cstring>
#include <android/log.h>
#include <thread>
#include <chrono>
#include <stdlib.h>
#include <cctype>

// ==================== BASE64 ====================
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

    while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
        char_array_4[i++] = encoded_string[in_]; in_++;
        if (i == 4) {
            for (i = 0; i < 4; i++)
                char_array_4[i] = base64_chars.find(char_array_4[i]);

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; i < 3; i++) ret += char_array_3[i];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 4; j++) char_array_4[j] = 0;
        for (j = 0; j < 4; j++) char_array_4[j] = base64_chars.find(char_array_4[j]);

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

    return false;
}

// ==================== DETEKSI MOD ====================
static bool isModifiedByTool(JNIEnv* env, jobject context) {
    jclass contextClass = env->GetObjectClass(context);
    jmethodID getAssets = env->GetMethodID(contextClass, "getAssets", "()Landroid/content/res/AssetManager;");
    jobject assetManager = env->CallObjectMethod(context, getAssets);

    jclass assetManagerClass = env->GetObjectClass(assetManager);
    jmethodID openMethod = env->GetMethodID(assetManagerClass, "open", "(Ljava/lang/String;)Ljava/io/InputStream;");

    jstring fileName = env->NewStringUTF("assets/pms");
    jobject inputStream = env->CallObjectMethod(assetManager, openMethod, fileName);
    env->DeleteLocalRef(fileName);

    if (inputStream != nullptr) return true;

    env->ExceptionClear();
    return false;
}

// ==================== SIGNATURE ====================
static bool isSignatureValid(JNIEnv* env, jobject context) {
    jclass cls = env->GetObjectClass(context);
    jmethodID method = env->GetMethodID(cls, "isSignatureValid", "()Z");

    if (method == nullptr) return false;

    return env->CallBooleanMethod(context, method);
}

// ==================== CLEAR CACHE ====================
static void clearAllCache(JNIEnv* env, jobject context) {
    jclass cls = env->GetObjectClass(context);
    jmethodID method = env->GetMethodID(cls, "clearAllCache", "()V");

    if (method != nullptr) {
        env->CallVoidMethod(context, method);
    }
}

// ==================== SCORE (INI YANG DIPAKAI) ====================
extern "C"
JNIEXPORT jint JNICALL
Java_com_lagradost_cloudstream3_CloudStreamApp_getSecurityScoreNative(
        JNIEnv *env,
        jobject thiz
) {
    int score = 0;

    if (isSignatureValid(env, thiz)) score += 13;
    if (!isModifiedByTool(env, thiz)) score += 17;
    if (!isProxyOrVpnActive(env, thiz)) score += 19;

    return score;
}

// ==================== REPO ====================
extern "C" {

static const char* ENCODED_PREMIUM_REPO = "aHR0cHM6Ly9yYXcuZ2l0aHVidXNlcmNvbnRlbnQuY29tL251eXVsczc5L1N0cmVhbVBsYXktRnJlZS9yZWZzL2hlYWRzL2J1aWxkcy9yZXBvLmpzb24=";
static const char* ENCODED_FREE_REPO    = "aHR0cHM6Ly9yYXcuZ2l0aHVidXNlcmNvbnRlbnQuY29tL21pY2hhdDg4L1JlcG9fR3JhdGlzL3JlZnMvaGVhZHMvYnVpbGRzL3JlcG8uanNvbg==";

JNIEXPORT jstring JNICALL
Java_com_lagradost_cloudstream3_utils_RepoProtector_nativeGetPremiumRepoUrl(JNIEnv* env, jclass) {
    return env->NewStringUTF(base64_decode(ENCODED_PREMIUM_REPO).c_str());
}

JNIEXPORT jstring JNICALL
Java_com_lagradost_cloudstream3_utils_RepoProtector_nativeGetFreeRepoUrl(JNIEnv* env, jclass) {
    return env->NewStringUTF(base64_decode(ENCODED_FREE_REPO).c_str());
}

// ==================== SECURITY CHECK ====================
JNIEXPORT void JNICALL
Java_com_lagradost_cloudstream3_CloudStreamApp_nativeSecurityCheck(JNIEnv* env, jobject thiz) {
    if (!isSignatureValid(env, thiz)) {
        clearAllCache(env, thiz);
        exit(0);
    }
    if (isModifiedByTool(env, thiz)) {
        clearAllCache(env, thiz);
        exit(0);
    }
    if (isProxyOrVpnActive(env, thiz)) {
        clearAllCache(env, thiz);
        exit(0);
    }
}

}