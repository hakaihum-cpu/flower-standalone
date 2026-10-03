package com.example.epsampler;

public final class NativeEngine {
    static { System.loadLibrary("epaudio"); }
    private NativeEngine() {}

    public static native boolean start();
    public static native void stop();
    public static native boolean loadBank(String path);
    public static native boolean isBankLoaded();
    public static native String bankStatus();

    public static native void noteOn(int note, int velocity);
    public static native void noteOff(int note, int velocity);
    public static native void polyPressure(int note, int pressure);
    public static native void channelPressure(int pressure);
    public static native void controlChange(int cc, int value);
    public static native void pitchBend(int value14);
    public static native void setDreamy(boolean enabled);
}
