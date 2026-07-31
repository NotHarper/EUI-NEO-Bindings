#include <jni.h>

#include "eui/neo_c_api.h"

#include <cstdint>
#include <cstring>
#include <string>

namespace {

void throwException(JNIEnv* env, const char* type, const std::string& message) {
    jclass clazz = env->FindClass(type);
    if (clazz != nullptr) env->ThrowNew(clazz, message.c_str());
}

bool validHandle(JNIEnv* env, jlong value) {
    if (value != 0) return true;
    throwException(env, "java/lang/IllegalStateException", "Invalid EUI-NEO engine handle.");
    return false;
}

std::string utf8(JNIEnv* env, jstring value) {
    if (value == nullptr) return {};
    const char* text = env->GetStringUTFChars(value, nullptr);
    if (text == nullptr) return {};
    std::string result(text);
    env->ReleaseStringUTFChars(value, text);
    return result;
}

void throwNeoException(JNIEnv* env, eui_neo_engine* engine, eui_neo_result code) {
    const char* message = engine ? eui_neo_last_error(engine) : nullptr;
    const char* msg = (message && *message) ? message : "EUI-NEO native call failed.";
    jclass clazz = env->FindClass("com/sudoevolve/euineo/NeoException");
    if (clazz == nullptr) return;
    jmethodID ctor = env->GetMethodID(clazz, "<init>", "(Ljava/lang/String;I)V");
    if (ctor == nullptr) return;
    jstring jmsg = env->NewStringUTF(msg);
    if (jmsg == nullptr) return;
    jobject ex = env->NewObject(clazz, ctor, jmsg, static_cast<jint>(code));
    env->DeleteLocalRef(jmsg);
    if (ex != nullptr) {
        env->Throw(static_cast<jthrowable>(ex));
        env->DeleteLocalRef(ex);
    }
}

void checkResult(JNIEnv* env, eui_neo_engine* engine, eui_neo_result result) {
    if (result == EUI_NEO_OK) return;
    if (result == EUI_NEO_INVALID_ARGUMENT) {
        const char* message = engine ? eui_neo_last_error(engine) : nullptr;
        throwException(env, "java/lang/IllegalArgumentException",
                       (message && *message) ? message : "Invalid argument.");
        return;
    }
    throwNeoException(env, engine, result);
}

} // namespace

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM*, void*) {
    return JNI_VERSION_1_6;
}

