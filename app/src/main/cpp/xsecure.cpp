#include <jni.h>
#include <string>
#include <cstring>
#include <android/log.h>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <unistd.h>
#include <cstdlib>

#define LOG_TAG "XSecure"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ==================== BASE64 DECODE ====================
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
            for (i = 0; i < 3; i++)
                ret += char_array_3[i];
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

// ==================== REPO PROTECTOR (ENCODED URLS) ====================
// Ganti dengan base64 dari URL premium dan free Anda yang sebenarnya
static const char* ENCODED_PREMIUM_REPO = "aHR0cHM6Ly9yYXcuZ2l0aHVidXNlcmNvbnRlbnQuY29tL251eXVsczc5L1N0cmVhbVBsYXktRnJlZS9yZWZzL2hlYWRzL2J1aWxkcy9yZXBvLmpzb24=";
static const char* ENCODED_FREE_REPO    = "aHR0cHM6Ly9yYXcuZ2l0aHVidXNlcmNvbnRlbnQuY29tL21pY2hhdDg4L1JlcG9fR3JhdGlzL3JlZnMvaGVhZHMvYnVpbGRzL3JlcG8uanNvbg==";

// ==================== PREMIUM STORAGE (FILE TERENKRIPSI) ====================
static const std::string PREMIUM_SALT = "ADIXTREAM_SECRET_KEY_2026_SECURE"; // SAMA DENGAN KOTLIN
static const std::string PREMIUM_FILE = "premium.dat";

static std::string getPremiumPath(JNIEnv* env, jobject context) {
    jclass cls = env->GetObjectClass(context);
    jmethodID getFilesDir = env->GetMethodID(cls, "getFilesDir", "()Ljava/io/File;");
    jobject dir = env->CallObjectMethod(context, getFilesDir);
    jclass fileCls = env->FindClass("java/io/File");
    jmethodID getPath = env->GetMethodID(fileCls, "getAbsolutePath", "()Ljava/lang/String;");
    jstring pathStr = (jstring)env->CallObjectMethod(dir, getPath);
    const char* path = env->GetStringUTFChars(pathStr, nullptr);
    std::string full = std::string(path) + "/" + PREMIUM_FILE;
    env->ReleaseStringUTFChars(pathStr, path);
    env->DeleteLocalRef(dir);
    env->DeleteLocalRef(pathStr);
    return full;
}

// Hash sederhana untuk proteksi file (tidak perlu MD5, cukup sederhana)
static std::string simpleHash(const std::string& data) {
    unsigned long hash = 5381;
    for (char c : data) hash = ((hash << 5) + hash) + (unsigned char)c;
    char buf[16];
    snprintf(buf, sizeof(buf), "%lx", hash);
    return std::string(buf);
}

static bool writePremiumData(const std::string& path, long long expiryMillis) {
    std::string data = std::to_string(expiryMillis);
    std::string sig = simpleHash(data + PREMIUM_SALT);
    std::string content = data + "|" + sig;
    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) return false;
    file << content;
    file.close();
    return true;
}

static long long readPremiumData(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return 0;
    std::string content;
    std::getline(file, content);
    file.close();
    size_t sep = content.find('|');
    if (sep == std::string::npos) return 0;
    std::string data = content.substr(0, sep);
    std::string sig = content.substr(sep + 1);
    if (simpleHash(data + PREMIUM_SALT) != sig) return 0;
    return std::stoll(data);
}

// ==================== MD5 VIA JNI (PERSIS SEPERTI KOTLIN) ====================
static std::string md5Hash(JNIEnv* env, const std::string& input) {
    jclass mdClass = env->FindClass("java/security/MessageDigest");
    jmethodID getInstance = env->GetStaticMethodID(mdClass, "getInstance", "(Ljava/lang/String;)Ljava/security/MessageDigest;");
    jstring md5 = env->NewStringUTF("MD5");
    jobject digest = env->CallStaticObjectMethod(mdClass, getInstance, md5);
    env->DeleteLocalRef(md5);

    jmethodID update = env->GetMethodID(mdClass, "update", "([B)V");
    jbyteArray inputBytes = env->NewByteArray(input.size());
    env->SetByteArrayRegion(inputBytes, 0, input.size(), (const jbyte*)input.c_str());
    env->CallVoidMethod(digest, update, inputBytes);
    env->DeleteLocalRef(inputBytes);

    jmethodID digestMethod = env->GetMethodID(mdClass, "digest", "()[B");
    jbyteArray hashBytes = (jbyteArray)env->CallObjectMethod(digest, digestMethod);
    jsize len = env->GetArrayLength(hashBytes);
    jbyte* bytes = env->GetByteArrayElements(hashBytes, nullptr);

    char hex[3];
    std::string result;
    for (int i = 0; i < len && result.size() < 3; ++i) {
        snprintf(hex, sizeof(hex), "%02x", (unsigned char)bytes[i]);
        result += hex;
    }
    env->ReleaseByteArrayElements(hashBytes, bytes, JNI_ABORT);
    env->DeleteLocalRef(hashBytes);
    env->DeleteLocalRef(digest);
    return result;
}

