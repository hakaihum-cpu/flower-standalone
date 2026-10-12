#include <jni.h>
#include <linux/usbdevice_fs.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <atomic>
#include <array>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <cerrno>
#include <cmath>
#include <chrono>
#include <string>
#include <memory>
#include "AmpModel.h"

// Experimental direct USB audio engine. The audio path does not use AAudio,
// AudioTrack, AudioRecord or the Android mixer. UAC1 48 kHz stereo PCM only.
// Always fail closed for unsupported USB descriptors/formats.
namespace {
std::atomic<bool> running{false};
std::array<std::atomic<float>,9> controls{};
std::atomic<int> channel{1},cabinet{0},bypass{0};
std::atomic<int> completedIn{0},completedOut{0},underruns{0},usbErrors{0};
std::atomic<int> rxBytes{0},txBytes{0};
constexpr int slots=4, maxBytes=4096, ringSize=4096;
struct Request {
  alignas(8) unsigned char urbBytes[sizeof(usbdevfs_urb)+sizeof(usbdevfs_iso_packet_desc)]{};
  alignas(8) std::array<unsigned char,maxBytes> pcm{};
  usbdevfs_urb* urb(){return reinterpret_cast<usbdevfs_urb*>(urbBytes);}
  usbdevfs_iso_packet_desc* iso(){
    return reinterpret_cast<usbdevfs_iso_packet_desc*>(urbBytes+sizeof(usbdevfs_urb));
  }
  bool pending=false;
};
static_assert(sizeof(usbdevfs_iso_packet_desc)==12,"ISO descriptor layout changed");

float decode(const unsigned char* b,int subBytes,int bits) {
  uint32_t x=0;for(int i=0;i<subBytes;i++)x|=static_cast<uint32_t>(b[i])<<(8*i);
  // UAC1 TYPE I PCM is little endian, signed, with valid bits in low part.
  const unsigned shift=32-bits;
  const int32_t signedSample=static_cast<int32_t>(x<<shift);
  return (static_cast<float>(signedSample)/2147483648.0f);
}
void encode(float v,unsigned char* dst,int subBytes,int bits){
  v=std::max(-1.0f,std::min(0.999999f,v));
  int64_t sample=static_cast<int64_t>(std::llround(v*(static_cast<int64_t>(1)<<(bits-1))));
  const int64_t lim=(static_cast<int64_t>(1)<<(bits-1));
  sample=std::max(-lim,std::min(lim-1,sample));
  const uint32_t value=static_cast<uint32_t>(static_cast<int32_t>(sample));
  for(int i=0;i<subBytes;i++)dst[i]=static_cast<unsigned char>((value>>(8*i))&255);
}
int submit(int fd,Request& r,unsigned endpoint,int packetBytes){
  std::memset(r.urbBytes,0,sizeof(r.urbBytes));
  auto* u=r.urb();
  u->type=USBDEVFS_URB_TYPE_ISO;
  u->endpoint=endpoint;
  u->flags=USBDEVFS_URB_ISO_ASAP;
  u->buffer=r.pcm.data();
  u->buffer_length=packetBytes;
  u->number_of_packets=1;
  u->usercontext=&r;
  r.iso()->length=packetBytes;
  if(ioctl(fd,USBDEVFS_SUBMITURB,u)!=0)return -errno;
  r.pending=true;return 0;
}
std::string run(int fd,int epIn,int epOut,int maxIn,int maxOut,
                int intervalIn,int intervalOut,int subBytes,int bits,int depth) {
  if(fd<0 || epIn<128 || epIn>255 || epOut<1 || epOut>127 ||
     subBytes<2 || subBytes>4 || bits<16 || bits>32 || bits>8*subBytes ||
     maxIn<12 || maxOut<12 || maxIn>maxBytes || maxOut>maxBytes ||
     depth<1 || depth>slots)
    return "USB_DIRECT_ERROR=INVALID_FORMAT_OR_ENDPOINT\n";
  const int speed=ioctl(fd,USBDEVFS_GET_SPEED);
  if(speed!=2 && speed!=3)
    return "USB_DIRECT_ERROR=USB_SPEED_UNSUPPORTED_OR_UNKNOWN speed="+std::to_string(speed)+" errno="+std::to_string(errno)+"\n";
  if(intervalIn<1||intervalIn>4||intervalOut<1||intervalOut>4)
    return "USB_DIRECT_ERROR=INTERVAL_UNSUPPORTED\n";
  // Full speed = one interval unit per millisecond; high speed = 125us.
  // Strictly require integral 48k PCM samples per interval.
  auto frames=[&](int interval)->int{
    return speed==2 ? 48*(1<<(interval-1)) : 6*(1<<(interval-1));
  };
  const int framesIn=frames(intervalIn), framesOut=frames(intervalOut);
  const int frameBytes=2*subBytes;
  const int inPacket=framesIn*frameBytes, outPacket=framesOut*frameBytes;
  if(inPacket>maxIn||outPacket>maxOut||inPacket>maxBytes||outPacket>maxBytes)
    return "USB_DIRECT_ERROR=PACKET_CAPACITY_TOO_SMALL expectedIn="+
       std::to_string(inPacket)+" expectedOut="+std::to_string(outPacket)+
       " maxIn="+std::to_string(maxIn)+" maxOut="+std::to_string(maxOut)+"\n";
  bool expected=false;
  if(!running.compare_exchange_strong(expected,true))
    return "USB_DIRECT_ERROR=ALREADY_RUNNING\n";
  completedIn=0;completedOut=0;underruns=0;usbErrors=0;rxBytes=0;txBytes=0;
  AmpModel model;model.setSampleRate(48000);model.reset();
  std::array<float,ringSize> fifo{};
  int rd=0,wr=0,fill=0,submitIn=0,submitOut=0,err=0,cleanupPending=0;
  int inputErrors=0,outputErrors=0;
  int firstInUrbStatus=0,firstInPacketStatus=0;
  int firstOutUrbStatus=0,firstOutPacketStatus=0;
  int firstSubmitInputError=0,firstSubmitOutputError=0;
  auto in=std::make_unique<std::array<Request,slots>>();
  auto out=std::make_unique<std::array<Request,slots>>();
  auto prepare=[&](Request& r) {
    AmpModel::Controls c;
    c.gain=controls[0].load();c.volume=controls[1].load();
    c.bass=controls[2].load();c.treble=controls[3].load();
    c.cut=controls[4].load();c.master=controls[5].load();
    c.spring=controls[6].load();c.tremSpeed=controls[7].load();
    c.tremDepth=controls[8].load();
    c.channel=channel.load();c.cabinet=cabinet.load();c.bypass=bypass.load();
    model.setControls(c);
    for(int i=0;i<framesOut;i++){
      float x=0;
      if(fill>0){x=fifo[rd];rd=(rd+1)%ringSize;--fill;}
      else underruns.fetch_add(1);
      const float y=model.process(x);
      encode(y,r.pcm.data()+i*frameBytes,subBytes,bits);
      encode(y,r.pcm.data()+i*frameBytes+subBytes,subBytes,bits);
    }
  };
  for(int i=0;i<depth;i++){
    int rc=submit(fd,(*in)[i],epIn,inPacket);
    if(rc){err=rc;firstSubmitInputError=rc;break;}++submitIn;
  }
  if(!err)for(int i=0;i<depth;i++){
    std::fill((*out)[i].pcm.begin(),(*out)[i].pcm.end(),0);
    int rc=submit(fd,(*out)[i],epOut,outPacket);
    if(rc){err=rc;firstSubmitOutputError=rc;break;}++submitOut;
  }
  while(!err && running.load(std::memory_order_acquire)){
    pollfd pfd{fd,POLLIN,0};
    int pollResult=poll(&pfd,1,20);
    if(pollResult<0 && errno!=EINTR){err=-errno;break;}
    for(int k=0;k<2*slots;k++){
      usbdevfs_urb* done=nullptr;
      if(ioctl(fd,USBDEVFS_REAPURBNDELAY,&done)!=0){
        if(errno!=EAGAIN && errno!=EINTR)err=-errno;
        break;
      }
      Request* r=nullptr;bool capture=false;
      for(auto& q:*in)if(q.urb()==done){r=&q;capture=true;break;}
      if(!r)for(auto& q:*out)if(q.urb()==done){r=&q;break;}
      if(!r){err=-20000;break;}
      r->pending=false;
      if(done->status!=0 || r->iso()->status!=0){
        usbErrors.fetch_add(1);
        if(capture){
          ++inputErrors;
          if(inputErrors==1){
            firstInUrbStatus=done->status;
            firstInPacketStatus=r->iso()->status;
          }
        }else{
          ++outputErrors;
          if(outputErrors==1){
            firstOutUrbStatus=done->status;
            firstOutPacketStatus=r->iso()->status;
          }
        }
        const int failure=done->status?done->status:r->iso()->status;
        // Endpoint disabled/disconnected is terminal. Thirty-two blind
        // retries hide the original cause and can disrupt the device.
        if(failure==-ESHUTDOWN || failure==-ENODEV || failure==-EPIPE ||
           failure==-EPERM || inputErrors+outputErrors>=3){
          err=failure;
          break;
        }
      }else if(capture){
        const int bytes=std::max(0,std::min(static_cast<int>(r->iso()->actual_length),maxIn));
        rxBytes.fetch_add(bytes);completedIn.fetch_add(1);
        const int n=bytes/frameBytes;
        for(int i=0;i<n;i++){
          const unsigned char* sample=r->pcm.data()+i*frameBytes;
          const float audio=decode(sample,subBytes,bits); // Input ch 1
          if(fill==ringSize){rd=(rd+1)%ringSize;--fill;}
          fifo[wr]=audio;wr=(wr+1)%ringSize;++fill;
        }
      }else{
        txBytes.fetch_add(std::max(0,static_cast<int>(r->iso()->actual_length)));
        completedOut.fetch_add(1);
      }
      if(capture){
        const int rc=submit(fd,*r,epIn,inPacket);
        if(rc){err=rc;if(!firstSubmitInputError)firstSubmitInputError=rc;break;}++submitIn;
      }else{
        prepare(*r);
        const int rc=submit(fd,*r,epOut,outPacket);
        if(rc){err=rc;if(!firstSubmitOutputError)firstSubmitOutputError=rc;break;}++submitOut;
      }
    }
  }
  // Signal stop to UI before waiting for USB controller URB cancellation.
  running.store(false,std::memory_order_release);
  int pending=0;
  for(auto& r:*in)if(r.pending){++pending;ioctl(fd,USBDEVFS_DISCARDURB,r.urb());}
  for(auto& r:*out)if(r.pending){++pending;ioctl(fd,USBDEVFS_DISCARDURB,r.urb());}
  auto until=std::chrono::steady_clock::now()+std::chrono::milliseconds(2000);
  while(pending && std::chrono::steady_clock::now()<until){
    pollfd pfd{fd,POLLIN,0};poll(&pfd,1,20);
    usbdevfs_urb* done=nullptr;
    if(ioctl(fd,USBDEVFS_REAPURBNDELAY,&done)!=0)continue;
    for(auto& r:*in)if(r.pending && r.urb()==done){r.pending=false;--pending;break;}
    for(auto& r:*out)if(r.pending && r.urb()==done){r.pending=false;--pending;break;}
  }
  if(pending){cleanupPending=pending;in.release();out.release();}
  char report[1024]{};
  std::snprintf(report,sizeof(report),
    "ENGINE=USBFS_DIRECT_PCM_DSP (NO_AAUDIO)\n"
    "USB_SPEED=%d SAMPLE_RATE=48000 CHANNELS=2 BYTES_PER_SAMPLE=%d VALID_BITS=%d\n"
    "INTERVAL_IN=%d INTERVAL_OUT=%d PACKET_IN=%d PACKET_OUT=%d\n"
    "QUEUE_DEPTH=%d SUBMIT_IN=%d COMPLETE_IN=%d BYTES_IN=%d\n"
    "SUBMIT_OUT=%d COMPLETE_OUT=%d BYTES_OUT=%d\n"
    "DSP_UNDERRUN_FRAMES=%d USB_ERRORS=%d ERROR_CODE=%d CLEANUP_PENDING=%d\n"
    "IN_ERR_COUNT=%d FIRST_IN_URB_STATUS=%d FIRST_IN_PACKET_STATUS=%d FIRST_IN_SUBMIT_ERR=%d\n"
    "OUT_ERR_COUNT=%d FIRST_OUT_URB_STATUS=%d FIRST_OUT_PACKET_STATUS=%d FIRST_OUT_SUBMIT_ERR=%d\n"
    "ANALOG_ROUND_TRIP_LATENCY=NOT_MEASURED\n",
    speed,subBytes,bits,intervalIn,intervalOut,inPacket,outPacket,depth,
    submitIn,completedIn.load(),rxBytes.load(),submitOut,completedOut.load(),
    txBytes.load(),underruns.load(),usbErrors.load(),err,cleanupPending,
    inputErrors,firstInUrbStatus,firstInPacketStatus,firstSubmitInputError,
    outputErrors,firstOutUrbStatus,firstOutPacketStatus,firstSubmitOutputError);
  return report;
}
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_analoglav_amps_usb_UsbDirectEngine_nativeRun(JNIEnv* env,jclass,
  jint fd,jint inEp,jint outEp,jint maxIn,jint maxOut,
  jint intervalIn,jint intervalOut,jint sampleBytes,jint bits,jint queueDepth){
  auto result=run(fd,inEp,outEp,maxIn,maxOut,intervalIn,intervalOut,
                  sampleBytes,bits,queueDepth);
  return env->NewStringUTF(result.c_str());
}
extern "C" JNIEXPORT void JNICALL
Java_com_analoglav_amps_usb_UsbDirectEngine_nativeStop(JNIEnv*,jclass) {
  running.store(false,std::memory_order_release);
}
extern "C" JNIEXPORT jintArray JNICALL
Java_com_analoglav_amps_usb_UsbDirectEngine_nativeCounters(JNIEnv* env,jclass) {
  jint out[]={running.load()?1:0,completedIn.load(),completedOut.load(),
              rxBytes.load(),txBytes.load(),underruns.load(),usbErrors.load()};
  jintArray arr=env->NewIntArray(7);
  if(arr)env->SetIntArrayRegion(arr,0,7,out);
  return arr;
}
extern "C" JNIEXPORT void JNICALL
Java_com_analoglav_amps_usb_UsbDirectEngine_nativeControls(JNIEnv* env,jclass,
  jfloatArray vals,jint ch,jint cab,jint by) {
  if(vals==nullptr||env->GetArrayLength(vals)<9)return;
  jfloat tmp[9]{};
  env->GetFloatArrayRegion(vals,0,9,tmp);
  for(int i=0;i<9;i++)controls[i].store(std::max(0.0f,std::min(1.0f,tmp[i])));
  channel.store(ch);cabinet.store(cab);bypass.store(by);
}
