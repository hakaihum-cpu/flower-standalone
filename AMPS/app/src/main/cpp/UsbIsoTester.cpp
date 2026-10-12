#include <jni.h>
#include <linux/usbdevice_fs.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <string>
#include <array>
#include <memory>
#include <algorithm>
#include <cstdio>

namespace {
struct alignas(8) Request {
  alignas(8) unsigned char storage[sizeof(usbdevfs_urb)+sizeof(usbdevfs_iso_packet_desc)]{};
  usbdevfs_urb* get(){return reinterpret_cast<usbdevfs_urb*>(storage);}
  usbdevfs_iso_packet_desc* packet(){return reinterpret_cast<usbdevfs_iso_packet_desc*>(storage+sizeof(usbdevfs_urb));}
  std::array<unsigned char,4096> audio{};
  bool pending=false;
};
static_assert(sizeof(usbdevfs_iso_packet_desc)==12,"unexpected USB iso descriptor size");
constexpr int kSlots=4;

int submit(int fd,Request& p,unsigned char ep,int packetBytes){
  std::memset(p.storage,0,sizeof(p.storage));
  auto* urb=p.get();
  urb->type=USBDEVFS_URB_TYPE_ISO;
  urb->endpoint=ep;
  urb->flags=USBDEVFS_URB_ISO_ASAP;
  urb->buffer=p.audio.data();
  urb->buffer_length=packetBytes;
  urb->number_of_packets=1;
  urb->usercontext=&p;
  p.packet()->length=packetBytes;
  // Transport feasibility only. Send zero PCM; never feed unverified guitar format.
  if(!(ep&0x80))std::fill(p.audio.begin(),p.audio.end(),0);
  if(ioctl(fd,USBDEVFS_SUBMITURB,p.get())!=0)return -errno;
  p.pending=true;
  return 0;
}

std::string run(int fd,int inEp,int outEp,int inBytes,int outBytes,int durationMs,int depth){
  if(fd<0 || inEp<128 || inEp>255 || outEp<1 || outEp>127 ||
     inBytes<1 || inBytes>4096 || outBytes<1 || outBytes>4096 ||
     durationMs<50 || durationMs>1500 || depth<1 || depth>kSlots)
     return "DIRECT_USB_ERROR=INVALID_ARGUMENTS\n";
  auto capture=std::make_unique<std::array<Request,kSlots>>();
  auto playback=std::make_unique<std::array<Request,kSlots>>();
  int submittedIn=0,submittedOut=0,completedIn=0,completedOut=0;
  int errorsIn=0,errorsOut=0,packetErrors=0,received=0,sent=0;
  int initialError=0,cleanupError=0;
  for(int i=0;i<depth;i++){auto& p=(*capture)[i];int rc=submit(fd,p,static_cast<unsigned char>(inEp),inBytes);
    if(rc<0){initialError=rc;break;}++submittedIn;}
  if(!initialError)for(int i=0;i<depth;i++){auto& p=(*playback)[i];
    int rc=submit(fd,p,static_cast<unsigned char>(outEp),outBytes);
    if(rc<0){initialError=rc;break;}++submittedOut;}
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(durationMs);
  while(!initialError && std::chrono::steady_clock::now()<deadline){
    pollfd ready{fd,POLLIN,0};
    if(poll(&ready,1,20)<0 && errno!=EINTR){initialError=-errno;break;}
    for(int k=0;k<2*kSlots;k++){
      usbdevfs_urb* done=nullptr;
      if(ioctl(fd,USBDEVFS_REAPURBNDELAY,&done)!=0){
        if(errno!=EAGAIN && errno!=EINTR){initialError=-errno;}break;
      }
      Request* r=nullptr;bool input=false;
      for(auto& p:*capture)if(p.get()==done){r=&p;input=true;break;}
      if(!r)for(auto& p:*playback)if(p.get()==done){r=&p;break;}
      if(!r){initialError=-20000;break;}
      r->pending=false;
      if(done->status || r->packet()->status){
        ++packetErrors;if(input)++errorsIn;else ++errorsOut;
      }else if(input){++completedIn;received+=r->packet()->actual_length;}
      else{++completedOut;sent+=r->packet()->actual_length;}
      int rc=submit(fd,*r,static_cast<unsigned char>(input?inEp:outEp),
                    input?inBytes:outBytes);
      if(rc<0){initialError=rc;break;}
      if(input)++submittedIn;else ++submittedOut;
    }
  }
  // The kernel can still reference the URB after submission.
  int outstanding=0;
  for(auto& p:*capture)if(p.pending){++outstanding;ioctl(fd,USBDEVFS_DISCARDURB,p.get());}
  for(auto& p:*playback)if(p.pending){++outstanding;ioctl(fd,USBDEVFS_DISCARDURB,p.get());}
  const auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(2500);
  while(outstanding && std::chrono::steady_clock::now()<end){
    pollfd ready{fd,POLLIN,0};poll(&ready,1,20);
    usbdevfs_urb* done=nullptr;
    if(ioctl(fd,USBDEVFS_REAPURBNDELAY,&done)!=0)continue;
    for(auto& p:*capture)if(p.get()==done && p.pending){p.pending=false;--outstanding;break;}
    for(auto& p:*playback)if(p.get()==done && p.pending){p.pending=false;--outstanding;break;}
  }
  if(outstanding){
    cleanupError=outstanding;
    // Safety: never free a kernel-owned DMA buffer. This experimental run
    // leaks retained pool memory on cleanup timeout instead of use-after-free.
    capture.release();playback.release();
  }
  char result[1024]{};
  std::snprintf(result,sizeof(result),
    "ENGINE=USBFS_ISO_DIRECT (NO_AAUDIO)\n"
    "QUEUE_DEPTH=%d\n"
    "ISO_IN_ENDPOINT=0x%02X PACKET_MAX=%d\n"
    "ISO_OUT_ENDPOINT=0x%02X PACKET_MAX=%d (SILENCE ONLY)\n"
    "SUBMIT_IN=%d COMPLETE_IN=%d BYTES_IN=%d ERR_IN=%d\n"
    "SUBMIT_OUT=%d COMPLETE_OUT=%d BYTES_OUT=%d ERR_OUT=%d\n"
    "PACKET_ERRORS=%d INITIAL_ERRNO=%d CLEANUP_PENDING=%d\n"
    "AUDIO_LOOPBACK=NOT_TESTED; LATENCY_MS=NOT_MEASURED\n",
    depth,inEp,inBytes,outEp,outBytes,submittedIn,completedIn,received,errorsIn,
    submittedOut,completedOut,sent,errorsOut,packetErrors,initialError,cleanupError);
  return result;
}
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_analoglav_amps_usb_UsbIsoTester_nativeRun(JNIEnv* env,jclass,
  jint fd,jint inEp,jint outEp,jint inBytes,jint outBytes,jint durationMs,jint depth){
  const std::string result=run(fd,inEp,outEp,inBytes,outBytes,durationMs,depth);
  return env->NewStringUTF(result.c_str());
}