extern "C" {

JNIEXPORT jlong JNICALL Java_com_sudoevolve_euineo_NeoEngine_nativeCreate(
    JNIEnv* env, jclass, jstring title, jstring pageId, jstring uiJson, jint width, jint height,
    jdouble fps, jfloat red, jfloat green, jfloat blue, jfloat alpha, jboolean resizable, jboolean decorated) {
    eui_neo_config config;
    eui_neo_config_init(&config);
    const std::string titleText = utf8(env, title);
    const std::string pageText = utf8(env, pageId);
    const std::string jsonText = utf8(env, uiJson);
    config.title_utf8 = titleText.empty() ? "EUI-NEO Java" : titleText.c_str();
    config.page_id_utf8 = pageText.empty() ? "java" : pageText.c_str();
    config.ui_json_utf8 = jsonText.empty() ? nullptr : jsonText.c_str();
    config.width = width;
    config.height = height;
    config.frames_per_second = fps;
    config.clear_color_r = red;
    config.clear_color_g = green;
    config.clear_color_b = blue;
    config.clear_color_a = alpha;
    config.resizable = resizable == JNI_TRUE ? 1 : 0;
    config.decorated = decorated == JNI_TRUE ? 1 : 0;
    eui_neo_engine* engine = eui_neo_create(&config);
    if (engine == nullptr) throwException(env, "com/sudoevolve/euineo/NeoException", "Unable to create native engine.");
    return reinterpret_cast<jlong>(engine);
}

JNIEXPORT jint JNICALL Java_com_sudoevolve_euineo_NeoEngine_nativeInitialize(JNIEnv* env, jclass, jlong value) {
    if (!validHandle(env, value)) return EUI_NEO_INVALID_ARGUMENT;
    auto* engine = reinterpret_cast<eui_neo_engine*>(value);
    const eui_neo_result result = eui_neo_initialize(engine);
    checkResult(env, engine, result);
    return result;
}

JNIEXPORT jint JNICALL Java_com_sudoevolve_euineo_NeoEngine_nativePumpEvents(JNIEnv* env, jclass, jlong value, jint timeout) {
    if (!validHandle(env, value)) return EUI_NEO_INVALID_ARGUMENT;
    auto* engine = reinterpret_cast<eui_neo_engine*>(value);
    const eui_neo_result result = eui_neo_pump_events(engine, timeout);
    checkResult(env, engine, result);
    return result;
}

JNIEXPORT jlongArray JNICALL Java_com_sudoevolve_euineo_NeoEngine_nativeFrame(JNIEnv* env, jclass, jlong value) {
    if (!validHandle(env, value)) return nullptr;
    auto* engine = reinterpret_cast<eui_neo_engine*>(value);
    eui_neo_frame_info info;
    eui_neo_frame_info_init(&info);
    const eui_neo_result result = eui_neo_frame(engine, &info);
    checkResult(env, engine, result);
    if (result != EUI_NEO_OK) return nullptr;
    jlong values[6];
    uint32_t dpi_bits;
    std::memcpy(&dpi_bits, &info.dpi_scale, sizeof(dpi_bits));
    values[0] = static_cast<jlong>(info.frame_number);
    values[1] = static_cast<jlong>(info.framebuffer_width);
    values[2] = static_cast<jlong>(info.framebuffer_height);
    values[3] = static_cast<jlong>(dpi_bits);
    values[4] = static_cast<jlong>(info.rendered);
    values[5] = static_cast<jlong>(info.running);
    jlongArray output = env->NewLongArray(6);
    if (output != nullptr) env->SetLongArrayRegion(output, 0, 6, values);
    return output;
}

JNIEXPORT jint JNICALL Java_com_sudoevolve_euineo_NeoEngine_nativeSetUiJson(JNIEnv* env, jclass, jlong value, jstring json) {
    if (!validHandle(env, value)) return EUI_NEO_INVALID_ARGUMENT;
    if (json == nullptr) {
        throwException(env, "java/lang/IllegalArgumentException", "json must not be null.");
        return EUI_NEO_INVALID_ARGUMENT;
    }
    auto* engine = reinterpret_cast<eui_neo_engine*>(value);
    const std::string text = utf8(env, json);
    const eui_neo_result result = eui_neo_set_ui_json(engine, text.c_str());
    checkResult(env, engine, result);
    return result;
}

JNIEXPORT jint JNICALL Java_com_sudoevolve_euineo_NeoEngine_nativeRequestUpdate(JNIEnv* env, jclass, jlong value) {
    if (!validHandle(env, value)) return EUI_NEO_INVALID_ARGUMENT;
    auto* engine = reinterpret_cast<eui_neo_engine*>(value);
    const eui_neo_result result = eui_neo_request_update(engine);
    checkResult(env, engine, result);
    return result;
}

JNIEXPORT jint JNICALL Java_com_sudoevolve_euineo_NeoEngine_nativeIsRunning(JNIEnv* env, jclass, jlong value) {
    if (!validHandle(env, value)) return 0;
    return eui_neo_is_running(reinterpret_cast<eui_neo_engine*>(value));
}

JNIEXPORT jint JNICALL Java_com_sudoevolve_euineo_NeoEngine_nativeShutdown(JNIEnv* env, jclass, jlong value) {
    if (!validHandle(env, value)) return EUI_NEO_INVALID_ARGUMENT;
    auto* engine = reinterpret_cast<eui_neo_engine*>(value);
    const eui_neo_result result = eui_neo_shutdown(engine);
    checkResult(env, engine, result);
    return result;
}

JNIEXPORT void JNICALL Java_com_sudoevolve_euineo_NeoEngine_nativeDestroy(JNIEnv*, jclass, jlong value) {
    if (value != 0) eui_neo_destroy(reinterpret_cast<eui_neo_engine*>(value));
}

JNIEXPORT jstring JNICALL Java_com_sudoevolve_euineo_NeoEngine_nativeLastError(JNIEnv* env, jclass, jlong value) {
    if (!validHandle(env, value)) return nullptr;
    return env->NewStringUTF(eui_neo_last_error(reinterpret_cast<eui_neo_engine*>(value)));
}

JNIEXPORT jstring JNICALL Java_com_sudoevolve_euineo_NeoEngine_nativeVersion(JNIEnv* env, jclass) {
    return env->NewStringUTF(eui_neo_version_string());
}

JNIEXPORT jlongArray JNICALL Java_com_sudoevolve_euineo_NeoEngine_nativePollEvent(JNIEnv* env, jclass, jlong value) {
    if (!validHandle(env, value)) return nullptr;
    auto* engine = reinterpret_cast<eui_neo_engine*>(value);
    eui_neo_event evt;
    eui_neo_event_init(&evt);
    const eui_neo_result result = eui_neo_poll_event(engine, &evt);
    if (result != EUI_NEO_OK) { checkResult(env, engine, result); return nullptr; }
    jlong out[7];
    uint32_t xb, yb, dxb, dyb;
    std::memcpy(&xb,  &evt.x,       sizeof(xb));
    std::memcpy(&yb,  &evt.y,       sizeof(yb));
    std::memcpy(&dxb, &evt.delta_x, sizeof(dxb));
    std::memcpy(&dyb, &evt.delta_y, sizeof(dyb));
    out[0] = static_cast<jlong>(evt.type);
    out[1] = static_cast<jlong>(xb);
    out[2] = static_cast<jlong>(yb);
    out[3] = static_cast<jlong>(dxb);
    out[4] = static_cast<jlong>(dyb);
    out[5] = 0;
    out[6] = 0;
    jlongArray arr = env->NewLongArray(7);
    if (arr != nullptr) env->SetLongArrayRegion(arr, 0, 7, out);
    return arr;
}

JNIEXPORT jstring JNICALL Java_com_sudoevolve_euineo_NeoEngine_nativeLastEventHandlerId(JNIEnv* env, jclass, jlong value) {
    if (!validHandle(env, value)) return nullptr;
    auto* engine = reinterpret_cast<eui_neo_engine*>(value);
    return env->NewStringUTF(eui_neo_last_event_handler_id(engine));
}

JNIEXPORT jstring JNICALL Java_com_sudoevolve_euineo_NeoEngine_nativeLastEventTextInput(JNIEnv* env, jclass, jlong value) {
    if (!validHandle(env, value)) return nullptr;
    auto* engine = reinterpret_cast<eui_neo_engine*>(value);
    return env->NewStringUTF(eui_neo_last_event_text_input(engine));
}

JNIEXPORT jint JNICALL Java_com_sudoevolve_euineo_NeoEngine_nativeSetWindowTitle(JNIEnv* env, jclass, jlong value, jstring title) {
    if (!validHandle(env, value)) return EUI_NEO_INVALID_ARGUMENT;
    auto* engine = reinterpret_cast<eui_neo_engine*>(value);
    const std::string text = utf8(env, title);
    const eui_neo_result result = eui_neo_set_window_title(engine, text.c_str());
    checkResult(env, engine, result);
    return result;
}

JNIEXPORT jint JNICALL Java_com_sudoevolve_euineo_NeoEngine_nativeSetWindowSize(JNIEnv* env, jclass, jlong value, jint w, jint h) {
    if (!validHandle(env, value)) return EUI_NEO_INVALID_ARGUMENT;
    auto* engine = reinterpret_cast<eui_neo_engine*>(value);
    const eui_neo_result result = eui_neo_set_window_size(engine, w, h);
    checkResult(env, engine, result);
    return result;
}

JNIEXPORT jint JNICALL Java_com_sudoevolve_euineo_NeoEngine_nativeApiVersion(JNIEnv*, jclass) {
    return static_cast<jint>(eui_neo_api_version());
}

} // extern "C"