// ==================== VALIDASI KODE PREMIUM (PERSIS LOGIKA ASLI) ====================
static bool validateCode(JNIEnv* env, const std::string& code, const std::string& deviceId, long long& outExpiryMillis) {
    if (code.length() != 6) return false;
    std::string datePartHex = code.substr(0, 3);
    std::string sigPartHex = code.substr(3, 3);

    // Hitung signature menggunakan MD5 seperti di Kotlin
    std::string checkInput = deviceId + datePartHex + PREMIUM_SALT;
    std::string expectedSig = md5Hash(env, checkInput);
    // Ambil 3 karakter pertama
    expectedSig = expectedSig.substr(0, 3);
    // Jadikan uppercase (karena sigPartHex dari kode biasanya uppercase)
    for (char &c : expectedSig) c = toupper(c);

    if (sigPartHex != expectedSig) return false;

    // Hitung expiry (sama seperti asli)
    int daysFromEpoch = (int)strtol(datePartHex.c_str(), NULL, 16);
    struct tm epoch = {0};
    epoch.tm_year = 2025 - 1900; // 2025
    epoch.tm_mon = 0;            // Januari
    epoch.tm_mday = 1;
    time_t epochTime = mktime(&epoch);
    time_t expiryTime = epochTime + daysFromEpoch * 86400 + 86399; // 23:59:59
    outExpiryMillis = (long long)expiryTime * 1000LL;
    return (time(nullptr) <= expiryTime);
}

// ==================== ANTI-PROXY / VPN (OPSIONAL) ====================
static bool isProxyOrVpnActive(JNIEnv* env, jobject context) {
    // Cek proxy settings
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
        bool active = (strlen(proxyStr) > 0 && strcmp(proxyStr, ":0") != 0);
        env->ReleaseStringUTFChars(httpProxy, proxyStr);
        env->DeleteLocalRef(httpProxy);
        if (active) return true;
    }
    // Cek VPN
    jclass connectivityClass = env->FindClass("android/net/ConnectivityManager");
    jmethodID getSystemService = env->GetMethodID(contextClass, "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;");
    jstring connService = env->NewStringUTF("connectivity");
    jobject cm = env->CallObjectMethod(context, getSystemService, connService);
    env->DeleteLocalRef(connService);
    if (cm != nullptr) {
        jmethodID getActiveNetwork = env->GetMethodID(connectivityClass, "getActiveNetwork",
            "()Landroid/net/Network;");
        jobject net = env->CallObjectMethod(cm, getActiveNetwork);
        if (net != nullptr) {
            jmethodID getCaps = env->GetMethodID(connectivityClass, "getNetworkCapabilities",
                "(Landroid/net/Network;)Landroid/net/NetworkCapabilities;");
            jobject caps = env->CallObjectMethod(cm, getCaps, net);
            if (caps != nullptr) {
                jclass capsClass = env->FindClass("android/net/NetworkCapabilities");
                jmethodID hasTransport = env->GetMethodID(capsClass, "hasTransport", "(I)Z");
                jboolean hasVpn = env->CallBooleanMethod(caps, hasTransport, 4);
                env->DeleteLocalRef(caps);
                if (hasVpn) return true;
            }
            env->DeleteLocalRef(net);
        }
        env->DeleteLocalRef(cm);
    }
    return false;
}

static void clearCache(JNIEnv* env, jobject context) {
    jclass ctxClass = env->GetObjectClass(context);
    jmethodID getCacheDir = env->GetMethodID(ctxClass, "getCacheDir", "()Ljava/io/File;");
    jobject cacheDir = env->CallObjectMethod(context, getCacheDir);
    if (cacheDir != nullptr) {
        jclass fileClass = env->FindClass("java/io/File");
        jmethodID delRec = env->GetMethodID(fileClass, "deleteRecursively", "()Z");
        env->CallBooleanMethod(cacheDir, delRec);
        env->DeleteLocalRef(cacheDir);
    }
}

