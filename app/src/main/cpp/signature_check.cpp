#include <jni.h>
#include <string>
#include <android/log.h>
#include <cstring>
#include <sstream>

#define LOG_TAG "SignatureCheck"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

std::string decrypt(const char* encrypted, size_t len, char key) {
    std::string result(len, '\0');
    for (size_t i = 0; i < len; ++i) {
        result[i] = encrypted[i] ^ (key + i % 7);
    }
    return result;
}

extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGI("JNI_OnLoad called");
    return JNI_VERSION_1_6;
}

JNIEXPORT jstring JNICALL
Java_com_lagradost_cloudstream3_SignatureUtils_getSignatureNative(
        JNIEnv* env,
        jobject thiz) {

    // ========== TEMPEL ARRAY YANG SUDAH DIPERBAIKI DI SINI ==========
    const char expected_hash_enc[] = {'\277'^0x5A, '\252'^0x5B, '\044'^0x5C, '\347'^0x5D, '\360'^0x5E, '\174'^0x5F, '\222'^0x60, '\203'^0x5A, '\132'^0x5B, '\060'^0x5C, '\237'^0x5D, '\242'^0x5E, '\257'^0x5F, '\121'^0x60, '\237'^0x5A, '\070'^0x5B, '\324'^0x5C, '\344'^0x5D, '\364'^0x5E, '\321'^0x5F, '\241'^0x60, '\264'^0x5A, '\104'^0x5B, '\047'^0x5C, '\253'^0x5D, '\160'^0x5E, '\243'^0x5F, '\332'^0x60, '\151'^0x5A, '\101'^0x5B, '\245'^0x5C, '\107'^0x5D};
    // ===============================================================

    const size_t hash_len = sizeof(expected_hash_enc);
    const char xor_key = 0x5A;
    std::string expected_hash = decrypt(expected_hash_enc, hash_len, xor_key);
    LOGI("Expected hash: %s", expected_hash.c_str());

    // Dapatkan context
    jclass native_class = env->GetObjectClass(thiz);
    jmethodID get_app_context = env->GetMethodID(native_class, "getApplicationContext", "()Landroid/content/Context;");
    jobject context = env->CallObjectMethod(thiz, get_app_context);
    if (context == nullptr) {
        LOGE("Failed to get context");
        return env->NewStringUTF("ERROR: No context");
    }

    jclass context_class = env->GetObjectClass(context);
    jmethodID get_package_manager = env->GetMethodID(context_class, "getPackageManager", "()Landroid/content/pm/PackageManager;");
    jobject pm = env->CallObjectMethod(context, get_package_manager);

    jmethodID get_package_name = env->GetMethodID(context_class, "getPackageName", "()Ljava/lang/String;");
    jstring pkg_name_j = (jstring)env->CallObjectMethod(context, get_package_name);

    jclass pm_class = env->GetObjectClass(pm);
    jmethodID get_package_info = env->GetMethodID(pm_class, "getPackageInfo", "(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;");
    const jint GET_SIGNING_CERTIFICATES = 0x08000000;
    jobject package_info = env->CallObjectMethod(pm, get_package_info, pkg_name_j, GET_SIGNING_CERTIFICATES);

    jobjectArray signatures = nullptr;
    jclass pi_class = env->GetObjectClass(package_info);
    jfieldID signing_info_field = env->GetFieldID(pi_class, "signingInfo", "Landroid/content/pm/SigningInfo;");
    if (signing_info_field != nullptr) {
        jobject signing_info = env->GetObjectField(package_info, signing_info_field);
        if (signing_info != nullptr) {
            jclass si_class = env->GetObjectClass(signing_info);
            jmethodID get_apk_contents_signers = env->GetMethodID(si_class, "getApkContentsSigners", "()[Landroid/content/pm/Signature;");
            signatures = (jobjectArray)env->CallObjectMethod(signing_info, get_apk_contents_signers);
        }
    }

    if (signatures == nullptr) {
        const jint GET_SIGNATURES = 0x00000040;
        package_info = env->CallObjectMethod(pm, get_package_info, pkg_name_j, GET_SIGNATURES);
        pi_class = env->GetObjectClass(package_info);
        jfieldID signatures_field = env->GetFieldID(pi_class, "signatures", "[Landroid/content/pm/Signature;");
        signatures = (jobjectArray)env->GetObjectField(package_info, signatures_field);
    }

    if (signatures == nullptr || env->GetArrayLength(signatures) == 0) {
        LOGE("No signatures found");
        return env->NewStringUTF("ERROR: No signatures");
    }

    jobject signature = env->GetObjectArrayElement(signatures, 0);
    jclass sig_class = env->GetObjectClass(signature);
    jmethodID to_byte_array = env->GetMethodID(sig_class, "toByteArray", "()[B");
    jbyteArray sig_bytes = (jbyteArray)env->CallObjectMethod(signature, to_byte_array);

    jclass message_digest_class = env->FindClass("java/security/MessageDigest");
    jmethodID get_instance = env->GetStaticMethodID(message_digest_class, "getInstance", "(Ljava/lang/String;)Ljava/security/MessageDigest;");
    jstring algorithm = env->NewStringUTF("SHA-256");
    jobject md = env->CallStaticObjectMethod(message_digest_class, get_instance, algorithm);

    jmethodID update = env->GetMethodID(message_digest_class, "update", "([B)V");
    env->CallVoidMethod(md, update, sig_bytes);

    jmethodID digest = env->GetMethodID(message_digest_class, "digest", "()[B");
    jbyteArray hash_bytes = (jbyteArray)env->CallObjectMethod(md, digest);

    jbyte* hash = env->GetByteArrayElements(hash_bytes, nullptr);
    jsize hash_len2 = env->GetArrayLength(hash_bytes);

    char hex[65] = {0};
    const char hex_chars[] = "0123456789ABCDEF";
    for (int i = 0; i < hash_len2; ++i) {
        hex[i*2]   = hex_chars[(hash[i] >> 4) & 0x0F];
        hex[i*2+1] = hex_chars[hash[i] & 0x0F];
    }

    env->ReleaseByteArrayElements(sig_bytes, env->GetByteArrayElements(sig_bytes, nullptr), JNI_ABORT);
    env->ReleaseByteArrayElements(hash_bytes, hash, JNI_ABORT);

    std::string actual_hash(hex);
    LOGI("Actual hash  : %s", actual_hash.c_str());

    // Buat string hasil yang akan ditampilkan di dialog
    std::ostringstream oss;
    oss << "Expected: " << expected_hash << "\n"
        << "Actual  : " << actual_hash << "\n"
        << "Match   : " << (expected_hash == actual_hash ? "YES" : "NO");

    return env->NewStringUTF(oss.str().c_str());
}

} // extern "C"