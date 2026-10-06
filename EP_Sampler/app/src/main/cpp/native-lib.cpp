#include <jni.h>
#include <string>
#include "AudioEngine.h"

extern "C" JNIEXPORT jboolean JNICALL Java_com_example_epsampler_NativeEngine_start(JNIEnv*,jclass){return AudioEngine::instance().start();}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_stop(JNIEnv*,jclass){AudioEngine::instance().stop();}
extern "C" JNIEXPORT jboolean JNICALL Java_com_example_epsampler_NativeEngine_restartAudioPreservingState(JNIEnv*,jclass){return AudioEngine::instance().restartAudioPreservingState();}
extern "C" JNIEXPORT jboolean JNICALL Java_com_example_epsampler_NativeEngine_loadBank(JNIEnv* env,jclass,jstring path){
    const char* p=env->GetStringUTFChars(path,nullptr); bool ok=AudioEngine::instance().loadBank(p?p:""); env->ReleaseStringUTFChars(path,p); return ok;
}
extern "C" JNIEXPORT jboolean JNICALL Java_com_example_epsampler_NativeEngine_loadBankFd(JNIEnv*,jclass,jint fd){return AudioEngine::instance().loadBankFd(fd);}
extern "C" JNIEXPORT jboolean JNICALL Java_com_example_epsampler_NativeEngine_isBankLoaded(JNIEnv*,jclass){return AudioEngine::instance().bankLoaded();}
extern "C" JNIEXPORT jstring JNICALL Java_com_example_epsampler_NativeEngine_bankStatus(JNIEnv* env,jclass){auto s=AudioEngine::instance().bankStatus();return env->NewStringUTF(s.c_str());}
extern "C" JNIEXPORT jboolean JNICALL Java_com_example_epsampler_NativeEngine_loadBankSlotFd(JNIEnv*,jclass,jint slot,jint fd){return AudioEngine::instance().loadBankSlotFd(slot,fd);}
extern "C" JNIEXPORT jboolean JNICALL Java_com_example_epsampler_NativeEngine_isBankSlotLoaded(JNIEnv*,jclass,jint slot){return AudioEngine::instance().bankLoaded(slot);}
extern "C" JNIEXPORT jstring JNICALL Java_com_example_epsampler_NativeEngine_bankSlotStatus(JNIEnv* env,jclass,jint slot){auto s=AudioEngine::instance().bankStatus(slot);return env->NewStringUTF(s.c_str());}
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
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_setDreamyMode(JNIEnv*,jclass,jint mode){AudioEngine::instance().setDreamyMode(mode);}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_setDreamyExtraParameters(JNIEnv*,jclass,jint p3,jint p4){AudioEngine::instance().setDreamyExtraParameters(p3,p4);}

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

extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_setAdsr(JNIEnv*,jclass,jint a,jint d,jint s,jint r){AudioEngine::instance().setAdsr(a,d,s,r);}

extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_setInstrument(JNIEnv*,jclass,jint instrument){AudioEngine::instance().setInstrument(instrument);}

extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_noteOnPart(JNIEnv*,jclass,jint part,jint n,jint v){AudioEngine::instance().noteOnPart(part,n,v);}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_noteOffPart(JNIEnv*,jclass,jint part,jint n,jint v){AudioEngine::instance().noteOffPart(part,n,v);}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_polyPressurePart(JNIEnv*,jclass,jint part,jint n,jint p){AudioEngine::instance().polyPressurePart(part,n,p);}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_channelPressurePart(JNIEnv*,jclass,jint part,jint p){AudioEngine::instance().channelPressurePart(part,p);}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_controlChangePart(JNIEnv*,jclass,jint part,jint c,jint v){AudioEngine::instance().controlChangePart(part,c,v);}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_pitchBendPart(JNIEnv*,jclass,jint part,jint v){AudioEngine::instance().pitchBendPart(part,v);}

extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_setDrumParameter(JNIEnv*,jclass,jint parameter,jint value){AudioEngine::instance().setDrumParameter(parameter,value);}

extern "C" JNIEXPORT jboolean JNICALL Java_com_example_epsampler_NativeEngine_loadDrumSample(
        JNIEnv* env, jclass, jint slot, jbyteArray bytes) {
    if (!bytes) return JNI_FALSE;
    const jsize size = env->GetArrayLength(bytes);
    if (size <= 0) return JNI_FALSE;
    jbyte* data = env->GetByteArrayElements(bytes, nullptr);
    if (!data) return JNI_FALSE;
    const bool ok = AudioEngine::instance().loadDrumSample(
            slot,
            reinterpret_cast<const uint8_t*>(data),
            static_cast<size_t>(size));
    env->ReleaseByteArrayElements(bytes, data, JNI_ABORT);
    return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_setDrumFx(
        JNIEnv*, jclass, jint boostDb, jint distortion) {
    AudioEngine::instance().setDrumFx(boostDb, distortion);
}

extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_setPartFx(
        JNIEnv*, jclass, jint part, jint boostDb, jint distortion) {
    AudioEngine::instance().setPartFx(part, boostDb, distortion);
}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_setPartMixer(
        JNIEnv*, jclass, jint part, jint volume, jint pan, jboolean muted) {
    AudioEngine::instance().setPartMixer(part, volume, pan, muted == JNI_TRUE);
}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_setDrumSampleMixer(
        JNIEnv*, jclass, jint slot, jint volume, jint pan, jboolean muted) {
    AudioEngine::instance().setDrumSampleMixer(slot, volume, pan, muted == JNI_TRUE);
}
extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_setFeltReverb(
        JNIEnv*, jclass, jint mix, jint decay) {
    AudioEngine::instance().setFeltReverb(mix, decay);
}

extern "C" JNIEXPORT void JNICALL Java_com_example_epsampler_NativeEngine_setPerformanceXY(
        JNIEnv*, jclass, jboolean active, jint part, jint x, jint y) {
    AudioEngine::instance().setPerformanceXY(active == JNI_TRUE, part, x, y);
}

extern "C" JNIEXPORT jint JNICALL Java_com_example_epsampler_NativeEngine_setAudioBufferBursts(
        JNIEnv*, jclass, jfloat bursts) {
    return AudioEngine::instance().setAudioBufferBursts(bursts);
}
extern "C" JNIEXPORT jint JNICALL Java_com_example_epsampler_NativeEngine_audioFramesPerBurst(
        JNIEnv*, jclass) {
    return AudioEngine::instance().audioFramesPerBurst();
}
extern "C" JNIEXPORT jint JNICALL Java_com_example_epsampler_NativeEngine_audioBufferSizeFrames(
        JNIEnv*, jclass) {
    return AudioEngine::instance().audioBufferSizeFrames();
}
extern "C" JNIEXPORT jint JNICALL Java_com_example_epsampler_NativeEngine_audioBufferCapacityFrames(
        JNIEnv*, jclass) {
    return AudioEngine::instance().audioBufferCapacityFrames();
}
extern "C" JNIEXPORT jint JNICALL Java_com_example_epsampler_NativeEngine_audioXRunCount(
        JNIEnv*, jclass) {
    return AudioEngine::instance().audioXRunCount();
}
extern "C" JNIEXPORT jfloat JNICALL Java_com_example_epsampler_NativeEngine_partMeter(
        JNIEnv*, jclass, jint part) {
    return AudioEngine::instance().partMeter(part);
}
