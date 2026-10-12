package com.analoglav.amps.usb;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;

/**
 * Non-invasive USB Audio descriptor parser. Pure Java for host tests.
 * Reads only GET_DESCRIPTOR data supplied by the Android USB framework.
 * No USB interface claim, no audio output, no vendor requests, no isochronous URBs.
 */
public final class UsbAudioDescriptorParser {
    private UsbAudioDescriptorParser() {}

    public static final class Endpoint {
        public final int address, attributes, interval, maxPacketBytes;
        Endpoint(int address, int attributes, int interval, int maxPacketBytes) {
            this.address = address; this.attributes = attributes;
            this.interval = interval; this.maxPacketBytes = maxPacketBytes;
        }
        public boolean isIso() { return (attributes & 3) == 1; }
        public boolean isInput() { return (address & 0x80) != 0; }
        public String summary() {
            return String.format(Locale.US,
                "%s ep=0x%02X iso=%s sync=%d maxPacket=%d interval=%d",
                isInput()?"IN":"OUT", address, isIso(), (attributes>>2)&3,
                maxPacketBytes,interval);
        }
    }
    public static final class StreamAlt {
        public final int interfaceNumber, alternate, cls, subClass, protocol;
        public final List<Endpoint> endpoints = new ArrayList<>();
        StreamAlt(int interfaceNumber, int alternate, int cls, int subClass, int protocol) {
            this.interfaceNumber=interfaceNumber; this.alternate=alternate;
            this.cls=cls;this.subClass=subClass;this.protocol=protocol;
        }
        public boolean isAudioStreaming() { return cls==1 && subClass==2; }
        public String summary() {
            StringBuilder b=new StringBuilder(String.format(Locale.US,
                "interface=%d alt=%d cls=%d subcls=%d protocol=%d",
                interfaceNumber,alternate,cls,subClass,protocol));
            for(Endpoint ep:endpoints)b.append("\n  ").append(ep.summary());
            return b.toString();
        }
    }
    public static final class Report {
        public final List<StreamAlt> alternatives = new ArrayList<>();
        public final List<String> errors = new ArrayList<>();
        public int audioBcdAdc = 0;  // e.g. 0x0100 or 0x0200, 0 means unknown
        public int usbBcd = 0;
        public int vendorId = -1, productId = -1;
        public List<StreamAlt> candidateStreamingAlts() {
            List<StreamAlt> out = new ArrayList<>();
            for (StreamAlt alt:alternatives) {
                if (!alt.isAudioStreaming()) continue;
                for (Endpoint ep:alt.endpoints) if(ep.isIso()) {
                    out.add(alt);break;
                }
            }
            return Collections.unmodifiableList(out);
        }
        public boolean hasIsoIn() {
            for(StreamAlt alt:candidateStreamingAlts())
                for(Endpoint ep:alt.endpoints)
                    if(ep.isIso() && ep.isInput())return true;
            return false;
        }
        public boolean hasIsoOut() {
            for(StreamAlt alt:candidateStreamingAlts())
                for(Endpoint ep:alt.endpoints)
                    if(ep.isIso() && !ep.isInput())return true;
            return false;
        }
        public String summary() {
            StringBuilder s=new StringBuilder(String.format(Locale.US,
                "USB bcd=%04X VID=%04X PID=%04X UAC bcdADC=%04X\n"+
                "Iso INPUT=%s Iso OUTPUT=%s\n",
                usbBcd, vendorId, productId, audioBcdAdc,
                hasIsoIn(),hasIsoOut()));
            for(StreamAlt alt:alternatives)
                if(alt.isAudioStreaming())s.append(alt.summary()).append('\n');
            for(String err:errors)s.append("WARN ").append(err).append('\n');
            s.append("Descriptors alone CANNOT prove usable native usbfs isochronous"+
                     " transfer, sampling-rate control, or <=10ms round trip.\n");
            return s.toString();
        }
    }
    static int u8(byte[] raw,int p) {return raw[p]&0xFF;}
    static int u16(byte[] raw,int p) {return u8(raw,p)|(u8(raw,p+1)<<8);}
    public static Report parse(byte[] raw) {
        Report result=new Report();
        if(raw==null || raw.length<2) {
            result.errors.add("descriptor data missing");return result;
        }
        StreamAlt current=null;
        int p=0;
        while (p+2<=raw.length) {
            int len=u8(raw,p), type=u8(raw,p+1);
            if(len<2 || p+len>raw.length) {
                result.errors.add("truncated/invalid descriptor at offset "+p);break;
            }
            if(type==1 && len>=18) {
                result.usbBcd=u16(raw,p+2);
                result.vendorId=u16(raw,p+8);
                result.productId=u16(raw,p+10);
            } else if(type==4 && len>=9) {
                current=new StreamAlt(u8(raw,p+2),u8(raw,p+3),
                    u8(raw,p+5),u8(raw,p+6),u8(raw,p+7));
                result.alternatives.add(current);
            } else if(type==5 && len>=7 && current!=null) {
                int max=u16(raw,p+4),packet=max&0x7FF;
                int multiplier=1+((max>>11)&3);
                current.endpoints.add(new Endpoint(
                    u8(raw,p+2),u8(raw,p+3),u8(raw,p+6),packet*multiplier));
            } else if(type==0x24 && len>=5 && current!=null &&
                       current.cls==1 && current.subClass==1 &&
                       u8(raw,p+2)==1) {
                // AudioControl CS_INTERFACE HEADER, bcdADC offset 3 for UAC1/2
                result.audioBcdAdc=u16(raw,p+3);
            }
            p+=len;
        }
        return result;
    }
}