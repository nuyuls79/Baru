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

JNIEXPORT void JNICALL
Java_com_lagradost_cloudstream3_MainActivity_nativeSecurityCheck(JNIEnv* env, jobject activity) {
    // 1. Cek proxy
    jclass settingsClass = env->FindClass("android/provider/Settings$Global");
    jmethodID getString = env->GetStaticMethodID(settingsClass, "getString",
        "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;");
    jclass contextClass = env->GetObjectClass(activity);
    jmethodID getContentResolver = env->GetMethodID(contextClass, "getContentResolver",
        "()Landroid/content/ContentResolver;");
    jobject contentResolver = env->CallObjectMethod(activity, getContentResolver);
    jstring httpProxy = (jstring)env->CallStaticObjectMethod(settingsClass, getString,
        contentResolver, env->NewStringUTF("http_proxy"));

    bool proxyActive = false;
    if (httpProxy != nullptr) {
        const char* proxyStr = env->GetStringUTFChars(httpProxy, nullptr);
        proxyActive = (strlen(proxyStr) > 0 && strcmp(proxyStr, ":0") != 0);
        env->ReleaseStringUTFChars(httpProxy, proxyStr);
    }

    // 2. Cek VPN
    bool vpnActive = false;
    jclass connectivityClass = env->FindClass("android/net/ConnectivityManager");
    jmethodID getSystemService = env->GetMethodID(contextClass, "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;");
    jstring connectivityService = env->NewStringUTF("connectivity");
    jobject connectivityManager = env->CallObjectMethod(activity, getSystemService,
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
                vpnActive = env->CallBooleanMethod(capabilities, hasTransport, 4); // TRANSPORT_VPN = 4
            }
        }
    }

    if (proxyActive || vpnActive) {
        // 3. Tampilkan dialog dan langsung panggil finish()
        jclass alertDialogBuilderClass = env->FindClass("android/app/AlertDialog$Builder");
        jmethodID builderConstructor = env->GetMethodID(alertDialogBuilderClass, "<init>",
            "(Landroid/content/Context;)V");
        jobject builder = env->NewObject(alertDialogBuilderClass, builderConstructor, activity);

        jmethodID setTitle = env->GetMethodID(alertDialogBuilderClass, "setTitle",
            "(Ljava/lang/CharSequence;)Landroid/app/AlertDialog$Builder;");
        jmethodID setMessage = env->GetMethodID(alertDialogBuilderClass, "setMessage",
            "(Ljava/lang/CharSequence;)Landroid/app/AlertDialog$Builder;");
        jmethodID setCancelable = env->GetMethodID(alertDialogBuilderClass, "setCancelable",
            "(Z)Landroid/app/AlertDialog$Builder;");
        jmethodID setPositiveButton = env->GetMethodID(alertDialogBuilderClass, "setPositiveButton",
            "(Ljava/lang/CharSequence;Landroid/content/DialogInterface$OnClickListener;)Landroid/app/AlertDialog$Builder;");
        jmethodID show = env->GetMethodID(alertDialogBuilderClass, "show",
            "()Landroid/app/AlertDialog;");

        jstring title = env->NewStringUTF("InternetServiceProvider Error");
        jstring message = env->NewStringUTF("Maaf, jaringan Anda terganggu. Aplikasi tidak dapat berjalan.");
        jstring buttonText = env->NewStringUTF("Keluar");

        builder = env->CallObjectMethod(builder, setTitle, title);
        builder = env->CallObjectMethod(builder, setMessage, message);
        builder = env->CallObjectMethod(builder, setCancelable, JNI_FALSE);
        builder = env->CallObjectMethod(builder, setPositiveButton, buttonText, nullptr);
        env->CallObjectMethod(builder, show);

        // Langsung panggil finish() agar activity tertutup
        jclass activityClass = env->GetObjectClass(activity);
        jmethodID finishMethod = env->GetMethodID(activityClass, "finish", "()V");
        env->CallVoidMethod(activity, finishMethod);
    }
}

} // extern "C"