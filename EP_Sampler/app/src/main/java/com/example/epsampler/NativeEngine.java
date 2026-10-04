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
    public static native void setAdsr(int attackMs, int decayMs, int sustainPct, int releaseMs);
    public static native void setInstrument(int instrument);
    public static native void setDrumParameter(int parameter, int value);
    public static native void setDrumFx(int boostDb, int distortion);
    public static native void setPartFx(int part, int boostDb, int distortion);
    public static native void setFeltReverb(int mix, int decay);
    public static native void setPerformanceXY(boolean active, int part, int x, int y);
    public static native int setAudioBufferBursts(float bursts);
    public static native int audioFramesPerBurst();
    public static native int audioBufferSizeFrames();
    public static native int audioBufferCapacityFrames();
    public static native int audioXRunCount();
    public static native boolean loadDrumSample(int slot, byte[] wavBytes);
    public static native void noteOnPart(int part, int note, int velocity);
    public static native void noteOffPart(int part, int note, int velocity);
    public static native void polyPressurePart(int part, int note, int pressure);
    public static native void channelPressurePart(int part, int pressure);
    public static native void controlChangePart(int part, int cc, int value);
    public static native void pitchBendPart(int part, int value14);
}
