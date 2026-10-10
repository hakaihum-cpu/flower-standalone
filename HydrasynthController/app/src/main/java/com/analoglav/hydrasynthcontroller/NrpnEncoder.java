package com.analoglav.hydrasynthcontroller;

/** Non-Registered Parameter Number: 4 CC packets; no raw CC UI. */
public final class NrpnEncoder {
  private NrpnEncoder() {}
  public static byte[] encode(int channel,int msb,int lsb,int value) {
    if(channel<1||channel>16||msb<0||msb>127||lsb<0||lsb>127||value<0||value>16383)
      throw new IllegalArgumentException("Invalid NRPN");
    int s=0xB0|(channel-1);
    return new byte[] {(byte)s,99,(byte)msb,(byte)s,98,(byte)lsb,
                        (byte)s,6,(byte)(value>>7),(byte)s,38,(byte)(value&127)};
  }
}
