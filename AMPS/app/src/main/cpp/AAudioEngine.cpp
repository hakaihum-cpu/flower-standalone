#include <aaudio/AAudio.h>
#include <jni.h>
#include <atomic>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <iterator>
#include <thread>
#include <chrono>
#include <time.h>
#include "AmpModel.h"
#include "AudioFifo.h"

// Separate high-priority INPUT and OUTPUT callbacks connected by a bounded
// single-producer/single-consumer FIFO. No callback allocates, waits or locks.
class LowLatencyEngine {
public:
  static constexpr uint32_t CAPACITY=8192;
  static constexpr int ST_STOPPED=0, ST_RUNNING=1, ST_FAILED=2;
  int start(int requestedIn,int requestedOut,int burstSelection) {
    std::lock_guard<std::mutex> lock(mutex_);
    closeLocked();
    selectedIn_=requestedIn; selectedOut_=requestedOut;
    lastError_.store(0);
    fifo_.reset();
    inputPeak_.store(0); outputPeak_.store(0);
    maxCallbackUs_.store(0);
    maxInputFrames_.store(0);maxOutputFrames_.store(0);
    int result = openStream(AAUDIO_DIRECTION_OUTPUT, AAUDIO_SHARING_MODE_EXCLUSIVE,
                            requestedOut,0,&output_);
    if(result!=AAUDIO_OK) {
      result=openStream(AAUDIO_DIRECTION_OUTPUT, AAUDIO_SHARING_MODE_SHARED,
                        requestedOut,0,&output_);
    }
    if(result!=AAUDIO_OK) return failLocked(result);
    const int sampleRate=AAudioStream_getSampleRate(output_);
    const int32_t burst=AAudioStream_getFramesPerBurst(output_);
    if(sampleRate<16000||burst<1) return failLocked(-20001);
    model_.setSampleRate(sampleRate);
    model_.reset();
    result=openStream(AAUDIO_DIRECTION_INPUT, AAUDIO_SHARING_MODE_EXCLUSIVE,
                      requestedIn,sampleRate,&input_);
    if(result!=AAUDIO_OK) {
      result=openStream(AAUDIO_DIRECTION_INPUT, AAUDIO_SHARING_MODE_SHARED,
                        requestedIn,sampleRate,&input_);
    }
    if(result!=AAUDIO_OK) return failLocked(result);
    if(AAudioStream_getSampleRate(input_)!=sampleRate ||
       AAudioStream_getFormat(input_)!=AAUDIO_FORMAT_PCM_I16 ||
       AAudioStream_getChannelCount(input_)!=1 ||
       AAudioStream_getFormat(output_)!=AAUDIO_FORMAT_PCM_FLOAT ||
       AAudioStream_getChannelCount(output_)!=2) return failLocked(-20002);
    const float ratios[]={0.f,.5f,.75f,1.f,1.5f,2.f,3.f,4.f,5.f,6.f,7.f,8.f};
    if(burstSelection<0||burstSelection>=static_cast<int>(sizeof(ratios)/sizeof(ratios[0])))
      burstSelection=0;
    // AAudio may round/clamp this value. Query actual from the stream.
    const int reqFrames=std::max(1,(int)std::lround(
       burst*(burstSelection==0?2.0f:ratios[burstSelection])));
    const int actual=AAudioStream_setBufferSizeInFrames(output_,reqFrames);
    if(actual<0) return failLocked(actual);
    // Cache burst size once; output callback must not access control-plane
    // AAudio APIs. Keep priming shallow to avoid adding software latency.
    outputBurst_=std::max(32,burst);
    const int inputBurst=std::max(32,AAudioStream_getFramesPerBurst(input_));
    const int callbackFrames=AAudioStream_getFramesPerDataCallback(output_);
    // If AAudio publishes its callback size, prime at least that many
    // samples. One large callback cannot safely consume one tiny burst.
    const int required=callbackFrames>0?callbackFrames:outputBurst_;
    queueTarget_=std::max(128,std::min(2048,std::max(required,inputBurst)));
    // The input callback has its own native capture buffer; do not conflate
    // output buffer size with input buffer or full end-to-end latency.
    running_.store(true,std::memory_order_release);
    result=AAudioStream_requestStart(input_);
    if(result!=AAUDIO_OK)return failLocked(result);
    // At startup previously the output stream consumed a completely empty
    // FIFO. Prime one hardware callback before enabling the output callback.
    // This wait happens on the UI caller, never on either audio thread.
    for(int retry=0;retry<40 && fifo_.available()<
        static_cast<uint32_t>(queueTarget_);retry++)
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    result=AAudioStream_requestStart(output_);
    if(result!=AAUDIO_OK)return failLocked(result);
    state_.store(ST_RUNNING);
    return 0;
  }
  void stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    closeLocked();
    lastError_.store(0);
  }
  void setControls(const float* values,int channel,int cabinet,int bypass){
    for(int i=0;i<9;i++)params_[i].store(
      std::max(0.f,std::min(1.f,values[i])),std::memory_order_relaxed);
    channel_.store(channel);cabinet_.store(cabinet);bypass_.store(bypass);
  }
  void getStats(jint (&values)[28]){
    std::lock_guard<std::mutex> lock(mutex_);
    std::fill(std::begin(values),std::end(values),0);
    values[0]=state_.load();
    values[1]=input_?AAudioStream_getDeviceId(input_):0;
    values[2]=output_?AAudioStream_getDeviceId(output_):0;
    values[3]=input_?AAudioStream_getBufferSizeInFrames(input_):0;
    values[4]=output_?AAudioStream_getBufferSizeInFrames(output_):0;
    values[5]=output_?AAudioStream_getFramesPerBurst(output_):0;
    values[6]=output_?AAudioStream_getSampleRate(output_):0;
    values[7]=output_?AAudioStream_getXRunCount(output_):0;
    values[8]=input_?AAudioStream_getXRunCount(input_):0;
    values[9]=output_?AAudioStream_getBufferCapacityInFrames(output_):0;
    values[10]=input_?AAudioStream_getBufferCapacityInFrames(input_):0;
    values[11]=output_?AAudioStream_getSharingMode(output_):0;
    values[12]=input_?AAudioStream_getSharingMode(input_):0;
    values[13]=fifo_.overflow();
    values[14]=fifo_.starvation();
    values[15]=static_cast<jint>(fifo_.available());
    values[16]=lastError_.load();
    values[17]=input_?AAudioStream_getChannelCount(input_):0;
    values[18]=output_?AAudioStream_getChannelCount(output_):0;
    values[19]=static_cast<jint>(inputPeak_.load()*1000.f);
    values[20]=static_cast<jint>(outputPeak_.load()*1000.f);
    values[21]=input_?AAudioStream_getSampleRate(input_):0;
    values[22]=input_?AAudioStream_getPerformanceMode(input_):0;
    values[23]=output_?AAudioStream_getPerformanceMode(output_):0;
    values[24]=maxCallbackUs_.load();
    values[25]=maxOutputFrames_.load();
    values[26]=maxInputFrames_.load();
    values[27]=fifo_.ratioPartsPerMillion();
  }
