#include <jni.h>
#include <Superpowered.h>
#include <SuperpoweredAndroidUSB.h>
#include "AmpModel.h"
#include <atomic>
#include <algorithm>
#include <cmath>
#include <mutex>

// Distinct USB Host engine. This file never uses AAudio or AudioTrack.
// Android UsbManager owns the physical USB fd; stopIO/onDisconnect happens
// BEFORE Java closes the fd. No raw USB or middleware stream fallback.
namespace {
std::once_flag initialized;
std::mutex lifecycle;
std::atomic<bool> running{false};
std::atomic<int> callbacks{0}, frames{0}, badLayout{0},
    inputPeak{0}, outputPeak{0};
std::atomic<float> controls[9];
std::atomic<int> channel{1}, cabinet{0}, bypass{0};
int connectedDevice=-1;
bool connected=false;
AmpModel model;
bool resetDsp=true;

bool processAudio(void *,int,float *data,int numberOfFrames,int sampleRate,
                  int inputChannels,int outputChannels) {
  if(!data || numberOfFrames<=0)return false; // NULL is the final callback.
  if(inputChannels!=2 || outputChannels!=2){
    badLayout.fetch_add(1,std::memory_order_relaxed);
    for(int k=0;k<numberOfFrames*std::max(0,outputChannels);k++)data[k]=0;
    return false;
  }
  if(resetDsp){
    model.setSampleRate(sampleRate);
    model.reset();
    resetDsp=false;
  }
  AmpModel::Controls p;
  p.gain=controls[0].load(std::memory_order_relaxed);
  p.volume=controls[1].load(std::memory_order_relaxed);
  p.bass=controls[2].load(std::memory_order_relaxed);
  p.treble=controls[3].load(std::memory_order_relaxed);
  p.cut=controls[4].load(std::memory_order_relaxed);
  p.master=controls[5].load(std::memory_order_relaxed);
  p.spring=controls[6].load(std::memory_order_relaxed);
  p.tremSpeed=controls[7].load(std::memory_order_relaxed);
  p.tremDepth=controls[8].load(std::memory_order_relaxed);
  p.channel=channel.load(std::memory_order_relaxed);
  p.cabinet=cabinet.load(std::memory_order_relaxed);
  p.bypass=bypass.load(std::memory_order_relaxed);
  model.setControls(p);
  int maxInput=0,maxOutput=0;
  for(int i=0;i<numberOfFrames;i++){
    // Stereo input/output: overwrite one completed frame without affecting
    // any future input frame. This is the iRig Pro Duo's reported layout.
    const float source=data[i*2];
    const float sample=model.process(source);
    data[i*2]=sample;
    data[i*2+1]=sample;
    maxInput=std::max(maxInput,(int)(std::fabs(source)*1000.0f));
    maxOutput=std::max(maxOutput,(int)(std::fabs(sample)*1000.0f));
  }
  inputPeak.store(maxInput,std::memory_order_relaxed);
  outputPeak.store(maxOutput,std::memory_order_relaxed);
  frames.fetch_add(numberOfFrames,std::memory_order_relaxed);
  callbacks.fetch_add(1,std::memory_order_relaxed);
  return true;
}
void stopLocked(){
  if(connected){
    Superpowered::AndroidUSBAudio::stopIO(connectedDevice);
    Superpowered::AndroidUSB::onDisconnect(connectedDevice);
    connected=false;
    connectedDevice=-1;
  }
  running.store(false,std::memory_order_release);
}
}
extern "C" JNIEXPORT jint JNICALL
Java_com_analoglav_amps_usb_SuperpoweredUsbEngine_nativeStart(
    JNIEnv* env,jclass,jint id,jint fd,jbyteArray raw,jint index) {
  if(fd<0||raw==nullptr)return -3;
  std::lock_guard<std::mutex> lock(lifecycle);
  stopLocked();
  std::call_once(initialized,[](){
    Superpowered::Initialize("ExampleLicenseKey-WillExpire-OnNextUpdate");
    Superpowered::AndroidUSB::initialize(nullptr,nullptr,nullptr,nullptr,nullptr);
  });
  const jsize length=env->GetArrayLength(raw);
  if(length<18)return -4;
  jbyte *bytes=env->GetByteArrayElements(raw,nullptr);
  if(!bytes)return -5;
  auto result=Superpowered::AndroidUSB::onConnect(id,fd,
      reinterpret_cast<const unsigned char*>(bytes),length);
  env->ReleaseByteArrayElements(raw,bytes,JNI_ABORT);
  if(!(static_cast<int>(result)&1)){
    Superpowered::AndroidUSB::onDisconnect(id);
    return -6;
  }
  connectedDevice=id;
  connected=true;
  resetDsp=true;
  callbacks.store(0);frames.store(0);badLayout.store(0);
  inputPeak.store(0);outputPeak.store(0);
  Superpowered::AndroidUSBAudioBufferSize setting=
      index<=0?Superpowered::AndroidUSBAudioBufferSize_VeryLow:
      index==1?Superpowered::AndroidUSBAudioBufferSize_Low:
               Superpowered::AndroidUSBAudioBufferSize_Mid;
  // Request verified iRig UAC1 format: 48kHz / 24-bit / stereo in+out.
  // No automatic AAudio/OpenSL ES fallback.
  if(!Superpowered::AndroidUSBAudio::easyIO(
      id,48000,24,2,2,setting,nullptr,processAudio)){
    stopLocked();
    return -7;
  }
  running.store(true,std::memory_order_release);
  return 0;
}
extern "C" JNIEXPORT void JNICALL
Java_com_analoglav_amps_usb_SuperpoweredUsbEngine_nativeStop(JNIEnv*,jclass){
  std::lock_guard<std::mutex> lock(lifecycle);
  stopLocked();
}
extern "C" JNIEXPORT void JNICALL
Java_com_analoglav_amps_usb_SuperpoweredUsbEngine_nativeControls(
    JNIEnv* env,jclass,jfloatArray params,jint ch,jint cab,jint by){
  if(!params || env->GetArrayLength(params)<9)return;
  jfloat values[9];
  env->GetFloatArrayRegion(params,0,9,values);
  for(int i=0;i<9;i++)
    controls[i].store(std::max(0.0f,std::min(1.0f,values[i])),
                      std::memory_order_relaxed);
  channel.store(ch,std::memory_order_relaxed);
  cabinet.store(cab,std::memory_order_relaxed);
  bypass.store(by,std::memory_order_relaxed);
}
extern "C" JNIEXPORT jintArray JNICALL
Java_com_analoglav_amps_usb_SuperpoweredUsbEngine_nativeCounters(
    JNIEnv* env,jclass){
  jint numbers[6]={running.load()?1:0,callbacks.load(),frames.load(),
      badLayout.load(),inputPeak.load(),outputPeak.load()};
  jintArray result=env->NewIntArray(6);
  if(result)env->SetIntArrayRegion(result,0,6,numbers);
  return result;
}
