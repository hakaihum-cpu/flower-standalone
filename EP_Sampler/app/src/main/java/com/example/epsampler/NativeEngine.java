package com.example.epsampler;

public final class NativeEngine {
    static { System.loadLibrary("epaudio"); }
    private NativeEngine() {}

    public static native boolean start();
    public static native void stop();
    public static native boolean loadBank(String path);
    public static native boolean loadBankFd(int fd);
    public static native boolean isBankLoaded();
    public static native String bankStatus();

    public static native void noteOn(int note, int velocity);
    public static native void noteOff(int note, int velocity);
    public static native void polyPressure(int note, int pressure);
    public static native void channelPressure(int pressure);
    public static native void controlChange(int cc, int value);
    public static native void pitchBend(int value14);
    public static native void setDreamy(boolean enabled);
    public static native void setBoosterStep(int step);
    public static native void setBoostDb(int db);
    public static native void setSpaceMode(int mode);
    public static native void setSpaceParameters(int mix, int decay);
    public static native void setTape(boolean enabled);
    public static native void setTapeParameters(int wow, int flutter, int drive);
    public static native void setDreamyParameters(int x, int y, int mix);

    public static native void recorderToggleRecording();
    public static native void recorderToggleRandom();
    public static native void recorderClear();
    public static native void recorderPlaySlot(int slot);
    public static native void recorderRecordSlot(int slot);
    public static native void recorderToggleClock();
    public static native void recorderSetBpm(int bpm);
    public static native void recorderMidiRealtime(int status);
    public static native boolean recorderIsRecording();
    public static native boolean recorderIsRandom();
    public static native boolean recorderIsMidiClock();
    public static native int recorderBpm();
    public static native int recorderRecordingSlot();
    public static native boolean recorderSlotPlaying(int slot);
    public static native float recorderSlotProgress(int slot);
    public static native int recorderValidSamples(int slot);
    public static native float[] recorderPeaks(int slot);
    public static native float recorderInputLevel();
}
