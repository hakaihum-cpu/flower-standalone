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
import java.util.List;

final class MidiController {
    interface Listener {
        void onNoteOn(int note, int velocity);
        void onNoteOff(int note, int velocity);
        void onPolyPressure(int note, int value);
        void onChannelPressure(int value);
        void onControlChange(int cc, int value);
        void onPitchBend(int value14);
        void onConnectionCountChanged(int count);
    }

    private final MidiManager midiManager;
    private final Handler main = new Handler(Looper.getMainLooper());
    private final Listener listener;
    private final List<MidiDevice> devices = new ArrayList<>();
    private final List<MidiOutputPort> ports = new ArrayList<>();
    private int pendingOpens = 0;

    MidiController(Context context, Listener listener) {
        this.listener = listener;
        midiManager = (MidiManager) context.getSystemService(Context.MIDI_SERVICE);
    }

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
            int opened = 0;
            for (MidiDeviceInfo.PortInfo pi : info.getPorts()) {
                if (pi.getType() != MidiDeviceInfo.PortInfo.TYPE_OUTPUT) continue;
                MidiOutputPort port = device.openOutputPort(pi.getPortNumber());
                if (port != null) {
                    port.connect(receiver);
                    ports.add(port);
                    opened++;
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
                if (b >= 0xF8) continue; // MIDI realtime messages may appear anywhere.
                if ((b & 0x80) != 0) {
                    if (b >= 0xF0) { runningStatus = 0; dataCount = 0; needed = 0; continue; }
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
            int type = status & 0xF0;
            switch (type) {
                case 0x80:
                    NativeEngine.noteOff(d1, d2);
                    listener.onNoteOff(d1, d2);
                    break;
                case 0x90:
                    if (d2 == 0) {
                        NativeEngine.noteOff(d1, 0);
                        listener.onNoteOff(d1, 0);
                    } else {
                        NativeEngine.noteOn(d1, d2);
                        listener.onNoteOn(d1, d2);
                    }
                    break;
                case 0xA0:
                    NativeEngine.polyPressure(d1, d2);
                    listener.onPolyPressure(d1, d2);
                    break;
                case 0xB0:
                    NativeEngine.controlChange(d1, d2);
                    listener.onControlChange(d1, d2);
                    break;
                case 0xD0:
                    NativeEngine.channelPressure(d1);
                    listener.onChannelPressure(d1);
                    break;
                case 0xE0:
                    int pb = (d2 << 7) | d1;
                    NativeEngine.pitchBend(pb);
                    listener.onPitchBend(pb);
                    break;
                default:
                    break;
            }
        }
    }
}
