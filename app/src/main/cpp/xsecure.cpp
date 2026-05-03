#include <jni.h>
#include <string>
#include <cstring>
#include <thread>
#include <chrono>
#include <stdlib.h>
#include <cctype>

// ==================== DELAYED KILL ====================
static void delayedKill() {
    std::thread([](){
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        exit(0);
    }).detach();
}

// ==================== SIGNATURE (SAFE MODE) ====================
static bool isSignatureValid(JNIEnv* env, jobject context) {
    // 🔥 sementara disable (biar tidak false positive)
    return true;
}

// ==================== MOD ====================
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

// ==================== VPN ====================
static bool isProxyOrVpnActive(JNIEnv* env, jobject context) {
    jclass settingsClass = env->FindClass("android/provider/Settings$Global");
    jmethodID getString = env->GetStaticMethodID(settingsClass, "getString",
        "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;");

    jclass contextClass = env->GetObjectClass(context);
    jmethodID getContentResolver = env->GetMethodID(contextClass, "getContentResolver",
        "()Landroid/content/ContentResolver;");
    jobject contentResolver = env->CallObjectMethod(context, getContentResolver);

    jstring key = env->NewStringUTF("http_proxy");
    jstring httpProxy = (jstring)env->CallStaticObjectMethod(settingsClass, getString,
        contentResolver, key);
    env->DeleteLocalRef(key);

    if (httpProxy != nullptr) {
        const char* proxyStr = env->GetStringUTFChars(httpProxy, nullptr);
        bool proxyActive = (strlen(proxyStr) > 0 && strcmp(proxyStr, ":0") != 0);
        env->ReleaseStringUTFChars(httpProxy, proxyStr);
        if (proxyActive) return true;
    }

    return false;
}

// ==================== SECURITY (SUPER STABLE) ====================
extern "C"
JNIEXPORT void JNICALL
Java_com_lagradost_cloudstream3_CloudStreamApp_nativeSecurityCheck(JNIEnv* env, jobject thiz) {

    // 🔥 hanya check ringan (tidak pakai counter)
    bool bad = false;

    if (isModifiedByTool(env, thiz)) bad = true;

    if (isProxyOrVpnActive(env, thiz)) bad = true;

    // 🔥 hanya kill jika jelas
    if (bad) {
        delayedKill();
    }
}