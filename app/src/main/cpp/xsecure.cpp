#include <jni.h>
#include <string>
#include <cstring>
#include <android/log.h>

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

// ==================== DETEKSI PROXY & VPN (ANTI-HTTPCANARY, DLL.) ====================
JNIEXPORT jboolean JNICALL
Java_com_lagradost_cloudstream3_MainActivity_isProxyOrVpnActive(JNIEnv* env, jclass, jobject context) {
    // 1. Cek proxy dari Settings.Global
    jclass settingsGlobalClass = env->FindClass("android/provider/Settings$Global");
    jmethodID getStringMethod = env->GetStaticMethodID(settingsGlobalClass, "getString",
        "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;");
    jclass contextClass = env->GetObjectClass(context);
    jmethodID getContentResolverMethod = env->GetMethodID(contextClass, "getContentResolver",
        "()Landroid/content/ContentResolver;");
    jobject contentResolver = env->CallObjectMethod(context, getContentResolverMethod);
    jstring httpProxyJ = (jstring)env->CallStaticObjectMethod(settingsGlobalClass, getStringMethod,
        contentResolver, env->NewStringUTF("http_proxy"));

    if (httpProxyJ != nullptr) {
        const char* proxyStr = env->GetStringUTFChars(httpProxyJ, nullptr);
        bool proxyActive = (strlen(proxyStr) > 0 && strcmp(proxyStr, ":0") != 0);
        env->ReleaseStringUTFChars(httpProxyJ, proxyStr);
        if (proxyActive) return JNI_TRUE;
    }

    // 2. Cek VPN dari ConnectivityManager
    jstring connectivityServiceName = env->NewStringUTF("connectivity");
    jmethodID getSystemServiceMethod = env->GetMethodID(contextClass, "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;");
    jobject connectivityManager = env->CallObjectMethod(context, getSystemServiceMethod,
        connectivityServiceName);
    env->DeleteLocalRef(connectivityServiceName);

    if (connectivityManager != nullptr) {
        jclass connectivityManagerClass = env->FindClass("android/net/ConnectivityManager");
        jmethodID getActiveNetworkMethod = env->GetMethodID(connectivityManagerClass,
            "getActiveNetwork", "()Landroid/net/Network;");
        jobject activeNetwork = env->CallObjectMethod(connectivityManager, getActiveNetworkMethod);
        if (activeNetwork != nullptr) {
            jmethodID getNetworkCapabilitiesMethod = env->GetMethodID(connectivityManagerClass,
                "getNetworkCapabilities",
                "(Landroid/net/Network;)Landroid/net/NetworkCapabilities;");
            jobject capabilities = env->CallObjectMethod(connectivityManager,
                getNetworkCapabilitiesMethod, activeNetwork);
            if (capabilities != nullptr) {
                jclass networkCapabilitiesClass = env->FindClass("android/net/NetworkCapabilities");
                jmethodID hasTransportMethod = env->GetMethodID(networkCapabilitiesClass,
                    "hasTransport", "(I)Z");
                // TRANSPORT_VPN = 4
                jboolean hasVpn = env->CallBooleanMethod(capabilities, hasTransportMethod, 4);
                if (hasVpn) return JNI_TRUE;
            }
        }
    }
    return JNI_FALSE;
}
// ====================================================================================

} // extern "C"