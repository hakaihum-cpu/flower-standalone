#include <jni.h>
#include <string>
#include "AudioEngine.h"

extern "C" JNIEXPORT jboolean JNICALL Java_com_example_epsampler_NativeEngine_start(JNIEnv*,jclass){return AudioEngine::instance().start();}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_stop(JNIEnv*,jclass){AudioEngine::instance().stop();}
extern "C" JNIEXPORT jboolean JNICALL Java_com_example_epsampler_NativeEngine_loadBank(JNIEnv* env,jclass,jstring path){
    const char* p=env->GetStringUTFChars(path,nullptr); bool ok=AudioEngine::instance().loadBank(p?p:""); env->ReleaseStringUTFChars(path,p); return ok;
}
extern "C" JNIEXPORT jboolean JNICALL Java_com_example_epsampler_NativeEngine_loadBankFd(JNIEnv*,jclass,jint fd){return AudioEngine::instance().loadBankFd(fd);}
extern "C" JNIEXPORT jboolean JNICALL Java_com_example_epsampler_NativeEngine_isBankLoaded(JNIEnv*,jclass){return AudioEngine::instance().bankLoaded();}
extern "C" JNIEXPORT jstring JNICALL Java_com_example_epsampler_NativeEngine_bankStatus(JNIEnv* env,jclass){auto s=AudioEngine::instance().bankStatus();return env->NewStringUTF(s.c_str());}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_noteOn(JNIEnv*,jclass,jint n,jint v){AudioEngine::instance().noteOn(n,v);}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_noteOff(JNIEnv*,jclass,jint n,jint v){AudioEngine::instance().noteOff(n,v);}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_polyPressure(JNIEnv*,jclass,jint n,jint p){AudioEngine::instance().polyPressure(n,p);}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_channelPressure(JNIEnv*,jclass,jint p){AudioEngine::instance().channelPressure(p);}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_controlChange(JNIEnv*,jclass,jint c,jint v){AudioEngine::instance().controlChange(c,v);}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_pitchBend(JNIEnv*,jclass,jint v){AudioEngine::instance().pitchBend(v);}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_setDreamy(JNIEnv*,jclass,jboolean on){AudioEngine::instance().setDreamy(on);}

extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_setBoosterStep(JNIEnv*,jclass,jint step){AudioEngine::instance().setBoosterStep(step);}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_setSpaceMode(JNIEnv*,jclass,jint mode){AudioEngine::instance().setSpaceMode(mode);}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_setTape(JNIEnv*,jclass,jboolean on){AudioEngine::instance().setTape(on);}

extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_setBoostDb(JNIEnv*,jclass,jint db){AudioEngine::instance().setBoostDb(db);}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_setSpaceParameters(JNIEnv*,jclass,jint mix,jint decay){AudioEngine::instance().setSpaceParameters(mix,decay);}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_setTapeParameters(JNIEnv*,jclass,jint wow,jint flutter,jint drive){AudioEngine::instance().setTapeParameters(wow,flutter,drive);}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_setDreamyParameters(JNIEnv*,jclass,jint x,jint y,jint mix){AudioEngine::instance().setDreamyParameters(x,y,mix);}

extern "C" JNIEXPORT jint JNICALL Java_com_example_epsampler_NativeEngine_setAudioBufferBursts(JNIEnv*,jclass,jfloat bursts){return AudioEngine::instance().setAudioBufferBursts(bursts);}
extern "C" JNIEXPORT jint JNICALL Java_com_example_epsampler_NativeEngine_audioFramesPerBurst(JNIEnv*,jclass){return AudioEngine::instance().audioFramesPerBurst();}
extern "C" JNIEXPORT jint JNICALL Java_com_example_epsampler_NativeEngine_audioBufferSizeFrames(JNIEnv*,jclass){return AudioEngine::instance().audioBufferSizeFrames();}
extern "C" JNIEXPORT jint JNICALL Java_com_example_epsampler_NativeEngine_audioBufferCapacityFrames(JNIEnv*,jclass){return AudioEngine::instance().audioBufferCapacityFrames();}
extern "C" JNIEXPORT jint JNICALL Java_com_example_epsampler_NativeEngine_audioXRunCount(JNIEnv*,jclass){return AudioEngine::instance().audioXRunCount();}

extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_recorderToggleRecording(JNIEnv*,jclass){AudioEngine::instance().recorderToggleRecording();}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_recorderToggleRandom(JNIEnv*,jclass){AudioEngine::instance().recorderToggleRandom();}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_recorderClear(JNIEnv*,jclass){AudioEngine::instance().recorderClear();}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_recorderPlaySlot(JNIEnv*,jclass,jint slot){AudioEngine::instance().recorderPlaySlot(slot);}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_recorderRecordSlot(JNIEnv*,jclass,jint slot){AudioEngine::instance().recorderRecordSlot(slot);}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_recorderToggleClock(JNIEnv*,jclass){AudioEngine::instance().recorderToggleClock();}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_recorderSetBpm(JNIEnv*,jclass,jint bpm){AudioEngine::instance().recorderSetBpm(bpm);}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_recorderMidiRealtime(JNIEnv*,jclass,jint status){AudioEngine::instance().recorderMidiRealtime(status);}
extern "C" JNIEXPORT jboolean JNICALL Java_com_example_epsampler_NativeEngine_recorderIsRecording(JNIEnv*,jclass){return AudioEngine::instance().recorderRecording();}
extern "C" JNIEXPORT jboolean JNICALL Java_com_example_epsampler_NativeEngine_recorderIsRandom(JNIEnv*,jclass){return AudioEngine::instance().recorderRandom();}
extern "C" JNIEXPORT jboolean JNICALL Java_com_example_epsampler_NativeEngine_recorderIsMidiClock(JNIEnv*,jclass){return AudioEngine::instance().recorderMidiClock();}
extern "C" JNIEXPORT jint JNICALL Java_com_example_epsampler_NativeEngine_recorderBpm(JNIEnv*,jclass){return AudioEngine::instance().recorderBpm();}
extern "C" JNIEXPORT jint JNICALL Java_com_example_epsampler_NativeEngine_recorderRecordingSlot(JNIEnv*,jclass){return AudioEngine::instance().recorderRecordingSlot();}
extern "C" JNIEXPORT jboolean JNICALL Java_com_example_epsampler_NativeEngine_recorderSlotPlaying(JNIEnv*,jclass,jint slot){return AudioEngine::instance().recorderSlotPlaying(slot);}
extern "C" JNIEXPORT jfloat JNICALL Java_com_example_epsampler_NativeEngine_recorderSlotProgress(JNIEnv*,jclass,jint slot){return AudioEngine::instance().recorderSlotProgress(slot);}
extern "C" JNIEXPORT jint JNICALL Java_com_example_epsampler_NativeEngine_recorderValidSamples(JNIEnv*,jclass,jint slot){return AudioEngine::instance().recorderValidSamples(slot);}
extern "C" JNIEXPORT jfloatArray JNICALL Java_com_example_epsampler_NativeEngine_recorderPeaks(JNIEnv* env,jclass,jint slot){
    constexpr int bins=IntegratedRecorder::kPeakBins;
    jfloatArray out=env->NewFloatArray(bins);
    if(!out) return nullptr;
    jfloat values[bins];
    for(int i=0;i<bins;i++) values[i]=AudioEngine::instance().recorderPeak(slot,i);
    env->SetFloatArrayRegion(out,0,bins,values);
    return out;
}
extern "C" JNIEXPORT jfloat JNICALL Java_com_example_epsampler_NativeEngine_recorderInputLevel(JNIEnv*,jclass){return AudioEngine::instance().recorderInputLevel();}
