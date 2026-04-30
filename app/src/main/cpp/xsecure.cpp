#include <jni.h>
#include <string>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <sys/stat.h>
#include <android/log.h>

#define LOG_TAG "xsecure"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

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

std::string md5(const std::string &input) {
    unsigned int hash = 5381;
    for (char c : input) {
        hash = ((hash << 5) + hash) + c;
    }
    char buf[9];
    snprintf(buf, sizeof(buf), "%08x", hash);
    return std::string(buf);
}

extern "C" {

static const char* ENCODED_PREMIUM_REPO = "aHR0cHM6Ly9yYXcuZ2l0aHVidXNlcmNvbnRlbnQuY29tL251eXVsczc5L1N0cmVhbVBsYXktRnJlZS9yZWZzL2hlYWRzL2J1aWxkcy9yZXBvLmpzb24=";
static const char* ENCODED_FREE_REPO    = "aHR0cHM6Ly9yYXcuZ2l0aHVidXNlcmNvbnRlbnQuY29tL21pY2hhdDg4L1JlcG9fR3JhdGlzL3JlZnMvaGVhZHMvYnVpbGRzL3JlcG8uanNvbg==";
static const char* ENCODED_FIREBASE_URL = "aHR0cHM6Ly9hZGl4dHJlYW0tcHJlbWl1bS1kZWZhdWx0LXJ0ZGIuYXNpYS1zb3V0aGVhc3QxLmZpcmViYXNlZGF0YWJhc2UuYXBwLw==";

JNIEXPORT jstring JNICALL
Java_com_lagradost_cloudstream3_utils_RepoProtector_getPremiumRepoUrl(JNIEnv* env, jclass) {
    std::string decoded = base64_decode(ENCODED_PREMIUM_REPO);
    return env->NewStringUTF(decoded.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_lagradost_cloudstream3_utils_RepoProtector_getFreeRepoUrl(JNIEnv* env, jclass) {
    std::string decoded = base64_decode(ENCODED_FREE_REPO);
    return env->NewStringUTF(decoded.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_lagradost_cloudstream3_utils_RepoProtector_getFirebaseUrl(JNIEnv* env, jclass) {
    std::string decoded = base64_decode(ENCODED_FIREBASE_URL);
    return env->NewStringUTF(decoded.c_str());
}

// Validasi kode premium di native, panggil callback Kotlin untuk menyimpan data
JNIEXPORT jboolean JNICALL
Java_com_lagradost_cloudstream3_PremiumManager_nativeActivatePremium(JNIEnv* env, jclass, jstring jcode, jstring jdeviceId) {
    const char* code = env->GetStringUTFChars(jcode, 0);
    const char* deviceId = env->GetStringUTFChars(jdeviceId, 0);

    if (strlen(code) != 6) {
        env->ReleaseStringUTFChars(jcode, code);
        env->ReleaseStringUTFChars(jdeviceId, deviceId);
        return false;
    }

    std::string datePartHex(code, 3);
    std::string sigPartHex(code + 3, 3);
    static const char* SALT = "ADIXTREAM_SECRET_KEY_2026_SECURE";
    std::string checkInput = std::string(deviceId) + datePartHex + SALT;
    std::string md5hash = md5(checkInput);
    std::string expectedSig = md5hash.substr(0, 3);

    bool valid = (sigPartHex == expectedSig);
    long days = strtol(datePartHex.c_str(), NULL, 16);
    time_t now = time(NULL);
    tm epoch = {};
    epoch.tm_year = 2025 - 1900;
    epoch.tm_mon = 0;
    epoch.tm_mday = 1;
    time_t baseTime = mktime(&epoch);
    time_t expiryTime = baseTime + (days * 86400);

    if (now > expiryTime) valid = false;

    // Panggil fungsi di PremiumManager untuk menyimpan hasil
    jclass cls = env->FindClass("com/lagradost/cloudstream3/PremiumManager");
    if (cls) {
        jmethodID mid = env->GetStaticMethodID(cls, "persistPremiumData", "(ZJ)V");
        if (mid) {
            env->CallStaticVoidMethod(cls, mid, valid, (jlong)expiryTime * 1000); // ms
        }
    }

    env->ReleaseStringUTFChars(jcode, code);
    env->ReleaseStringUTFChars(jdeviceId, deviceId);
    return valid;
}

// Cek signature APK (anti-repack)
JNIEXPORT jboolean JNICALL
Java_com_lagradost_cloudstream3_PremiumManager_isSignatureValid(JNIEnv* env, jclass) {
    // Implementasi sederhana: baca /proc/self/maps atau gunakan PackageManager
    // Untuk contoh ini kita anggap valid (Anda bisa implementasikan sendiri)
    return true;
}

} // extern "C"