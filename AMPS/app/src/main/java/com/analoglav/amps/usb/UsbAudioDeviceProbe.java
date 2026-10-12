package com.analoglav.amps.usb;

import android.content.Context;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbManager;
import android.hardware.usb.UsbEndpoint;
import android.hardware.usb.UsbInterface;
import java.util.Locale;

/** Read-only USB Audio descriptor collection, without altering audio routing. */
public final class UsbAudioDeviceProbe {
  private UsbAudioDeviceProbe(){}
  public static String inspect(Context context, UsbDevice selected) {
    if(selected==null)return "USB PROBE: NO SELECTED DEVICE";
    UsbManager m=(UsbManager)context.getSystemService(Context.USB_SERVICE);
    if(m==null)return "USB MANAGER MISSING";
    StringBuilder b=new StringBuilder();
    b.append(String.format(Locale.US,"USB DEVICE: %s\nVID=%04X PID=%04X interfaces=%d\n",
        selected.getProductName(),selected.getVendorId(),selected.getProductId(),
        selected.getInterfaceCount()));
    for(int i=0;i<selected.getInterfaceCount();i++){
      UsbInterface iface=selected.getInterface(i);
      b.append(String.format(Locale.US,
          "IFACE id=%d alt=%d class=%d subclass=%d protocol=%d endpoints=%d\n",
          iface.getId(),iface.getAlternateSetting(),iface.getInterfaceClass(),
          iface.getInterfaceSubclass(),iface.getInterfaceProtocol(),
          iface.getEndpointCount()));
      for(int j=0;j<iface.getEndpointCount();j++){
        UsbEndpoint ep=iface.getEndpoint(j);
        b.append(String.format(Locale.US,
          "  ENDPOINT addr=0x%02X dir=%s type=%d iso=%s maxPacket=%d interval=%d\n",
          ep.getAddress(),ep.getDirection()==128?"IN":"OUT",ep.getType(),
          ep.getType()==1,ep.getMaxPacketSize(),ep.getInterval()));
      }
    }
    if(!m.hasPermission(selected))return b.append("USB PERMISSION REQUIRED\n").toString();
    UsbDeviceConnection connection=null;
    try{
      connection=m.openDevice(selected);
      if(connection==null)return b.append("DEVICE OPEN FAILED\n").toString();
      byte[] raw=connection.getRawDescriptors();
      if(raw==null||raw.length==0){
        b.append("RAW DESCRIPTORS UNAVAILABLE; INTERFACE DETAILS ABOVE RETAINED\n");
        return b.toString();
      }
      b.append(UsbAudioDescriptorParser.parse(raw).summary());
      b.append("RAW DESCRIPTORS HEX:");
      {
        for(int i=0;i<raw.length && i<8192;i++)
          b.append(String.format(Locale.US,"%02X",raw[i]&255));
        if(raw.length>8192)b.append(" [TRUNCATED]");
      }
      b.append("\n");
    }catch(SecurityException ex){
      b.append("USB SECURITY EXCEPTION\n");
    }finally{
      if(connection!=null)connection.close();
    }
    b.append("END OF USB DESCRIPTOR SCAN. CHECK USBFS TEST COUNTERS SEPARATELY.\n");
    return b.toString();
  }
}
