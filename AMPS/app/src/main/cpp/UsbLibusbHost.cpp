#include <jni.h>
#include <libusb.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cerrno>
#include <memory>
#include <string>
#include <sys/time.h>
#include "AmpModel.h"

// USB Audio Class 1.0 host, Android-owned USB fd, libusb async ISO backend.
// The Android Java layer owns the USB permission, audio control interface,
// stream interfaces and UAC1 48kHz setup. This module never uses AAudio.
namespace {
constexpr int kRate=48000;
constexpr int kMaxSlots=4;
constexpr int kIsoPackets=2;
constexpr int kMaxBuffer=4096;
constexpr int kFifoCapacity=4096;
std::atomic<bool> running{false};
std::array<std::atomic<float>,9> controls{};
std::atomic<int> channel{1}, cabinet{0}, bypass{0};
std::atomic<int> inCompleted{0},outCompleted{0},rxBytes{0},txBytes{0};
std::atomic<int> underruns{0},usbErrors{0};

float decode(const unsigned char* b,int sampleBytes,int validBits) {
  uint32_t packed=0;
  for(int i=0;i<sampleBytes;i++)
    packed|=static_cast<uint32_t>(b[i])<<(i*8);
  const int32_t sample=static_cast<int32_t>(packed<<(32-validBits));
  return static_cast<float>(sample)/2147483648.0f;
}
void encode(float value,unsigned char* dst,int sampleBytes,int validBits){
  value=std::max(-1.f,std::min(0.999999f,value));
  const int64_t limit=int64_t{1}<<(validBits-1);
  int64_t sample=static_cast<int64_t>(std::llround(value*limit));
  sample=std::max(-limit,std::min(limit-1,sample));
  const uint32_t packed=static_cast<uint32_t>(static_cast<int32_t>(sample));
  for(int i=0;i<sampleBytes;i++)dst[i]=static_cast<unsigned char>(
      (packed>>(i*8))&0xff);
}
struct Host;
struct Slot {
  Host* owner=nullptr;
  libusb_transfer* transfer=nullptr;
  bool input=false;
  bool submitted=false;
  alignas(8) std::array<unsigned char,kMaxBuffer> pcm{};
};
struct Host {
  libusb_context* ctx=nullptr;
  libusb_device_handle* handle=nullptr;
  AmpModel amp;
  std::array<Slot,kMaxSlots> input{};
  std::array<Slot,kMaxSlots> output{};
  std::array<float,kFifoCapacity> fifo{};
  int fd=-1,epIn=0,epOut=0,inPacket=0,outPacket=0,bytes=0,bits=0;
  int framesPerPacket=0,depth=0;
  int rd=0,wr=0,fill=0;
  int error=0,firstInSubmitError=0,firstOutSubmitError=0;
  int firstInputStatus=-1,firstOutputStatus=-1;
  int firstInputIsoStatus=-1,firstOutputIsoStatus=-1;
  int firstBadInputPacket=-1,firstBadOutputPacket=-1;
  int inSubmit=0,outSubmit=0,inPackets=0,outPackets=0;
  int maxLoopGapUs=0,cleanupPending=0;
  bool stopping=false;
  explicit Host(int descriptor):fd(descriptor){
    amp.setSampleRate(kRate);
    amp.reset();
  }
  void applyControls(){
    AmpModel::Controls c;
    c.gain=controls[0].load();c.volume=controls[1].load();
    c.bass=controls[2].load();c.treble=controls[3].load();
    c.cut=controls[4].load();c.master=controls[5].load();
    c.spring=controls[6].load();c.tremSpeed=controls[7].load();
    c.tremDepth=controls[8].load();
    c.channel=channel.load();c.cabinet=cabinet.load();
    c.bypass=bypass.load();
    amp.setControls(c);
  }
  void prepareOutput(Slot& slot){
    applyControls();
    const int frameBytes=bytes*2;
    for(int i=0;i<framesPerPacket*kIsoPackets;i++){
      float x=0;
      if(fill>0){
        x=fifo[rd];rd=(rd+1)%kFifoCapacity;--fill;
      }else underruns.fetch_add(1);
      const float y=amp.process(x);
      encode(y,slot.pcm.data()+i*frameBytes,bytes,bits);
      encode(y,slot.pcm.data()+i*frameBytes+bytes,bytes,bits);
    }
  }
  void receivePacket(const unsigned char* pcm,int count){
    const int frames=count/(bytes*2);
    for(int i=0;i<frames;i++){
      const float sample=decode(pcm+i*bytes*2,bytes,bits);
      if(fill==kFifoCapacity){
        rd=(rd+1)%kFifoCapacity;--fill;
      }
      fifo[wr]=sample;wr=(wr+1)%kFifoCapacity;++fill;
    }
  }
  int activeCount() const{
    int n=0;
    for(const Slot& x:input)if(x.submitted)++n;
    for(const Slot& x:output)if(x.submitted)++n;
    return n;
  }
  void recordError(const Slot& slot,int transferStatus,
                   int packetStatus,int packetIndex){
    usbErrors.fetch_add(1);
    if(slot.input){
      if(firstInputStatus<0){
        firstInputStatus=transferStatus;
        firstInputIsoStatus=packetStatus;
        firstBadInputPacket=packetIndex;
      }
    }else if(firstOutputStatus<0){
      firstOutputStatus=transferStatus;
      firstOutputIsoStatus=packetStatus;
      firstBadOutputPacket=packetIndex;
    }
    // libusb status 0..6 is distinct from kernel errno (-108).
    error=-1000-(transferStatus!=LIBUSB_TRANSFER_COMPLETED?
        transferStatus:packetStatus);
    stopping=true;
  }
  static void LIBUSB_CALL completed(libusb_transfer* t){
    if(!t||!t->user_data)return;
    auto* slot=static_cast<Slot*>(t->user_data);
    Host& h=*slot->owner;
    slot->submitted=false;
    if(h.stopping || !running.load(std::memory_order_acquire))return;
    if(t->status!=LIBUSB_TRANSFER_COMPLETED){
      h.recordError(*slot,t->status,-1,-1);
      return;
    }
    for(int i=0;i<t->num_iso_packets;i++){
      const auto& packet=t->iso_packet_desc[i];
      if(packet.status!=LIBUSB_TRANSFER_COMPLETED){
        h.recordError(*slot,t->status,packet.status,i);
        return;
      }
      const auto* data=libusb_get_iso_packet_buffer_simple(
          t,static_cast<unsigned int>(i));
      if(!data){
        h.recordError(*slot,LIBUSB_TRANSFER_ERROR,-1,i);
        return;
      }
      if(slot->input){
        const int count=std::min(static_cast<int>(packet.actual_length),
                                 h.inPacket);
        h.receivePacket(data,std::max(0,count));
        rxBytes.fetch_add(std::max(0,count));
        ++h.inPackets;
      }else{
        txBytes.fetch_add(h.outPacket);
        ++h.outPackets;
      }
    }
    if(slot->input){
      inCompleted.fetch_add(1);
    }else{
      outCompleted.fetch_add(1);
      h.prepareOutput(*slot);
    }
    if(h.stopping || !running.load(std::memory_order_acquire))return;
    const int result=libusb_submit_transfer(t);
    if(result!=LIBUSB_SUCCESS){
      if(slot->input)h.firstInSubmitError=result;
      else h.firstOutSubmitError=result;
      h.error=result;h.stopping=true;
      usbErrors.fetch_add(1);
    }else{
      slot->submitted=true;
      if(slot->input)++h.inSubmit;else ++h.outSubmit;
    }
  }
  int startSlot(Slot& s,bool capture){
    s.owner=this;s.input=capture;
    s.transfer=libusb_alloc_transfer(kIsoPackets);
    if(!s.transfer)return LIBUSB_ERROR_NO_MEM;
    const int packetSize=capture?inPacket:outPacket;
    const int endpoint=capture?epIn:epOut;
    libusb_fill_iso_transfer(s.transfer,handle,
        static_cast<unsigned char>(endpoint),s.pcm.data(),
        packetSize*kIsoPackets,kIsoPackets,completed,&s,0);
    libusb_set_iso_packet_lengths(s.transfer,
        static_cast<unsigned int>(packetSize));
    // Keep initial playback completely silent; no uninitialised buffers.
    if(!capture)s.pcm.fill(0);
    const int rc=libusb_submit_transfer(s.transfer);
    if(rc!=LIBUSB_SUCCESS){
      if(capture)firstInSubmitError=rc;else firstOutSubmitError=rc;
      error=rc;stopping=true;usbErrors.fetch_add(1);
      return rc;
    }
    s.submitted=true;
    if(capture)++inSubmit;else ++outSubmit;
    return LIBUSB_SUCCESS;
  }
  void cleanup(){
    stopping=true;
    for(Slot& x:input)if(x.submitted)libusb_cancel_transfer(x.transfer);
    for(Slot& x:output)if(x.submitted)libusb_cancel_transfer(x.transfer);
    const auto deadline=std::chrono::steady_clock::now()+
        std::chrono::seconds(2);
    while(activeCount()>0 && std::chrono::steady_clock::now()<deadline){
      timeval wait{0,1000};
      const int rc=libusb_handle_events_timeout_completed(ctx,&wait,nullptr);
      if(rc!=LIBUSB_SUCCESS && rc!=LIBUSB_ERROR_INTERRUPTED)break;
    }
    cleanupPending=activeCount();
  }
  void closeSafe(){
    if(cleanupPending)return;
    for(Slot& x:input)if(x.transfer)libusb_free_transfer(x.transfer);
    for(Slot& x:output)if(x.transfer)libusb_free_transfer(x.transfer);
    if(handle)libusb_close(handle);handle=nullptr;
    if(ctx)libusb_exit(ctx);ctx=nullptr;
  }
  std::string report()const{
    char msg[2300]{};
    std::snprintf(msg,sizeof(msg),
      "ENGINE=LIBUSB_UAC1_HOST_ASYNC_DSP (NO_AAUDIO)\n"
      "LIBUSB_FD_WRAP=1 NO_DEVICE_DISCOVERY=1\n"
      "SAMPLE_RATE=48000 CHANNELS=2 BYTES_PER_SAMPLE=%d VALID_BITS=%d\n"
      "USB_ENDPOINT_IN=0x%02X USB_ENDPOINT_OUT=0x%02X\n"
      "ISO_PACKETS_PER_TRANSFER=%d IN_PACKET_BYTES=%d OUT_PACKET_BYTES=%d QUEUE_DEPTH=%d\n"
      "SUBMIT_IN=%d COMPLETE_IN=%d PACKETS_IN=%d RX_BYTES=%d\n"
      "SUBMIT_OUT=%d COMPLETE_OUT=%d PACKETS_OUT=%d TX_BYTES=%d\n"
      "DSP_UNDERRUN_FRAMES=%d LIBUSB_ERRORS=%d LIBUSB_ERROR_CODE=%d\n"
      "FIRST_IN_TRANSFER_STATUS=%d FIRST_IN_PACKET_STATUS=%d FIRST_IN_BAD_PACKET=%d FIRST_IN_SUBMIT_ERR=%d\n"
      "FIRST_OUT_TRANSFER_STATUS=%d FIRST_OUT_PACKET_STATUS=%d FIRST_OUT_BAD_PACKET=%d FIRST_OUT_SUBMIT_ERR=%d\n"
      "MAX_LOOP_GAP_US=%d CLEANUP_PENDING=%d\n"
      "LIBUSB_TRANSFER_STATUS_ENUM=0:COMPLETED,1:ERROR,2:TIMED_OUT,3:CANCELLED,4:STALL,5:NO_DEVICE,6:OVERFLOW\n"
      "ENGINE_PATH=LIBUSB_ASYNC_ISO; NO_AAUDIO_FALLBACK\n"
      "ANALOG_ROUND_TRIP_LATENCY=NOT_MEASURED\n",
      bytes,bits,epIn,epOut,kIsoPackets,inPacket,outPacket,depth,
      inSubmit,inCompleted.load(),inPackets,rxBytes.load(),
      outSubmit,outCompleted.load(),outPackets,txBytes.load(),
      underruns.load(),usbErrors.load(),error,
      firstInputStatus,firstInputIsoStatus,firstBadInputPacket,firstInSubmitError,
      firstOutputStatus,firstOutputIsoStatus,firstBadOutputPacket,firstOutSubmitError,
      maxLoopGapUs,cleanupPending);
    return msg;
  }
};
std::string run(int fd,int epIn,int epOut,int maxIn,int maxOut,
                int intervalIn,int intervalOut,int sampleBytes,int bits,int depth){
  if(fd<0 || epIn<128 || epIn>255 || epOut<1 || epOut>127 ||
     sampleBytes<2||sampleBytes>4||bits<16||bits>sampleBytes*8||
     depth<1||depth>kMaxSlots||intervalIn!=1||intervalOut!=1)
    return "USB_DIRECT_ERROR=LIBUSB_BAD_FORMAT_OR_INTERVAL\n";
  const int inPacket=48*2*sampleBytes;
  const int outPacket=inPacket;
  if(inPacket>maxIn||outPacket>maxOut||
     inPacket*kIsoPackets>kMaxBuffer||outPacket*kIsoPackets>kMaxBuffer)
    return "USB_DIRECT_ERROR=LIBUSB_INVALID_PACKET_CAPACITY\n";
  bool expected=false;
  if(!running.compare_exchange_strong(expected,true))
    return "USB_DIRECT_ERROR=LIBUSB_ALREADY_RUNNING\n";
  inCompleted=0;outCompleted=0;rxBytes=0;txBytes=0;
  underruns=0;usbErrors=0;
  auto state=std::make_unique<Host>(fd);
  Host& h=*state;
  h.epIn=epIn;h.epOut=epOut;h.inPacket=inPacket;h.outPacket=outPacket;
  h.bytes=sampleBytes;h.bits=bits;h.depth=depth;h.framesPerPacket=48;
  // Bypass global/sysfs device enumeration: Android grants this fd.
  const libusb_init_option option{LIBUSB_OPTION_NO_DEVICE_DISCOVERY,{}};
  int rc=libusb_init_context(&h.ctx,&option,1);
  if(rc!=LIBUSB_SUCCESS){
    h.error=rc;
  }else{
    rc=libusb_wrap_sys_device(h.ctx,static_cast<intptr_t>(fd),&h.handle);
    if(rc!=LIBUSB_SUCCESS)h.error=rc;
  }
  if(!h.error){
    // Android Java already claimed and configured IF0/IF1/IF2 on this
    // exact same fd. Re-claiming a Java-owned interface is unnecessary.
    for(int i=0;i<depth && !h.error;i++)
      h.startSlot(h.output[i],false);
    for(int i=0;i<depth && !h.error;i++)
      h.startSlot(h.input[i],true);
  }
  const auto noOutputDeadline=std::chrono::steady_clock::now()+
      std::chrono::milliseconds(250);
  auto previousLoop=std::chrono::steady_clock::now();
  while(!h.error && running.load(std::memory_order_acquire)){
    const auto now=std::chrono::steady_clock::now();
    h.maxLoopGapUs=std::max(h.maxLoopGapUs,static_cast<int>(
        std::chrono::duration_cast<std::chrono::microseconds>(
        now-previousLoop).count()));
    previousLoop=now;
    if(outCompleted.load()==0&&now>=noOutputDeadline){
      h.error=-ETIMEDOUT;
      break;
    }
    timeval wait{0,1000}; // libusb handles its own usbfs poll and ISO URBs
    rc=libusb_handle_events_timeout_completed(h.ctx,&wait,nullptr);
    if(rc!=LIBUSB_SUCCESS && rc!=LIBUSB_ERROR_INTERRUPTED){
      h.error=rc;break;
    }
  }
  running.store(false,std::memory_order_release);
  if(h.ctx)h.cleanup();
  std::string result=h.report();
  if(h.cleanupPending){
    // URBs may still reference their transfer/PCM buffers. Leak this one
    // worker's state rather than risking use-after-free (diagnostic explicit).
    state.release();
  }else h.closeSafe();
  return result;
}
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_analoglav_amps_usb_UsbDirectEngine_nativeRun(JNIEnv* env,jclass,
    jint fd,jint inEp,jint outEp,jint maxIn,jint maxOut,
    jint intervalIn,jint intervalOut,jint bytes,jint bits,jint depth){
  const std::string result=run(fd,inEp,outEp,maxIn,maxOut,
      intervalIn,intervalOut,bytes,bits,depth);
  return env->NewStringUTF(result.c_str());
}
extern "C" JNIEXPORT void JNICALL
Java_com_analoglav_amps_usb_UsbDirectEngine_nativeStop(JNIEnv*,jclass){
  running.store(false,std::memory_order_release);
}
extern "C" JNIEXPORT jintArray JNICALL
Java_com_analoglav_amps_usb_UsbDirectEngine_nativeCounters(JNIEnv* env,jclass){
  jint out[]={running.load()&&inCompleted.load()>0&&outCompleted.load()>0?1:0,
      inCompleted.load(),outCompleted.load(),rxBytes.load(),txBytes.load(),
      underruns.load(),usbErrors.load()};
  auto arr=env->NewIntArray(7);
  if(arr)env->SetIntArrayRegion(arr,0,7,out);
  return arr;
}
extern "C" JNIEXPORT void JNICALL
Java_com_analoglav_amps_usb_UsbDirectEngine_nativeControls(JNIEnv* env,jclass,
    jfloatArray array,jint ch,jint cab,jint by){
  if(!array||env->GetArrayLength(array)<9)return;
  jfloat values[9]{};
  env->GetFloatArrayRegion(array,0,9,values);
  for(int i=0;i<9;i++)
    controls[i].store(std::max(0.f,std::min(1.f,values[i])));
  channel.store(ch);cabinet.store(cab);bypass.store(by);
}
