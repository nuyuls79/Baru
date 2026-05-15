#include <jni.h>
#include <string>
#include <cstring>
#include <android/log.h>
#include <thread>
#include <chrono>
#include <stdlib.h>
#include <mutex>
#include <ctime>
#include <cctype>

#define LOG_TAG "XSECURE"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

static bool g_premium = false;
static long long g_expiry = 0;
static std::string g_deviceId = "";

static std::mutex g_lock;

static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

static inline bool is_base64(unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string base64_decode(const std::string &encoded_string) {

    int in_len = encoded_string.size();

    int i = 0;
    int j = 0;
    int in_ = 0;

    unsigned char char_array_4[4], char_array_3[3];

    std::string ret;

    while (in_len-- &&
           (encoded_string[in_] != '=') &&
           is_base64(encoded_string[in_])) {

        char_array_4[i++] = encoded_string[in_];
        in_++;

        if (i == 4) {

            for (i = 0; i < 4; i++)
                char_array_4[i] =
                        base64_chars.find(char_array_4[i]);

            char_array_3[0] =
                    (char_array_4[0] << 2) +
                    ((char_array_4[1] & 0x30) >> 4);

            char_array_3[1] =
                    ((char_array_4[1] & 0xf) << 4) +
                    ((char_array_4[2] & 0x3c) >> 2);

            char_array_3[2] =
                    ((char_array_4[2] & 0x3) << 6) +
                    char_array_4[3];

            for (i = 0; i < 3; i++)
                ret += char_array_3[i];

            i = 0;
        }
    }

    if (i) {

        for (j = i; j < 4; j++)
            char_array_4[j] = 0;

        for (j = 0; j < 4; j++)
            char_array_4[j] =
                    base64_chars.find(char_array_4[j]);

        char_array_3[0] =
                (char_array_4[0] << 2) +
                ((char_array_4[1] & 0x30) >> 4);

        char_array_3[1] =
                ((char_array_4[1] & 0xf) << 4) +
                ((char_array_4[2] & 0x3c) >> 2);

        char_array_3[2] =
                ((char_array_4[2] & 0x3) << 6) +
                char_array_4[3];

        for (j = 0; j < i - 1; j++)
            ret += char_array_3[j];
    }

    return ret;
}

// ======================================================
// REPO
// ======================================================

static const char* ENCODED_PREMIUM_REPO =
"aHR0cHM6Ly9yYXcuZ2l0aHVidXNlcmNvbnRlbnQuY29tL251eXVsczc5L1N0cmVhbVBsYXktRnJlZS9yZWZzL2hlYWRzL2J1aWxkcy9yZXBvLmpzb24=";

static const char* ENCODED_FREE_REPO =
"aHR0cHM6Ly9yYXcuZ2l0aHVidXNlcmNvbnRlbnQuY29tL21pY2hhdDg4L1JlcG9fR3JhdGlzL3JlZnMvaGVhZHMvYnVpbGRzL3JlcG8uanNvbg==";

// ======================================================
// APK SIGNATURE CHECK
// ======================================================

static bool isApkSignatureValid(JNIEnv* env, jobject context) {

    // sementara always true
    // nanti bisa diisi SHA256 signature check

    return true;
}

// ======================================================
// PROXY / VPN CHECK
// ======================================================

static bool isProxyOrVpnActive(
        JNIEnv* env,
        jobject context
) {

    jclass settingsClass =
            env->FindClass(
                    "android/provider/Settings$Global"
            );

    if (settingsClass == nullptr)
        return false;

    jmethodID getString =
            env->GetStaticMethodID(
                    settingsClass,
                    "getString",
                    "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;"
            );

    jclass contextClass =
            env->GetObjectClass(context);

    jmethodID getContentResolver =
            env->GetMethodID(
                    contextClass,
                    "getContentResolver",
                    "()Landroid/content/ContentResolver;"
            );

    jobject resolver =
            env->CallObjectMethod(
                    context,
                    getContentResolver
            );

    jstring proxyKey =
            env->NewStringUTF("http_proxy");

    jstring proxyValue =
            (jstring) env->CallStaticObjectMethod(
                    settingsClass,
                    getString,
                    resolver,
                    proxyKey
            );

    env->DeleteLocalRef(proxyKey);

    if (proxyValue != nullptr) {

        const char* proxy =
                env->GetStringUTFChars(
                        proxyValue,
                        nullptr
                );

        bool active =
                strlen(proxy) > 0 &&
                strcmp(proxy, ":0") != 0;

        env->ReleaseStringUTFChars(
                proxyValue,
                proxy
        );

        if (active)
            return true;
    }

    return false;
}

// ======================================================
// CLEAR CACHE
// ======================================================

static void clearAllCache(
        JNIEnv* env,
        jobject context
) {

    jclass contextClass =
            env->GetObjectClass(context);

    jmethodID getCacheDir =
            env->GetMethodID(
                    contextClass,
                    "getCacheDir",
                    "()Ljava/io/File;"
            );

    jobject cacheDir =
            env->CallObjectMethod(
                    context,
                    getCacheDir
            );

    if (cacheDir != nullptr) {

        jclass fileClass =
                env->FindClass("java/io/File");

        jmethodID deleteMethod =
                env->GetMethodID(
                        fileClass,
                        "delete",
                        "()Z"
                );

        env->CallBooleanMethod(
                cacheDir,
                deleteMethod
        );
    }
}

// ======================================================
// MONITOR THREAD
// ======================================================

static void startMonitoringThread(
        JNIEnv* env,
        jobject context
) {

    JavaVM* vm;
    env->GetJavaVM(&vm);

    jobject globalContext =
            env->NewGlobalRef(context);

    std::thread([vm, globalContext]() {

        JNIEnv* envThread = nullptr;

        vm->AttachCurrentThread(
                &envThread,
                nullptr
        );

        while (true) {

            std::this_thread::sleep_for(
                    std::chrono::seconds(2)
            );

            if (isProxyOrVpnActive(
                    envThread,
                    globalContext
            )) {

                clearAllCache(
                        envThread,
                        globalContext
                );

                exit(0);
            }
        }

    }).detach();
}

// ======================================================
// JNI
// ======================================================

extern "C" {

// ===================== REPO ======================

JNIEXPORT jstring JNICALL
Java_com_lagradost_cloudstream3_utils_RepoProtector_nativeGetPremiumRepoUrl(
        JNIEnv* env,
        jclass clazz
) {

    std::string decoded =
            base64_decode(
                    ENCODED_PREMIUM_REPO
            );

    return env->NewStringUTF(
            decoded.c_str()
    );
}

JNIEXPORT jstring JNICALL
Java_com_lagradost_cloudstream3_utils_RepoProtector_nativeGetFreeRepoUrl(
        JNIEnv* env,
        jclass clazz
) {

    std::string decoded =
            base64_decode(
                    ENCODED_FREE_REPO
            );

    return env->NewStringUTF(
            decoded.c_str()
    );
}

// ===================== PREMIUM ======================

JNIEXPORT void JNICALL
Java_com_lagradost_cloudstream3_PremiumManager_nativeSetPremium(
        JNIEnv* env,
        jobject thiz,
        jboolean premium,
        jlong expiry,
        jstring deviceId
) {

    std::lock_guard<std::mutex> guard(g_lock);

    g_premium = premium;
    g_expiry = expiry;

    const char* dev =
            env->GetStringUTFChars(
                    deviceId,
                    nullptr
            );

    g_deviceId = dev;

    env->ReleaseStringUTFChars(
            deviceId,
            dev
    );

    LOGD("Premium activated");
}

JNIEXPORT jboolean JNICALL
Java_com_lagradost_cloudstream3_PremiumManager_nativeIsPremium(
        JNIEnv* env,
        jobject thiz,
        jlong currentTime,
        jstring deviceId
) {

    std::lock_guard<std::mutex> guard(g_lock);

    if (!g_premium)
        return JNI_FALSE;

    const char* dev =
            env->GetStringUTFChars(
                    deviceId,
                    nullptr
            );

    std::string currentDev = dev;

    env->ReleaseStringUTFChars(
            deviceId,
            dev
    );

    if (currentDev != g_deviceId)
        return JNI_FALSE;

    if (currentTime > g_expiry) {

        g_premium = false;
        g_expiry = 0;
        g_deviceId.clear();

        return JNI_FALSE;
    }

    return JNI_TRUE;
}

JNIEXPORT jlong JNICALL
Java_com_lagradost_cloudstream3_PremiumManager_nativeGetExpiry(
        JNIEnv* env,
        jobject thiz
) {

    std::lock_guard<std::mutex> guard(g_lock);

    return g_expiry;
}

JNIEXPORT void JNICALL
Java_com_lagradost_cloudstream3_PremiumManager_nativeClearPremium(
        JNIEnv* env,
        jobject thiz
) {

    std::lock_guard<std::mutex> guard(g_lock);

    g_premium = false;
    g_expiry = 0;
    g_deviceId.clear();

    LOGD("Premium cleared");
}

// ===================== CHECK ======================

JNIEXPORT void JNICALL
Java_com_lagradost_cloudstream3_CloudStreamApp_checkAndBlock(
        JNIEnv* env,
        jobject thiz
) {

    if (!isApkSignatureValid(env, thiz)) {

        clearAllCache(env, thiz);

        exit(0);
    }

    if (isProxyOrVpnActive(env, thiz)) {

        clearAllCache(env, thiz);

        exit(0);
    }
}

// ===================== MONITOR ======================

JNIEXPORT void JNICALL
Java_com_lagradost_cloudstream3_CloudStreamApp_startNativeMonitor(
        JNIEnv* env,
        jobject thiz
) {

    startMonitoringThread(env, thiz);
}

}