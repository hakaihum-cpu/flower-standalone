package com.example.epsampler;

import android.content.Context;
import android.media.midi.MidiDevice;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiDeviceStatus;
import android.media.midi.MidiManager;
import android.media.midi.MidiOutputPort;
import android.media.midi.MidiReceiver;
import android.os.Handler;
import android.os.Looper;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

final class MidiController {
    interface Listener {
        void onNoteOn(int part, int note, int velocity);
        void onNoteOff(int part, int note, int velocity);
        void onPolyPressure(int part, int note, int value);
        void onChannelPressure(int part, int value);
        void onControlChange(int part, int cc, int value);
        void onPitchBend(int part, int value14);
        void onConnectionCountChanged(int count);
    }

    private final MidiManager midiManager;
    private final Handler main = new Handler(Looper.getMainLooper());
    private final Listener listener;
    private final List<MidiDevice> devices = new ArrayList<>();
    private final List<MidiOutputPort> ports = new ArrayList<>();
    private int pendingOpens = 0;

    // 0 = OFF, 1..16 = MIDI channel. Default parts use CH1..CH8.
    private final int[] partChannels = new int[]{1,2,3,4,5,6,7,8};

    MidiController(Context context, Listener listener) {
        this.listener = listener;
        midiManager = (MidiManager) context.getSystemService(Context.MIDI_SERVICE);
    }

    void setPartChannels(int[] channels) {
        if (channels == null) return;
        for (int i=0; i<partChannels.length; i++) {
            int value = i < channels.length ? channels[i] : 0;
            partChannels[i] = Math.max(0, Math.min(16, value));
        }
    }

    int[] getPartChannels() { return Arrays.copyOf(partChannels, partChannels.length); }

    // Legacy compatibility only. Multi-timbral routing is always channel-aware.
    void setChannel(int ignored) { }
    int getChannel() { return 0; }

    void start() {
        if (midiManager == null) return;
        midiManager.registerDeviceCallback(callback, main);
        for (MidiDeviceInfo info : midiManager.getDevices()) open(info);
    }

    void stop() {
        if (midiManager == null) return;
        midiManager.unregisterDeviceCallback(callback);
        closeAll();
    }

    private final MidiManager.DeviceCallback callback = new MidiManager.DeviceCallback() {
        @Override public void onDeviceAdded(MidiDeviceInfo device) { open(device); }
        @Override public void onDeviceRemoved(MidiDeviceInfo device) { reopenAll(); }
        @Override public void onDeviceStatusChanged(MidiDeviceStatus status) { }
    };

    private void reopenAll() {
        closeAll();
        for (MidiDeviceInfo info : midiManager.getDevices()) open(info);
    }

    private void closeAll() {
        for (MidiOutputPort p : ports) {
            try { p.disconnect(receiver); } catch (Exception ignored) { }
            try { p.close(); } catch (IOException ignored) { }
        }
        ports.clear();
        for (MidiDevice d : devices) {
            try { d.close(); } catch (IOException ignored) { }
        }
        devices.clear();
        listener.onConnectionCountChanged(0);
    }

    private void open(MidiDeviceInfo info) {
        if (midiManager == null) return;
        pendingOpens++;
        midiManager.openDevice(info, device -> {
            pendingOpens--;
            if (device == null) return;
            devices.add(device);
            for (MidiDeviceInfo.PortInfo pi : info.getPorts()) {
                if (pi.getType() != MidiDeviceInfo.PortInfo.TYPE_OUTPUT) continue;
                MidiOutputPort port = device.openOutputPort(pi.getPortNumber());
                if (port != null) {
                    port.connect(receiver);
                    ports.add(port);
                }
            }
            listener.onConnectionCountChanged(ports.size());
        }, main);
    }

    private final MidiParser parser = new MidiParser();
    private final MidiReceiver receiver = new MidiReceiver() {
        @Override public void onSend(byte[] msg, int offset, int count, long timestamp) {
            parser.feed(msg, offset, count);
        }
    };

    private final class MidiParser {
        int runningStatus = 0;
        final int[] data = new int[2];
        int dataCount = 0;
        int needed = 0;

        void feed(byte[] bytes, int off, int count) {
            for (int i = off; i < off + count; i++) {
                int b = bytes[i] & 0xFF;
                if (b >= 0xF8) continue;
                if ((b & 0x80) != 0) {
                    if (b >= 0xF0) {
                        runningStatus = 0;
                        dataCount = 0;
                        needed = 0;
                        continue;
                    }
                    runningStatus = b;
                    dataCount = 0;
                    int type = b & 0xF0;
                    needed = (type == 0xC0 || type == 0xD0) ? 1 : 2;
                    continue;
                }
                if (runningStatus == 0 || needed == 0) continue;
                data[dataCount++] = b;
                if (dataCount >= needed) {
                    dispatch(runningStatus, data[0], needed == 2 ? data[1] : 0);
                    dataCount = 0;
                }
            }
        }

        void dispatch(int status, int d1, int d2) {
            final int channel = (status & 0x0F) + 1;
            final int type = status & 0xF0;

            for (int part=0; part<partChannels.length; part++) {
                if (partChannels[part] != channel) continue;

                switch (type) {
                    case 0x80:
                        NativeEngine.noteOffPart(part, d1, d2);
                        listener.onNoteOff(part, d1, d2);
                        break;
                    case 0x90:
                        if (d2 == 0) {
                            NativeEngine.noteOffPart(part, d1, 0);
                            listener.onNoteOff(part, d1, 0);
                        } else {
                            NativeEngine.noteOnPart(part, d1, d2);
                            listener.onNoteOn(part, d1, d2);
                        }
                        break;
                    case 0xA0:
                        NativeEngine.polyPressurePart(part, d1, d2);
                        listener.onPolyPressure(part, d1, d2);
                        break;
                    case 0xB0:
                        NativeEngine.controlChangePart(part, d1, d2);
                        listener.onControlChange(part, d1, d2);
                        break;
                    case 0xD0:
                        NativeEngine.channelPressurePart(part, d1);
                        listener.onChannelPressure(part, d1);
                        break;
                    case 0xE0:
                        int pb = (d2 << 7) | d1;
                        NativeEngine.pitchBendPart(part, pb);
                        listener.onPitchBend(part, pb);
                        break;
                    default:
                        break;
                }
            }
        }
    }
}