private:
  std::mutex mutex_;
  AAudioStream *input_=nullptr,*output_=nullptr;
  int selectedIn_=-1,selectedOut_=-1;
  std::atomic<bool> running_{false};
  std::atomic<int> state_{ST_STOPPED},lastError_{0};
  AudioFifo fifo_;
  int outputBurst_=192,queueTarget_=384;
  std::atomic<int> maxCallbackUs_{0},maxOutputFrames_{0},maxInputFrames_{0};
  std::atomic<float> inputPeak_{0},outputPeak_{0};
  std::array<std::atomic<float>,9> params_{};
  std::atomic<int> channel_{1},cabinet_{0},bypass_{0};
  AmpModel model_;
  static void updateMaximum(std::atomic<int>& destination,int value) {
    int old=destination.load(std::memory_order_relaxed);
    while(old<value && !destination.compare_exchange_weak(old,value,
                  std::memory_order_relaxed,std::memory_order_relaxed)){}
  }
  static aaudio_data_callback_result_t inputCallback(
      AAudioStream*,void* data,void* buffer,int32_t frames){
    auto& e=*static_cast<LowLatencyEngine*>(data);
    if(!e.running_.load(std::memory_order_acquire))
      return AAUDIO_CALLBACK_RESULT_STOP;
    if(!buffer || frames<=0)return AAUDIO_CALLBACK_RESULT_CONTINUE;
    const auto* samples=static_cast<const int16_t*>(buffer);
    float peak=0;
    for(int32_t i=0;i<frames;i++)
      peak=std::max(peak,std::abs(samples[i]*(1.f/32768.f)));
    e.fifo_.push(samples,frames);
    e.inputPeak_.store(peak,std::memory_order_relaxed);
    updateMaximum(e.maxInputFrames_,frames);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
  }
  static aaudio_data_callback_result_t outputCallback(
      AAudioStream*,void* data,void* buffer,int32_t frames){
    auto& e=*static_cast<LowLatencyEngine*>(data);
    if(!e.running_.load(std::memory_order_acquire))
      return AAUDIO_CALLBACK_RESULT_STOP;
    if(!buffer || frames<=0)return AAUDIO_CALLBACK_RESULT_CONTINUE;
    struct timespec before{},after{};
    clock_gettime(CLOCK_MONOTONIC,&before);
    auto* out=static_cast<float*>(buffer);
    AmpModel::Controls c{};
    c.gain=e.params_[0].load();c.volume=e.params_[1].load();
    c.bass=e.params_[2].load();c.treble=e.params_[3].load();
    c.cut=e.params_[4].load();c.master=e.params_[5].load();
    c.spring=e.params_[6].load();c.tremSpeed=e.params_[7].load();
    c.tremDepth=e.params_[8].load();
    c.channel=e.channel_.load();c.cabinet=e.cabinet_.load();
    c.bypass=e.bypass_.load();
    e.model_.setControls(c);
    float peak=0;
    // Callback frame count is platform-selected and is not necessarily the
    // buffer size shown in CONFIG. Process any size in fixed stack chunks.
    float input[256];
    for(int32_t offset=0;offset<frames;){
      const int count=std::min(256,frames-offset);
      e.fifo_.pop(input,count,e.queueTarget_);
      for(int i=0;i<count;i++){
        const float value=e.model_.process(input[i]);
        out[(offset+i)*2]=value;out[(offset+i)*2+1]=value;
        peak=std::max(peak,std::abs(value));
      }
      offset+=count;
    }
    e.outputPeak_.store(peak,std::memory_order_relaxed);
    updateMaximum(e.maxOutputFrames_,frames);
    clock_gettime(CLOCK_MONOTONIC,&after);
    const auto elapsed=static_cast<int>(
      (after.tv_sec-before.tv_sec)*1000000L +
      (after.tv_nsec-before.tv_nsec)/1000L);
    updateMaximum(e.maxCallbackUs_,elapsed);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
  }
  static void errorCallback(AAudioStream*,void* data,aaudio_result_t error){
    auto& e=*static_cast<LowLatencyEngine*>(data);
    e.lastError_.store(error);e.state_.store(ST_FAILED);
    e.running_.store(false);
    // AAudio callbacks MUST NOT close/reopen streams. UI may restart later.
  }
  int openStream(aaudio_direction_t direction,aaudio_sharing_mode_t sharing,
                 int deviceId,int sampleRate,AAudioStream** dest){
    AAudioStreamBuilder* builder=nullptr;
    int r=AAudio_createStreamBuilder(&builder);
    if(r!=AAUDIO_OK)return r;
    AAudioStreamBuilder_setDirection(builder,direction);
    AAudioStreamBuilder_setSharingMode(builder,sharing);
    AAudioStreamBuilder_setPerformanceMode(builder,AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setChannelCount(builder,direction==AAUDIO_DIRECTION_OUTPUT?2:1);
    AAudioStreamBuilder_setFormat(builder,direction==AAUDIO_DIRECTION_OUTPUT?
                               AAUDIO_FORMAT_PCM_FLOAT:AAUDIO_FORMAT_PCM_I16);
    if(sampleRate>0)AAudioStreamBuilder_setSampleRate(builder,sampleRate);
    if(deviceId>0)AAudioStreamBuilder_setDeviceId(builder,deviceId);
    AAudioStreamBuilder_setDataCallback(builder,direction==AAUDIO_DIRECTION_OUTPUT?
                                         outputCallback:inputCallback,this);
    AAudioStreamBuilder_setErrorCallback(builder,errorCallback,this);
    r=AAudioStreamBuilder_openStream(builder,dest);
    AAudioStreamBuilder_delete(builder);
    return r;
  }
  int failLocked(int code){
    lastError_.store(code);
    closeLocked();
    state_.store(ST_FAILED);
    lastError_.store(code);
    return code;
  }
  void closeLocked(){
    running_.store(false,std::memory_order_release);
    if(output_)AAudioStream_requestStop(output_);
    if(input_)AAudioStream_requestStop(input_);
    if(output_){AAudioStream_close(output_);output_=nullptr;}
    if(input_){AAudioStream_close(input_);input_=nullptr;}
    state_.store(ST_STOPPED);
  }
};
static LowLatencyEngine engine;
extern "C" JNIEXPORT jint JNICALL
Java_com_analoglav_amps_AudioBridge_nativeStart(JNIEnv*,jobject,
                                                jint inId,jint outId,jint burst){
  return engine.start(inId,outId,burst);
}
extern "C" JNIEXPORT void JNICALL
Java_com_analoglav_amps_AudioBridge_nativeStop(JNIEnv*,jobject){
  engine.stop();
}
extern "C" JNIEXPORT jintArray JNICALL
Java_com_analoglav_amps_AudioBridge_nativeStats(JNIEnv* env,jobject){
  jint value[28]{};
  engine.getStats(value);
  auto array=env->NewIntArray(28);
  if(array)env->SetIntArrayRegion(array,0,28,value);
  return array;
}
extern "C" JNIEXPORT void JNICALL
Java_com_analoglav_amps_AudioBridge_nativeControls(JNIEnv* env,jobject,
                           jfloatArray controls,jint ch,jint cab,jint bypass){
  if(controls==nullptr||env->GetArrayLength(controls)<9)return;
  jfloat value[9]{};
  env->GetFloatArrayRegion(controls,0,9,value);
  engine.setControls(value,ch,cab,bypass);
}
