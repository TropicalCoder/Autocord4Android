#include <jni.h>
#include <android/input.h>
#include <android/log.h>
#include <android/bitmap.h>
#include "AudioEngine.h"
#ifdef DEBUG_LOGGING
#define  LOG_TAG    "AudioEngine"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#endif
static AudioEngine audioEngine;


extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv *env;
    vm->GetEnv((void**)&env, JNI_VERSION_1_6);
    jclass tmp = env->FindClass("com/tropicalcoder/autocord/MainActivity");
#ifdef DEBUG_LOGGING
    __android_log_print(ANDROID_LOG_DEBUG, "native-lib", "JNI_OnLoad");
#endif
    auto jniMainClass = (jclass)env->NewGlobalRef(tmp);
    AudioEngine::CacheCallbackParams(vm, jniMainClass, env);
    return  JNI_VERSION_1_6;
}

JNIEXPORT jboolean JNICALL
Java_com_tropicalcoder_autocord_MainActivity_checkExpired(
        JNIEnv *env,
        jobject clazz)
{
    return static_cast<jboolean>(AudioEngine::CheckExpired());
}


JNIEXPORT void JNICALL
Java_com_tropicalcoder_autocord_MainActivity_startEngine(
        JNIEnv *env,
        jclass clazz, float fPeakAmplitude)
{
    audioEngine.start(fPeakAmplitude);
}

JNIEXPORT float JNICALL
Java_com_tropicalcoder_autocord_MainActivity_stopEngine(
        JNIEnv *env,
        jclass clazz)
{
    return audioEngine.stop();
}

JNIEXPORT float JNICALL
Java_com_tropicalcoder_autocord_MainActivity_getGainApplied(
        JNIEnv *env,
        jclass clazz)
{
    return audioEngine.GetGainApplied();
}


JNIEXPORT float JNICALL
Java_com_tropicalcoder_autocord_MainActivity_getInputLevel(
        JNIEnv *env,
        jclass clazz)
{
    return audioEngine.GetSmoothedInputLevel();
}


JNIEXPORT void JNICALL
Java_com_tropicalcoder_autocord_MainActivity_setMode(
        JNIEnv *env,
        jobject clazz,
        jint nMode )
{
    audioEngine.SetMode(nMode);
}

JNIEXPORT void JNICALL
Java_com_tropicalcoder_autocord_MainActivity_setPause(
        JNIEnv *env,
        jobject clazz,
        jboolean bEnable )
{
    audioEngine.EnablePause(bEnable);
}

JNIEXPORT void JNICALL
Java_com_tropicalcoder_autocord_MainActivity_setPrefilter(
        JNIEnv *env,
        jobject clazz,
        jboolean bEnable )
{
    audioEngine.EnablePrefilter(bEnable);
}

JNIEXPORT void JNICALL
Java_com_tropicalcoder_autocord_MainActivity_setInputBoost(
        JNIEnv *env,
        jobject clazz,
        jboolean bEnable )
{
    audioEngine.EnableInputBoost(bEnable);
}

JNIEXPORT void JNICALL
Java_com_tropicalcoder_autocord_CanvasView_setTriggerDB(JNIEnv *env, jobject thiz, jfloat dB)
{
    audioEngine.SetTriggerDB(dB);
}


JNIEXPORT void JNICALL
Java_com_tropicalcoder_autocord_CanvasView_setTriggerFreqs(JNIEnv *env, jobject thiz, jfloat minFreq, jfloat maxFreq)
{
    audioEngine.SetTriggerFreqs(minFreq, maxFreq);
}



JNIEXPORT jfloat JNICALL
Java_com_tropicalcoder_autocord_CanvasView_getFrequencyAtGraphCoord(JNIEnv *env, jobject thiz, jint x)
{
    return audioEngine.GetMusicalNote(x);
}

JNIEXPORT int JNICALL
Java_com_tropicalcoder_autocord_CanvasView_getGraphCoordAtFrequency(JNIEnv *env, jobject thiz, jfloat f)
{
    return audioEngine.GetFrequencyMusicalNoteArrayOffset(f);
}

JNIEXPORT void JNICALL
Java_com_tropicalcoder_autocord_MainActivity_doFileIO(JNIEnv *env, jclass instance, jstring pathname, jint fileSizeBytes)
{
    const char *utf8 = env->GetStringUTFChars(pathname, nullptr);
    assert(nullptr != utf8);

    audioEngine.openFile(const_cast<char *>(utf8), fileSizeBytes);
    // release the Java string and UTF-8
    env->ReleaseStringUTFChars(pathname, utf8);
}

JNIEXPORT void JNICALL
Java_com_tropicalcoder_autocord_MainActivity_passMediaPathname(JNIEnv *env, jobject instance, jstring pathname)
{
    const char *utf8 = env->GetStringUTFChars(pathname, nullptr);
    assert(nullptr != utf8);

    memset(audioEngine.MediaPathname, 0, MEDIAPATHNAMESIZE);
    strcpy(audioEngine.MediaPathname,  const_cast<char *>(utf8));

    // release the Java string and UTF-8
    env->ReleaseStringUTFChars(pathname, utf8);
}

JNIEXPORT jstring JNICALL
Java_com_tropicalcoder_autocord_MainActivity_getTimeStr(JNIEnv *env, jobject instance)
{
    char *ptext = audioEngine.getTimeStr();
    return env->NewStringUTF(ptext);
}

