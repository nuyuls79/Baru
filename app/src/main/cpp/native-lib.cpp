#include <jni.h>
#include <string>
#include <sys/ptrace.h>
#include <unistd.h>
#include <dlfcn.h>
#include <android/log.h>
#include <cstring>

#define LOG_TAG "NativeLib"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

void anti_debug() {
    ptrace(PTRACE_TRACEME, 0, 0, 0);
}

std::string decrypt(const char* encrypted, size_t len, char key) {
    std::string result(len, '\0');
    for (size_t i = 0; i < len; ++i) {
        result[i] = encrypted[i] ^ (key + i % 7);
    }
    return result;
}

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    anti_debug();
    return JNI_VERSION_1_6;
}

JNIEXPORT jboolean JNICALL
Java_com_lagradost_cloudstream3_utils_NativeSecurity_checkSignature(
        JNIEnv* env,
        jobject /* this */,
        jobject context) {

    // ========== TEMPEL HASIL XOR SCRIPT DI SINI ==========
    const char expected_hash_enc[] = {'\277'^0x5A, '\252'^0x5B, '\044'^0x5C, '\347'^0x5D, '\360'^0x5E, '\174'^0x5F, '\222'^0x60, '\203'^0x5A, '\132'^0x5B, '\060'^0x5C, '\237'^0x5D, '\242'^0x5E, '\257'^0x5F, '\121'^0x60, '\237'^0x5A, '\070'^0x5B, '\324'^0x5C, '\344'^0x5D, '\364'^0x5E, '\321'^0x5F, '\241'^0x60, '\264'^0x5A, '\104'^0x5B, '\047'^0x5C, '\253'^0x5D, '\160'^0x5E, '\243'^0x5F, '\332'^0x60, '\151'^0x5A, '\101'^0x5B, '\245'^0x5C, '\107'^0x5D};
    // =====================================================

    const size_t hash_len = sizeof(expected_hash_enc);
    const char xor_key = 0x5A;
    std::string expected_hash = decrypt(expected_hash_enc, hash_len, xor_key);

    jclass context_class = env->GetObjectClass(context);
    jmethodID get_package_manager = env->GetMethodID(
            context_class, "getPackageManager", "()Landroid/content/pm/PackageManager;");
    jobject pm = env->CallObjectMethod(context, get_package_manager);

    jmethodID get_package_name = env->GetMethodID(
            context_class, "getPackageName", "()Ljava/lang/String;");
    jstring pkg_name_j = (jstring)env->CallObjectMethod(context, get_package_name);

    jclass pm_class = env->GetObjectClass(pm);
    jmethodID get_package_info = env->GetMethodID(
            pm_class, "getPackageInfo",
            "(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;");

    // Coba API modern (SigningInfo) jika tersedia
    const jint GET_SIGNING_CERTIFICATES = 0x08000000;
    jobject package_info = env->CallObjectMethod(pm, get_package_info, pkg_name_j, GET_SIGNING_CERTIFICATES);

    jobjectArray signatures = nullptr;
    jclass pi_class = env->GetObjectClass(package_info);

    jfieldID signing_info_field = env->GetFieldID(pi_class, "signingInfo", "Landroid/content/pm/SigningInfo;");
    if (signing_info_field != nullptr) {
        jobject signing_info = env->GetObjectField(package_info, signing_info_field);
        if (signing_info != nullptr) {
            jclass si_class = env->GetObjectClass(signing_info);
            jmethodID get_apk_contents_signers = env->GetMethodID(
                    si_class, "getApkContentsSigners", "()[Landroid/content/pm/Signature;");
            signatures = (jobjectArray)env->CallObjectMethod(signing_info, get_apk_contents_signers);
        }
    }

    // Fallback ke GET_SIGNATURES
    if (signatures == nullptr) {
        const jint GET_SIGNATURES = 0x00000040;
        package_info = env->CallObjectMethod(pm, get_package_info, pkg_name_j, GET_SIGNATURES);
        pi_class = env->GetObjectClass(package_info);
        jfieldID signatures_field = env->GetFieldID(
                pi_class, "signatures", "[Landroid/content/pm/Signature;");
        signatures = (jobjectArray)env->GetObjectField(package_info, signatures_field);
    }

    if (signatures == nullptr || env->GetArrayLength(signatures) == 0) {
        LOGE("No signatures found!");
        volatile int* crash = nullptr;
        *crash = 0xDEAD;
        return JNI_FALSE;
    }

    jobject signature = env->GetObjectArrayElement(signatures, 0);
    jclass sig_class = env->GetObjectClass(signature);
    jmethodID to_byte_array = env->GetMethodID(sig_class, "toByteArray", "()[B");
    jbyteArray sig_bytes = (jbyteArray)env->CallObjectMethod(signature, to_byte_array);

    jclass message_digest_class = env->FindClass("java/security/MessageDigest");
    jmethodID get_instance = env->GetStaticMethodID(
            message_digest_class, "getInstance",
            "(Ljava/lang/String;)Ljava/security/MessageDigest;");
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
    if (expected_hash != actual_hash) {
        LOGE("SIGNATURE MISMATCH!");
        LOGE("Expected: %s", expected_hash.c_str());
        LOGE("Got     : %s", hex);
        volatile int* crash = nullptr;
        *crash = 0xDEAD;
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_lagradost_cloudstream3_utils_NativeSecurity_checkPackageName(
        JNIEnv* env,
        jobject /* this */,
        jobject context) {

    const char expected_pkg_enc[] = {
        'c'^0x3F, 'o'^0x3F, 'm'^0x40, '.'^0x41, 'x'^0x42, 's'^0x43, 't'^0x44, 'r'^0x45,
        'e'^0x46, 'a'^0x47, 'm'^0x48, '.'^0x49, 'a'^0x4A, 'p'^0x4B, 'p'^0x4C
    };
    const size_t len = sizeof(expected_pkg_enc);
    const char key = 0x3F;
    std::string expected_pkg = decrypt(expected_pkg_enc, len, key);

    jclass context_class = env->GetObjectClass(context);
    jmethodID get_package_name = env->GetMethodID(
            context_class, "getPackageName", "()Ljava/lang/String;");
    jstring pkg_name_j = (jstring)env->CallObjectMethod(context, get_package_name);

    const char* pkg_name_c = env->GetStringUTFChars(pkg_name_j, nullptr);
    bool is_valid = (expected_pkg == pkg_name_c);

    env->ReleaseStringUTFChars(pkg_name_j, pkg_name_c);

    if (!is_valid) {
        LOGE("PACKAGE NAME MISMATCH!");
        volatile int* crash = nullptr;
        *crash = 0xDEAD;
    }
    return is_valid ? JNI_TRUE : JNI_FALSE;
}

#ifdef __cplusplus
}
#endif