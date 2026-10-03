#include <jni.h>
#include <string>
#include "AudioEngine.h"

extern "C" JNIEXPORT jboolean JNICALL Java_com_example_epsampler_NativeEngine_start(JNIEnv*,jclass){return AudioEngine::instance().start();}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_stop(JNIEnv*,jclass){AudioEngine::instance().stop();}
extern "C" JNIEXPORT jboolean JNICALL Java_com_example_epsampler_NativeEngine_loadBank(JNIEnv* env,jclass,jstring path){
    const char* p=env->GetStringUTFChars(path,nullptr); bool ok=AudioEngine::instance().loadBank(p?p:""); env->ReleaseStringUTFChars(path,p); return ok;
}
extern "C" JNIEXPORT jboolean JNICALL Java_com_example_epsampler_NativeEngine_isBankLoaded(JNIEnv*,jclass){return AudioEngine::instance().bankLoaded();}
extern "C" JNIEXPORT jstring JNICALL Java_com_example_epsampler_NativeEngine_bankStatus(JNIEnv* env,jclass){auto s=AudioEngine::instance().bankStatus();return env->NewStringUTF(s.c_str());}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_noteOn(JNIEnv*,jclass,jint n,jint v){AudioEngine::instance().noteOn(n,v);}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_noteOff(JNIEnv*,jclass,jint n,jint v){AudioEngine::instance().noteOff(n,v);}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_polyPressure(JNIEnv*,jclass,jint n,jint p){AudioEngine::instance().polyPressure(n,p);}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_channelPressure(JNIEnv*,jclass,jint p){AudioEngine::instance().channelPressure(p);}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_controlChange(JNIEnv*,jclass,jint c,jint v){AudioEngine::instance().controlChange(c,v);}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_pitchBend(JNIEnv*,jclass,jint v){AudioEngine::instance().pitchBend(v);}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_setDreamy(JNIEnv*,jclass,jboolean on){AudioEngine::instance().setDreamy(on);}