JNIEXPORT void JNICALL
Java_com_tropicalcoder_autocord_MainActivity_passEncodeBuffer(JNIEnv *env, jobject instance, jobject buffer)
{
    void* bufPtr = env->GetDirectBufferAddress(buffer);
    jlong bufLength = env->GetDirectBufferCapacity(buffer);
/*
    if(bufPtr == nullptr)
    {
        __android_log_print(ANDROID_LOG_VERBOSE, "passEncodeBuffer", "bufPtr is NULL!");
    } else
    {
        __android_log_print(ANDROID_LOG_VERBOSE, "passEncodeBuffer", "bufPtr appears to be valid!");
    }
    __android_log_print(ANDROID_LOG_VERBOSE, "passEncodeBuffer", "bufLength: %d", (int)bufLength);
*/
    audioEngine.passEncodeBuffer((__uint8_t*)bufPtr, (int)bufLength);
}

/*
JNIEXPORT void JNICALL
Java_com_tropicalcoder_autocord_MainActivity_touchEvent(JNIEnv *env, jobject obj, jint action) {
    switch (action) {
        case AMOTION_EVENT_ACTION_DOWN:
            __android_log_write(ANDROID_LOG_VERBOSE, "MainActivity_touchEvent", "ACTION_DOWN");
            break;
        case AMOTION_EVENT_ACTION_UP:
            __android_log_write(ANDROID_LOG_VERBOSE, "MainActivity_touchEvent", "ACTION_UP");
            break;
        default:
            break;
    }
}
*/


JNIEXPORT jint JNICALL
Java_com_tropicalcoder_autocord_MainActivity_processData(
        JNIEnv *env,
        jclass clazz, jboolean bFetchPeak)
{
    return static_cast<jint>(audioEngine.ProcessData(bFetchPeak));
}

JNIEXPORT jstring JNICALL
Java_com_tropicalcoder_autocord_MainActivity_getPeakInfo(JNIEnv *env, jobject clazz)
{
    char *ptext = audioEngine.GetPeakInfo();
    return env->NewStringUTF(ptext);
}

JNIEXPORT jstring JNICALL
Java_com_tropicalcoder_autocord_MainActivity_getMusicalNote(JNIEnv *env, jobject clazz)
{
    char *ptext = audioEngine.GetMusicalNote();
    return env->NewStringUTF(ptext);
}

JNIEXPORT int JNICALL
Java_com_tropicalcoder_autocord_MainActivity_getTotalEventsWrittenToLog(
        JNIEnv *env,
        jobject clazz)
{
    return audioEngine.GetTotalEventsWrittenToLog();
}


JNIEXPORT jfloat JNICALL
Java_com_tropicalcoder_autocord_CanvasView_renderGraph(JNIEnv *env, jobject obj, jobject bitmap)
{
    AndroidBitmapInfo  info;
    void*              pixels;
    int                ret;

    if ((ret = AndroidBitmap_getInfo(env, bitmap, &info)) < 0) {
#ifdef DEBUG_LOGGING
        LOGE("AndroidBitmap_getInfo() failed ! error=%d", ret);
#endif
        return 0;
    }

    if (info.format != ANDROID_BITMAP_FORMAT_RGB_565) {
#ifdef DEBUG_LOGGING
        LOGE("Bitmap format is not RGB_565 !");
#endif
        return 0;
    }

    if ((ret = AndroidBitmap_lockPixels(env, bitmap, &pixels)) < 0) {
#ifdef DEBUG_LOGGING
        LOGE("AndroidBitmap_lockPixels() failed ! error=%d", ret);
#endif
    }

    float dB = audioEngine.RenderGraph(&info, pixels);

    AndroidBitmap_unlockPixels(env, bitmap);
    return dB;
    // LOGE("x: %d, y: %d, stride: %d", info.width, info.height, info.stride);
}

JNIEXPORT void JNICALL
Java_com_tropicalcoder_autocord_CanvasView_clearBitmap(JNIEnv *env, jobject obj, jobject bitmap)
{
    AndroidBitmapInfo  info;
    void*              pixels;
    int                ret;

    if ((ret = AndroidBitmap_getInfo(env, bitmap, &info)) < 0) {
#ifdef DEBUG_LOGGING
        LOGE("AndroidBitmap_getInfo() failed ! error=%d", ret);
#endif
        return;
    }


    if ((ret = AndroidBitmap_lockPixels(env, bitmap, &pixels)) < 0) {
#ifdef DEBUG_LOGGING
        LOGE("AndroidBitmap_lockPixels() failed ! error=%d", ret);
#endif
    }

    audioEngine.EraseGraph(&info, pixels);

    AndroidBitmap_unlockPixels(env, bitmap);
#ifdef DEBUG_LOGGING
    LOGE("x: %d, y: %d, stride: %d", info.width, info.height, info.stride);
#endif
}

} // End extern "C"
/*
extern "C"
JNIEXPORT void JNICALL
Java_com_tropicalcoder_autocord_MainActivity_passMediaPathnameFileDescriptor(JNIEnv *env,
                                                                              jobject thiz, jint fd)
{
    audioEngine.PassMediaPathnameFileDescriptor(fd);
}
*/extern "C"
JNIEXPORT jboolean JNICALL
Java_com_tropicalcoder_autocord_AudioEncoder_prepareEncodeBuffer(JNIEnv *env, jobject thiz)
{
    return static_cast<jboolean>(audioEngine.PrepareEncodeBuffer());
}