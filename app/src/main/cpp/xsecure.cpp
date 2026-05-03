#include <jni.h>
#include <string>
#include <cstring>
#include <stdlib.h>
#include <cctype>
#include <sys/ptrace.h>

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

// ==================== SIGNATURE ====================
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

// ==================== SECURITY CORE ====================
static bool isSignatureValid(JNIEnv* env, jobject context) {
    jclass ctx = env->GetObjectClass(context);

    jmethodID getPM = env->GetMethodID(ctx, "getPackageManager", "()Landroid/content/pm/PackageManager;");
    jobject pm = env->CallObjectMethod(context, getPM);

    jmethodID getPN = env->GetMethodID(ctx, "getPackageName", "()Ljava/lang/String;");
    jstring pn = (jstring)env->CallObjectMethod(context, getPN);

    jclass pmClass = env->GetObjectClass(pm);
    jmethodID getPI = env->GetMethodID(pmClass, "getPackageInfo",
        "(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;");

    jobject pi = env->CallObjectMethod(pm, getPI, pn, 0x08000000);
    if (pi == nullptr) return false;

    jclass piClass = env->GetObjectClass(pi);
    jfieldID signingInfoField = env->GetFieldID(piClass, "signingInfo", "Landroid/content/pm/SigningInfo;");
    jobject signingInfo = env->GetObjectField(pi, signingInfoField);

    jclass siClass = env->GetObjectClass(signingInfo);
    jmethodID getSigners = env->GetMethodID(siClass, "getApkContentsSigners",
        "()[Landroid/content/pm/Signature;");
    jobjectArray sigs = (jobjectArray)env->CallObjectMethod(signingInfo, getSigners);

    std::string original = getOriginalSignature();

    jint len = env->GetArrayLength(sigs);
    for (int i = 0; i < len; i++) {
        jobject sig = env->GetObjectArrayElement(sigs, i);
        jclass sigClass = env->GetObjectClass(sig);

        jmethodID toBytes = env->GetMethodID(sigClass, "toByteArray", "()[B");
        jbyteArray bytes = (jbyteArray)env->CallObjectMethod(sig, toBytes);

        std::string hash = computeSha256(env, bytes);
        if (hash == original) return true;
    }

    return false;
}

// ==================== MOD ====================
static bool isModifiedByTool(JNIEnv* env, jobject context) {
    jclass ctx = env->GetObjectClass(context);
    jmethodID getAssets = env->GetMethodID(ctx, "getAssets", "()Landroid/content/res/AssetManager;");
    jobject assets = env->CallObjectMethod(context, getAssets);

    jclass am = env->GetObjectClass(assets);
    jmethodID open = env->GetMethodID(am, "open", "(Ljava/lang/String;)Ljava/io/InputStream;");

    jstring file = env->NewStringUTF("assets/pms");
    jobject stream = env->CallObjectMethod(assets, open, file);

    if (stream != nullptr) return true;
    env->ExceptionClear();
    return false;
}

// ==================== VPN ====================
static bool isProxyOrVpnActive(JNIEnv* env, jobject context) {
    jclass settings = env->FindClass("android/provider/Settings$Global");
    jmethodID getString = env->GetStaticMethodID(settings, "getString",
        "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;");

    jclass ctx = env->GetObjectClass(context);
    jmethodID getCR = env->GetMethodID(ctx, "getContentResolver",
        "()Landroid/content/ContentResolver;");
    jobject cr = env->CallObjectMethod(context, getCR);

    jstring key = env->NewStringUTF("http_proxy");
    jstring proxy = (jstring)env->CallStaticObjectMethod(settings, getString, cr, key);

    if (proxy != nullptr) {
        const char* p = env->GetStringUTFChars(proxy, nullptr);
        bool active = strlen(p) > 0 && strcmp(p, ":0") != 0;
        env->ReleaseStringUTFChars(proxy, p);
        if (active) return true;
    }
    return false;
}

// ==================== NATIVE ====================
extern "C" {

static jint native_getSecurityScore(JNIEnv* env, jobject thiz) {
    int score = 0;
    if (isSignatureValid(env, thiz)) score += 13;
    if (!isModifiedByTool(env, thiz)) score += 17;
    if (!isProxyOrVpnActive(env, thiz)) score += 19;
    if (!detectDebugging()) score += 23;
    return score;
}

static void native_securityCheck(JNIEnv* env, jobject thiz) {
    if (!isSignatureValid(env, thiz)) exit(0);
    if (isModifiedByTool(env, thiz)) exit(0);
    if (isProxyOrVpnActive(env, thiz)) exit(0);
    if (detectDebugging()) exit(0);
}

// ==================== REGISTER ====================
static JNINativeMethod appMethods[] = {
    {"getSecurityScoreNative", "()I", (void*)native_getSecurityScore},
    {"nativeSecurityCheck", "()V", (void*)native_securityCheck},
};

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    JNIEnv* env = nullptr;

    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_VERSION_1_6;
    }

    jclass appClass = env->FindClass("com/lagradost/cloudstream3/CloudStreamApp");
    if (appClass != nullptr) {
        env->RegisterNatives(appClass, appMethods, 2);
    } else {
        env->ExceptionClear();
    }

    return JNI_VERSION_1_6;
}

}