static void monitoringThread(JNIEnv* env, jobject context) {
    JavaVM* jvm;
    env->GetJavaVM(&jvm);
    jobject globalCtx = env->NewGlobalRef(context);
    std::thread([jvm, globalCtx]() {
        JNIEnv* monEnv;
        jvm->AttachCurrentThread(&monEnv, nullptr);
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            if (isProxyOrVpnActive(monEnv, globalCtx)) {
                clearCache(monEnv, globalCtx);
                jvm->DetachCurrentThread();
                exit(0);
            }
        }
    }).detach();
}

// ==================== JNI FUNCTIONS ====================
extern "C" {

// ---------- RepoProtector ----------
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

// ---------- PremiumManager ----------
JNIEXPORT jboolean JNICALL
Java_com_lagradost_cloudstream3_PremiumManager_nativeActivatePremium(JNIEnv* env, jclass,
    jstring jCode, jstring jDeviceId, jobject context) {
    const char* codePtr = env->GetStringUTFChars(jCode, nullptr);
    const char* devicePtr = env->GetStringUTFChars(jDeviceId, nullptr);
    std::string code(codePtr);
    std::string deviceId(devicePtr);
    env->ReleaseStringUTFChars(jCode, codePtr);
    env->ReleaseStringUTFChars(jDeviceId, devicePtr);

    long long expiry = 0;
    bool ok = validateCode(env, code, deviceId, expiry);
    if (!ok) return JNI_FALSE;

    std::string path = getPremiumPath(env, context);
    if (writePremiumData(path, expiry)) return JNI_TRUE;
    return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_lagradost_cloudstream3_PremiumManager_nativeIsPremium(JNIEnv* env, jclass, jobject context) {
    std::string path = getPremiumPath(env, context);
    long long expiry = readPremiumData(path);
    if (expiry == 0) return JNI_FALSE;
    jlong now = env->CallStaticLongMethod(env->FindClass("java/lang/System"),
                env->GetStaticMethodID(env->FindClass("java/lang/System"), "currentTimeMillis", "()J"));
    return (now < expiry) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jlong JNICALL
Java_com_lagradost_cloudstream3_PremiumManager_nativeGetExpiryMillis(JNIEnv* env, jclass, jobject context) {
    std::string path = getPremiumPath(env, context);
    return (jlong)readPremiumData(path);
}

JNIEXPORT jstring JNICALL
Java_com_lagradost_cloudstream3_PremiumManager_nativeGetExpiryString(JNIEnv* env, jclass, jobject context) {
    long long expiry = readPremiumData(getPremiumPath(env, context));
    if (expiry == 0) return env->NewStringUTF("Non-Premium");
    // Format menggunakan SimpleDateFormat Java
    jclass dateClass = env->FindClass("java/util/Date");
    jmethodID dateCtor = env->GetMethodID(dateClass, "<init>", "(J)V");
    jobject dateObj = env->NewObject(dateClass, dateCtor, expiry);
    jclass sdfClass = env->FindClass("java/text/SimpleDateFormat");
    jstring pattern = env->NewStringUTF("dd MMM yyyy");
    jclass localeClass = env->FindClass("java/util/Locale");
    jmethodID getDefault = env->GetStaticMethodID(localeClass, "getDefault", "()Ljava/util/Locale;");
    jobject locale = env->CallStaticObjectMethod(localeClass, getDefault);
    jmethodID sdfCtor = env->GetMethodID(sdfClass, "<init>", "(Ljava/lang/String;Ljava/util/Locale;)V");
    jobject sdf = env->NewObject(sdfClass, sdfCtor, pattern, locale);
    jmethodID format = env->GetMethodID(sdfClass, "format", "(Ljava/util/Date;)Ljava/lang/String;");
    jstring result = (jstring)env->CallObjectMethod(sdf, format, dateObj);
    env->DeleteLocalRef(pattern);
    env->DeleteLocalRef(locale);
    env->DeleteLocalRef(sdf);
    env->DeleteLocalRef(dateObj);
    return result;
}

JNIEXPORT void JNICALL
Java_com_lagradost_cloudstream3_PremiumManager_nativeDeactivatePremium(JNIEnv* env, jclass, jobject context) {
    std::string path = getPremiumPath(env, context);
    unlink(path.c_str());
}

// ---------- Anti-proxy & anti-tamper (opsional, dipanggil dari Application) ----------
JNIEXPORT void JNICALL
Java_com_lagradost_cloudstream3_CloudStreamApp_checkAndBlockNative(JNIEnv* env, jobject thiz) {
    // Cek signature APK di sini jika perlu
    if (isProxyOrVpnActive(env, thiz)) {
        clearCache(env, thiz);
        exit(0);
    }
}

JNIEXPORT void JNICALL
Java_com_lagradost_cloudstream3_CloudStreamApp_startNativeMonitor(JNIEnv* env, jobject thiz) {
    monitoringThread(env, thiz);
}

} // extern "C"