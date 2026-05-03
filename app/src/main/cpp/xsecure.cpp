#include <jni.h>
#include <string>
#include <cstring>
#include <android/log.h>
#include <thread>
#include <chrono>
#include <stdlib.h>
#include <cctype>
#include <sys/ptrace.h>
#include <ctime>

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

// ==================== RANDOM ====================
static int randomGate() {
    static bool seeded = false;
    if (!seeded) {
        srand(time(NULL));
        seeded = true;
    }
    return rand() % 100;
}

// ==================== DELAYED KILL ====================
static void delayedKill() {
    std::thread([](){
        std::this_thread::sleep_for(std::chrono::milliseconds(300 + rand() % 700));
        exit(0);
    }).detach();
}

// ==================== DECOY ====================
static bool fakeSecurityCheck(JNIEnv*, jobject) {
    int r = rand() % 100;
    if (r < 5) return false;
    return true;
}

// ==================== OBFUSCATED SIGNATURE ====================
static std::string getOriginalSignature() {
    const unsigned char data[] = {
        0x38^0x5A,0x6B^0x5A,0x6B^0x5A,0x6F^0x5A,0x63^0x5A,0x62^0x5A,0x63^0x5A,0x3B^0x5A,
        0x38^0x5A,0x63^0x5A,0x3E^0x5A,0x3C^0x5A,0x3C^0x5A,0x3B^0x5A,0x3F^0x5A,0x3F^0x5A,
        0x68^0x5A,0x6F^0x5A,0x6F^0x5A,0x69^0x5A,0x6A^0x5A,0x3C^0x5A,0x3F^0x5A,0x3F^0x5A,
        0x3D^0x5A,0x3F^0x5A,0x6C^0x5A,0x3B^0x5A,0x6F^0x5A,0x3F^0x5A,0x3F^0x5A,0x3C^0x5A,
        0x6B^0x5A,0x6F^0x5A,0x6F^0x5A,0x6C^0x5A,0x6B^0x5A,0x6C^0x5A,0x3D^0x5A,0x3D^0x5A,
        0x3B^0x5A,0x3C^0x5A,0x3C^0x5A,0x6A^0x5A,0x3E^0x5A,0x6A^0x5A,0x6C^0x5A,0x3C^0x5A,
        0x3F^0x5A,0x3F^0x5A,0x3F^0x5A,0x3E^0x5A,0x3C^0x5A,0x3F^0x5A,0x3F^0x5A,0x3F^0x5A,
        0x6F^0x5A,0x6A^0x5A,0x6C^0x5A,0x6B^0x5A,0x6F^0x5A,0x68^0x5A,0x3C^0x5A,0x3C^0x5A
    };

    std::string result;
    for (int i = 0; i < 64; i++) {
        result += (char)(data[i] ^ 0x5A);
    }
    return result;
}

// ==================== HASH ====================
static std::string computeSha256(JNIEnv* env, jbyteArray input) {
    jclass mdClass = env->FindClass("java/security/MessageDigest");
    jmethodID getInstance = env->GetStaticMethodID(mdClass, "getInstance",
        "(Ljava/lang/String;)Ljava/security/MessageDigest;");
    jobject md = env->CallStaticObjectMethod(mdClass, getInstance, env->NewStringUTF("SHA-256"));

    jmethodID update = env->GetMethodID(mdClass, "update", "([B)V");
    env->CallVoidMethod(md, update, input);

    jmethodID digest = env->GetMethodID(mdClass, "digest", "()[B");
    jbyteArray digestBytes = (jbyteArray)env->CallObjectMethod(md, digest);

    jsize size = env->GetArrayLength(digestBytes);
    jbyte* data = env->GetByteArrayElements(digestBytes, nullptr);

    char hex[65];
    for (int i = 0; i < size; i++) {
        sprintf(hex + (i * 2), "%02x", (unsigned char)data[i]);
    }
    hex[64] = 0;

    env->ReleaseByteArrayElements(digestBytes, data, 0);
    return std::string(hex);
}

// ==================== ANTI DEBUG ====================
static bool detectDebugging() {
    if (ptrace(PTRACE_TRACEME, 0, 1, 0) == -1) return true;
    ptrace(PTRACE_DETACH, 0, 1, 0);
    return false;
}

// ==================== SIGNATURE ====================
static bool isSignatureValid(JNIEnv* env, jobject context) {

    jclass contextClass = env->GetObjectClass(context);

    jmethodID getPackageName = env->GetMethodID(contextClass, "getPackageName", "()Ljava/lang/String;");
    jstring packageName = (jstring)env->CallObjectMethod(context, getPackageName);

    jmethodID getPackageManager = env->GetMethodID(contextClass, "getPackageManager", "()Landroid/content/pm/PackageManager;");
    jobject pm = env->CallObjectMethod(context, getPackageManager);

    jclass pmClass = env->GetObjectClass(pm);
    jmethodID getPackageInfo = env->GetMethodID(pmClass, "getPackageInfo",
        "(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;");

    jint flags = 0x08000000;

    jobject packageInfo = env->CallObjectMethod(pm, getPackageInfo, packageName, flags);
    if (packageInfo == nullptr) return false;

    jclass piClass = env->GetObjectClass(packageInfo);

    jfieldID signingInfoField = env->GetFieldID(piClass, "signingInfo", "Landroid/content/pm/SigningInfo;");
    jobject signingInfo = env->GetObjectField(packageInfo, signingInfoField);

    jclass siClass = env->GetObjectClass(signingInfo);
    jmethodID getSigners = env->GetMethodID(siClass, "getApkContentsSigners",
        "()[Landroid/content/pm/Signature;");
    jobjectArray signatures = (jobjectArray)env->CallObjectMethod(signingInfo, getSigners);

    std::string ORIGINAL = getOriginalSignature();

    jint len = env->GetArrayLength(signatures);

    for (int i = 0; i < len; i++) {
        jobject sig = env->GetObjectArrayElement(signatures, i);

        jclass sigClass = env->GetObjectClass(sig);
        jmethodID toByteArray = env->GetMethodID(sigClass, "toByteArray", "()[B");
        jbyteArray sigBytes = (jbyteArray)env->CallObjectMethod(sig, toByteArray);

        std::string hash = computeSha256(env, sigBytes);

        if (hash == ORIGINAL) {
            return true;
        }
    }

    return false;
}

// ==================== DOUBLE CHECK ====================
static bool doubleSignature(JNIEnv* env, jobject context) {
    if (!isSignatureValid(env, context)) return false;

    if (rand() % 10 == 7) {
        if (!isSignatureValid(env, context)) return false;
    }

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

// ==================== SECURITY ====================
extern "C"
JNIEXPORT void JNICALL
Java_com_lagradost_cloudstream3_CloudStreamApp_nativeSecurityCheck(JNIEnv* env, jobject thiz) {

    int gate = randomGate();

    fakeSecurityCheck(env, thiz);

    if (gate < 70) {
        if (!doubleSignature(env, thiz)) { delayedKill(); return; }
        if (isModifiedByTool(env, thiz)) { delayedKill(); return; }
    }

    if (gate % 2 == 0) {
        if (isProxyOrVpnActive(env, thiz)) { delayedKill(); return; }
    }

    if (detectDebugging()) {
        if (gate > 40) delayedKill();
    }
}