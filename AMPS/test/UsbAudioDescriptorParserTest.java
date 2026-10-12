package com.analoglav.amps.usb;

public final class UsbAudioDescriptorParserTest {
    static void ok(boolean value,String message) {
        if(!value)throw new AssertionError(message);
    }
    public static void main(String[] args) {
        byte[] config=new byte[] {
          // DEVICE: bcd USB 2.0, example VID/PID only
          18,1,0,2,0,0,0,64,0x34,0x12,0x78,0x56,0,1,1,2,3,1,
          // Configuration, total length not validated in this parser
          9,2,0,0,3,1,0,(byte)0x80,50,
          // Audio Control interface and class header UAC1
          9,4,0,0,0,1,1,0,0,
          9,0x24,1,0,1,9,0,1,1,
          // Audio Streaming interface 1 alt0 (zero endpoints)
          9,4,1,0,0,1,2,0,0,
          // Audio Streaming interface 1 alt1 OUT
          9,4,1,1,1,1,2,0,0,
          7,5,1,9,(byte)0xC0,0,1,
          // Audio Streaming interface 2 alt1 IN
          9,4,2,1,1,1,2,0,0,
          7,5,(byte)0x82,5,(byte)0xC0,0,1,
        };
        UsbAudioDescriptorParser.Report report=UsbAudioDescriptorParser.parse(config);
        ok(report.audioBcdAdc==0x0100,"UAC1 header");
        ok(report.usbBcd==0x0200,"USB 2.0");
        ok(report.vendorId==0x1234 && report.productId==0x5678,"VID PID");
        ok(report.candidateStreamingAlts().size()==2,"2 streaming alternates");
        ok(report.hasIsoIn() && report.hasIsoOut(),"both audio paths");
        ok(report.candidateStreamingAlts().get(0).endpoints.get(0).maxPacketBytes==192,
           "PCM max packet");
        byte[] broken={5,4,0,0};
        ok(!UsbAudioDescriptorParser.parse(broken).errors.isEmpty(),
            "truncation must be detected");
        byte[] zero={0,0};
        ok(!UsbAudioDescriptorParser.parse(zero).errors.isEmpty(),
            "zero length must not loop");
        // UAC2 AudioControl header and high-bandwidth isochronous endpoint.
        byte[] uac2=new byte[] {
          9,4,0,0,0,1,1,0x20,0,
          9,0x24,1,0,2,0,0,0,0,
          9,4,1,1,1,1,2,0x20,0,
          7,5,(byte)0x81,5,0,(byte)0x14,1
        };
        UsbAudioDescriptorParser.Report r2=UsbAudioDescriptorParser.parse(uac2);
        ok(r2.audioBcdAdc==0x0200,"UAC2 header");
        ok(r2.hasIsoIn()&&!r2.hasIsoOut(),"UAC2 IN only");
        ok(r2.candidateStreamingAlts().get(0).endpoints.get(0).maxPacketBytes==3072,
           "High bandwidth maximum packet");
        System.out.println("PASS: UAC1/UAC2, IN/OUT, high-bandwidth packet, invalid data checks");
        System.out.println(report.summary());
    }
}