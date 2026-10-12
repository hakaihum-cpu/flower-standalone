package com.analoglav.amps.usb;

import android.content.Context;
import android.hardware.usb.UsbConstants;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbEndpoint;
import android.hardware.usb.UsbInterface;
import android.hardware.usb.UsbManager;
import java.util.Locale;

/**
 * Experimental direct USBFS isochronous transport. NO AAudio input/output,
 * NO audio passthrough or latency claim. Sends silence on the OUT endpoint.
 *
 * The caller stops AAudio first, and must call this on a worker thread.
 * USB permission must have been granted explicitly by the user.
 */
public final class UsbIsoTester {
  static { System.loadLibrary("amps_audio"); }
  private UsbIsoTester(){}
  private static native String nativeRun(int fd,int inEp,int outEp,
      int maxInPacket,int maxOutPacket,int durationMs,int inflightUrbDepth);

  private static final class Selection {
    UsbInterface iface;UsbEndpoint ep;
    Selection(UsbInterface f,UsbEndpoint e){iface=f;ep=e;}
  }
  private static Selection select(UsbDevice device,boolean input){
    Selection best=null;
    for(int i=0;i<device.getInterfaceCount();i++){
      UsbInterface f=device.getInterface(i);
      if(f.getInterfaceClass()!=1 || f.getInterfaceSubclass()!=2 ||
         f.getAlternateSetting()==0)continue;
      for(int j=0;j<f.getEndpointCount();j++){
        UsbEndpoint ep=f.getEndpoint(j);
        if(ep.getType()!=UsbConstants.USB_ENDPOINT_XFER_ISOC)continue;
        if((ep.getAttributes()>>4 & 3)!=0)continue; // skip feedback/status
        if((ep.getDirection()==UsbConstants.USB_DIR_IN)!=input)continue;
        if(ep.getMaxPacketSize()<=0 || ep.getMaxPacketSize()>4096)continue;
        if(best==null || ep.getMaxPacketSize()<best.ep.getMaxPacketSize())
          best=new Selection(f,ep);
      }
    }
    return best;
  }
  public static String execute(Context context,UsbDevice device,int durationMs,int inflightUrbDepth) {
    if(device==null)return "DIRECT_USB=NO_DEVICE\n";
    UsbManager manager=(UsbManager)context.getSystemService(Context.USB_SERVICE);
    if(manager==null)return "DIRECT_USB=USB_MANAGER_MISSING\n";
    if(!manager.hasPermission(device))return "DIRECT_USB=USB_PERMISSION_REQUIRED\n";
    Selection in=select(device,true),out=select(device,false);
    StringBuilder result=new StringBuilder(String.format(Locale.US,
        "TARGET_USB VID=%04X PID=%04X DEVICE=%s\n",
        device.getVendorId(),device.getProductId(),device.getProductName()));
    if(in==null || out==null)
      return result.append("DIRECT_USB=NO_BIDIRECTIONAL_ISO_DATA_ALTERNATE\n").toString();
    result.append(String.format(Locale.US,
        "IN IF=%d ALT=%d EP=0x%02X MAX_PACKET=%d INTERVAL=%d\n"+
        "OUT IF=%d ALT=%d EP=0x%02X MAX_PACKET=%d INTERVAL=%d\n",
        in.iface.getId(),in.iface.getAlternateSetting(),in.ep.getAddress(),
        in.ep.getMaxPacketSize(),in.ep.getInterval(),
        out.iface.getId(),out.iface.getAlternateSetting(),out.ep.getAddress(),
        out.ep.getMaxPacketSize(),out.ep.getInterval()));
    if(in.iface.getId()==out.iface.getId() &&
       in.iface.getAlternateSetting()!=out.iface.getAlternateSetting())
      return result.append("DIRECT_USB=CONFLICTING_ALTERNATE_SETTINGS\n").toString();
    UsbDeviceConnection conn=null;
    boolean claimedIn=false,claimedOut=false;
    try {
      conn=manager.openDevice(device);
      if(conn==null)return result.append("DIRECT_USB=OPEN_FAILED\n").toString();
      claimedIn=conn.claimInterface(in.iface,true);
      if(!claimedIn)return result.append("DIRECT_USB=CLAIM_IN_FAILED\n").toString();
      claimedOut=in.iface.getId()==out.iface.getId() || conn.claimInterface(out.iface,true);
      if(!claimedOut)return result.append("DIRECT_USB=CLAIM_OUT_FAILED\n").toString();
      if(!conn.setInterface(in.iface))
        return result.append("DIRECT_USB=SET_ALT_IN_FAILED\n").toString();
      if(in.iface.getId()!=out.iface.getId()&&!conn.setInterface(out.iface))
        return result.append("DIRECT_USB=SET_ALT_OUT_FAILED\n").toString();
      result.append(nativeRun(conn.getFileDescriptor(),in.ep.getAddress(),
          out.ep.getAddress(),in.ep.getMaxPacketSize(),
          out.ep.getMaxPacketSize(),durationMs,inflightUrbDepth));
    }catch(Exception failure){
      result.append("DIRECT_USB_EXCEPTION=").append(failure.getClass().getSimpleName())
          .append(": ").append(failure.getMessage()).append('\n');
    }finally{
      if(conn!=null){
        if(claimedOut && in.iface.getId()!=out.iface.getId())
          conn.releaseInterface(out.iface);
        if(claimedIn)conn.releaseInterface(in.iface);
        conn.close();
      }
    }
    result.append("AAUDIO=NOT_USED_DURING_USBFS_TEST\n")
          .append("POST_TEST=USB_DEVICE_MAY_REQUIRE_RECONNECT\n");
    return result.toString();
  }
}
