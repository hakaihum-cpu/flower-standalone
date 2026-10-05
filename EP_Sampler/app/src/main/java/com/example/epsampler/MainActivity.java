package com.example.epsampler;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.provider.Settings;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.widget.ArrayAdapter;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.ScrollView;
import android.widget.Button;
import android.widget.Toast;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;


public class MainActivity extends Activity implements MidiController.Listener, PianoView.ActionListener, DrumEditorView.Listener, PerformanceXYView.Listener, MixerView.Listener {
    private static final int PICK_BANK = 1001;
    private static final String PREFS = "violin_physical";
    private static final String KEY_BANK_URI = "bank_uri";
    private static final String KEY_BOOST = "boost_step";
    private static final String KEY_SPACE = "space_mode";
    private static final String KEY_TAPE = "tape_on";
    private static final String KEY_DREAMY = "dreamy_on";
    private static final String KEY_BOOST_DB = "boost_db";
    private static final String KEY_SPACE_MIX = "space_mix";
    private static final String KEY_SPACE_DECAY = "space_decay";
    private static final String KEY_TAPE_WOW = "tape_wow";
    private static final String KEY_TAPE_FLUTTER = "tape_flutter";
    private static final String KEY_TAPE_DRIVE = "tape_drive";
    private static final String KEY_DREAM_X = "dream_x";
    private static final String KEY_DREAM_Y = "dream_y";
    private static final String KEY_DREAM_MIX = "dream_mix";
    private static final String KEY_DREAM_MODE = "dream_mode";
    private static final String KEY_DREAM_P3 = "dream_p3";
    private static final String KEY_DREAM_P4 = "dream_p4";
    private static final String KEY_PART_MIDI_PREFIX = "part_midi_ch_";
    private static final String KEY_MANUAL_SUSTAIN = "manual_sustain";
    private static final String KEY_ATTACK_MS = "attack_ms";
    private static final String KEY_DECAY_MS = "decay_ms";
    private static final String KEY_SUSTAIN_PCT = "sustain_pct";
    private static final String KEY_RELEASE_MS = "release_ms";
    private static final String KEY_INSTRUMENT = "instrument_mode";
    private static final String KEY_DRUM_KICK_TUNE = "drum_kick_tune";   // legacy migration
    private static final String KEY_DRUM_HAT_TUNE = "drum_hat_tune";     // legacy migration
    private static final String KEY_DRUM_SNARE_TUNE = "drum_snare_tune";// legacy migration
    private static final String KEY_DRUM_DECAY = "drum_decay";           // legacy migration
    private static final String KEY_DRUM_PARAM_PREFIX = "drum_param_";
    private static final String KEY_DRUM_BOOST_DB = "drum_boost_db";
    private static final String KEY_DRUM_DISTORTION = "drum_distortion";
    private static final String KEY_PART_BOOST_PREFIX = "part_boost_db_";
    private static final String KEY_PART_DIST_PREFIX = "part_distortion_";
    private static final String KEY_FELT_REVERB_MIX = "felt_reverb_mix";
    private static final String KEY_FELT_REVERB_DECAY = "felt_reverb_decay";
    private static final String KEY_AUDIO_BUFFER_BURSTS = "audio_buffer_bursts";
    private static final String KEY_MIX_VOL_PREFIX = "mix_vol_";
    private static final String KEY_MIX_PAN_PREFIX = "mix_pan_";
    private static final String KEY_MIX_MUTE_PREFIX = "mix_mute_";
    private static final String[] AUDIO_BUFFER_LABELS = {
            "AUTO", "0.5 BURST", "0.75 BURST", "1 BURST", "1.5 BURSTS", "2 BURSTS",
            "3 BURSTS", "4 BURSTS", "5 BURSTS", "6 BURSTS", "7 BURSTS", "8 BURSTS"
    };
    private static final float[] AUDIO_BUFFER_VALUES = {
            0f, 0.5f, 0.75f, 1f, 1.5f, 2f, 3f, 4f, 5f, 6f, 7f, 8f
    };
    private static final String PRESET_PREFS = "violin_physical_presets";
    private static final int PRESET_SLOTS = 8;

    private static final String[] DREAM_MODE_NAMES = new String[] {
            "DREAMY",
            "MICROCOSM / MOSAIC",
            "MICROCOSM / GLIDE",
            "MICROCOSM / HAZE",
            "CHROMA / COLLAGE",
            "CHROMA / SPACE",
            "MOOD / REVERB",
            "MOOD / DELAY",
            "MOOD / SLIP",
            "MOOD / TAPE",
            "MOOD / STRETCH"
    };
    private static final String[][] DREAM_PARAM_NAMES = new String[][] {
            {"DRIFT", "FRAGMENT", "COLOR", "SPACE"},
            {"ACTIVITY", "VARIATION", "REPEATS", "SPACE"},
            {"ACTIVITY", "SHAPE", "REPEATS", "SPACE"},
            {"DENSITY", "SPREAD", "VARIATION", "DIFFUSION"},
            {"TIME", "AMOUNT", "DRIFT", "CASSETTE"},
            {"TIME / SIZE", "AMOUNT", "DRIFT", "CASSETTE"},
            {"CLOCK", "TIME / SIZE", "MODIFY / SMEAR", "MICRO-LOOP"},
            {"CLOCK", "TIME", "MODIFY / FEEDBACK", "MICRO-LOOP"},
            {"CLOCK", "REFRESH", "MODIFY / SPEED", "MICRO-LOOP"},
            {"CLOCK", "LENGTH", "MODIFY / SPEED", "FADE"},
            {"CLOCK", "LENGTH", "MODIFY / STRETCH", "TONE"}
    };
    private static final int[][] DREAM_DEFAULTS = new int[][] {
            {28,28,50,50,34},
            {55,25,45,35,45},
            {35,60,40,35,45},
            {65,55,35,65,50},
            {42,55,32,30,45},
            {55,60,25,20,42},
            {50,55,70,35,45},
            {55,45,55,30,42},
            {50,45,65,30,45},
            {45,50,55,65,50},
            {50,45,55,45,48}
    };
    private static final String[] DREAM_MODE_DESCRIPTIONS = new String[] {
            "Original Dreamy: +5 / +12 semitone overlapping micro-loops.",
            "Microcosm-inspired Mosaic: overlapping loop layers at multiple playback speeds.",
            "Microcosm-inspired Glide: overlapping short loops with moving playback speed.",
            "Microcosm Haze: grain-density/spread wash with A-D style speed families.",
            "Chroma Collage: looping delay with feedback, random double-speed events and Drift.",
            "Chroma Space: large diffusion/reverb with pitch-modulated Drift.",
            "MOOD Wet/Reverb + captured micro-loop layer.",
            "MOOD Wet/Delay + optional micro-loop layer.",
            "MOOD Wet/Slip: refresh + playback speed/direction.",
            "MOOD Micro-Looper/Tape: loop length, speed/direction and fade.",
            "MOOD Micro-Looper/Stretch: slice length, stretch direction and tone."
    };

    private static final String[] INSTRUMENT_NAMES = new String[] {
            "VIOLIN", "FLUTE", "SAXOPHONE", "FELT PIANO",
            "PIANICA / ACCORDION", "XYLOPHONE", "WOOD BASS", "DRUMS",
            "EP-SAMPLE"
    };
    private static final String[] INSTRUMENT_BUTTONS = new String[] {
            "VIOLIN", "FLUTE", "SAX", "FELT",
            "ACCORD", "XYLO", "BASS", "DRUMS", "EP"
    };
    private PianoView pianoView;
    private ImageView epBackground;
    private PerformanceVideoLayer videoLayer;
    private PerformanceXYView performanceXYView;
    private DrumEditorView drumEditorView;
    private MixerView mixerView;
    private MidiController midiController;
    private volatile boolean dreamy = false;
    private int boosterStep = 0;
    private int spaceMode = 0;
    private boolean tape = false;
    private int boostDb = 0;
    private int spaceMix = 50, spaceDecay = 50;
    private int tapeWow = 50, tapeFlutter = 50, tapeDrive = 50;
    private int dreamX = 28, dreamY = 28, dreamP3 = 50, dreamP4 = 50, dreamMix = 34;
    private int dreamMode = 0;
    private final int[] partMidiChannels = new int[]{1,2,3,4,5,6,7,8,9};
    private boolean manualSustain = false;
    private int bowPressure = 74;
    private int bowSpeed = 74;
    private int bowPosition = 42;
    private int vibratoDepth = 14;
    private int attackMs = 20;
    private int decayMs = 120;
    private int sustainPct = 90;
    private int releaseMs = 300;
    private int instrumentMode = 0;
    private final int[] drumParameters = new int[]{
            64, 58, 46, 38,
            64, 43, 74, 56,
            64, 53, 74, 58
    };
    private int drumBoostDb = 6;
    private int drumDistortion = 0;
    private final int[] partBoostDb = new int[]{0,0,0,0,0,0,0,6,0};
    private final int[] partDistortion = new int[]{0,0,0,0,0,0,0,0,0};
    private int feltReverbMix = 28;
    private int feltReverbDecay = 58;
    private float audioBufferBursts = 0f;
    private final int[] partMixerVolume = new int[]{112,112,112,112,112,112,112,112,127};
    private final int[] partMixerPan = new int[]{64,64,64,64,64,64,64,64,64};
    private final boolean[] partMixerMute = new boolean[9];

    @Override protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        hideSystemUI();
        epBackground = new ImageView(this);
        epBackground.setImageResource(com.example.epsampler.R.drawable.piano_reference);
        epBackground.setScaleType(ImageView.ScaleType.FIT_CENTER);
        epBackground.setBackgroundColor(android.graphics.Color.BLACK);
        videoLayer = new PerformanceVideoLayer(this);
        pianoView = new PianoView(this);
        pianoView.setPerformanceVideoLayer(videoLayer);
        pianoView.setActionListener(this);
        performanceXYView = new PerformanceXYView(this, pianoView);
        performanceXYView.setListener(this);
        drumEditorView = new DrumEditorView(this);
        drumEditorView.setListener(this);
        mixerView = new MixerView(this);
        mixerView.setListener(this);

        FrameLayout root = new FrameLayout(this);
        root.addView(epBackground, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));
        root.addView(videoLayer, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));
        // Performance XY sits below the control UI. PianoView returns false for
        // blank-area ACTION_DOWN events so those gestures still reach XY.
        root.addView(performanceXYView, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));
        root.addView(pianoView, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));
        root.addView(drumEditorView, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));
        root.addView(mixerView, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));

        epBackground.setZ(0f);
        videoLayer.setZ(1f);
        performanceXYView.setZ(10f);
        pianoView.setZ(20f);
        drumEditorView.setZ(30f);
        mixerView.setZ(40f);
        setContentView(root);

        boosterStep = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_BOOST, 0);
        spaceMode = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_SPACE, 0);
        tape = getSharedPreferences(PREFS, MODE_PRIVATE).getBoolean(KEY_TAPE, false);
        dreamy = getSharedPreferences(PREFS, MODE_PRIVATE).getBoolean(KEY_DREAMY, false);
        boostDb = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_BOOST_DB, boosterStep * 2);
        spaceMix = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_SPACE_MIX, 50);
        spaceDecay = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_SPACE_DECAY, 50);
        tapeWow = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_TAPE_WOW, 50);
        tapeFlutter = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_TAPE_FLUTTER, 50);
        tapeDrive = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_TAPE_DRIVE, 50);
        dreamX = Math.max(0, Math.min(100, getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_DREAM_X, 28)));
        dreamY = Math.max(0, Math.min(100, getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_DREAM_Y, 28)));
        dreamP3 = Math.max(0, Math.min(100, getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_DREAM_P3, 50)));
        dreamP4 = Math.max(0, Math.min(100, getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_DREAM_P4, 50)));
        dreamMix = Math.max(0, Math.min(100, getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_DREAM_MIX, 34)));
        dreamMode = Math.max(0, Math.min(DREAM_MODE_NAMES.length - 1,
                getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_DREAM_MODE, 0)));
        for (int i=0; i<partMidiChannels.length; i++) {
            partMidiChannels[i] = Math.max(0, Math.min(16,
                    getSharedPreferences(PREFS, MODE_PRIVATE)
                            .getInt(KEY_PART_MIDI_PREFIX + i, i + 1)));
            partMixerVolume[i] = Math.max(0, Math.min(127,
                    getSharedPreferences(PREFS, MODE_PRIVATE)
                            .getInt(KEY_MIX_VOL_PREFIX + i, i == 8 ? 127 : 112)));
            partMixerPan[i] = Math.max(0, Math.min(127,
                    getSharedPreferences(PREFS, MODE_PRIVATE)
                            .getInt(KEY_MIX_PAN_PREFIX + i, 64)));
            partMixerMute[i] = getSharedPreferences(PREFS, MODE_PRIVATE)
                    .getBoolean(KEY_MIX_MUTE_PREFIX + i, false);
        }
        manualSustain = getSharedPreferences(PREFS, MODE_PRIVATE).getBoolean(KEY_MANUAL_SUSTAIN, false);
        attackMs = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_ATTACK_MS, 20);
        decayMs = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_DECAY_MS, 120);
        sustainPct = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_SUSTAIN_PCT, 90);
        releaseMs = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_RELEASE_MS, 300);
        instrumentMode = Math.max(0, Math.min(8,
                getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_INSTRUMENT, 0)));
        android.content.SharedPreferences drumPrefs = getSharedPreferences(PREFS, MODE_PRIVATE);
        drumBoostDb = Math.max(0, Math.min(18, drumPrefs.getInt(KEY_DRUM_BOOST_DB, 6)));
        drumDistortion = Math.max(0, Math.min(127, drumPrefs.getInt(KEY_DRUM_DISTORTION, 0)));
        for (int i=0; i<partBoostDb.length; i++) {
            int boostFallback = (i == 7) ? drumBoostDb : 0;
            int distFallback = (i == 7) ? drumDistortion : 0;
            partBoostDb[i] = Math.max(0, Math.min(18,
                    drumPrefs.getInt(KEY_PART_BOOST_PREFIX + i, boostFallback)));
            partDistortion[i] = Math.max(0, Math.min(127,
                    drumPrefs.getInt(KEY_PART_DIST_PREFIX + i, distFallback)));
        }
        drumBoostDb = partBoostDb[7];
        drumDistortion = partDistortion[7];
        feltReverbMix = Math.max(0, Math.min(100,
                drumPrefs.getInt(KEY_FELT_REVERB_MIX, 28)));
        feltReverbDecay = Math.max(0, Math.min(100,
                drumPrefs.getInt(KEY_FELT_REVERB_DECAY, 58)));
        audioBufferBursts = drumPrefs.getFloat(KEY_AUDIO_BUFFER_BURSTS, 0f);
        for (int i=0; i<drumParameters.length; i++) {
            int fallback = drumParameters[i];
            if (i == 0) fallback = drumPrefs.getInt(KEY_DRUM_KICK_TUNE, fallback);
            else if (i == 4) fallback = drumPrefs.getInt(KEY_DRUM_HAT_TUNE, fallback);
            else if (i == 8) fallback = drumPrefs.getInt(KEY_DRUM_SNARE_TUNE, fallback);
            else if (i == 1 || i == 5 || i == 9) fallback = drumPrefs.getInt(KEY_DRUM_DECAY, fallback);
            drumParameters[i] = Math.max(0, Math.min(127,
                    drumPrefs.getInt(KEY_DRUM_PARAM_PREFIX + i, fallback)));
        }

        pianoView.setBoosterStep(boosterStep);
        pianoView.setBoostDb(boostDb);
        pianoView.setSpaceMode(spaceMode);
        pianoView.setTape(tape);
        pianoView.setDreamy(dreamy);
        pianoView.setInstrumentName(INSTRUMENT_NAMES[instrumentMode], INSTRUMENT_BUTTONS[instrumentMode]);
        pianoView.setInstrumentMidiChannel(partMidiChannels[instrumentMode]);
        pianoView.setSampleMode(instrumentMode == 8);
        performanceXYView.setInstrumentMode(instrumentMode);
        drumEditorView.setValues(drumParameters);
        drumEditorView.setDrumFx(partBoostDb[7], partDistortion[7]);
        drumEditorView.setDrumsVisible(instrumentMode == 7);
        mixerView.setMixerState(partMixerVolume, partMixerPan, partMixerMute);
        pianoView.controlChange(7, partMixerVolume[instrumentMode]);
        epBackground.setVisibility(instrumentMode == 8 ? View.VISIBLE : View.GONE);
        videoLayer.setVisibility(instrumentMode == 8 ? View.GONE : View.VISIBLE);
        if (instrumentMode < 8) videoLayer.setInstrument(instrumentMode);

        loadDrumSampleAssets();
        NativeEngine.start();
        loadExistingBank();
        NativeEngine.setAudioBufferBursts(audioBufferBursts);
        NativeEngine.setBoosterStep(boosterStep);
        NativeEngine.setBoostDb(boostDb);
        NativeEngine.setSpaceMode(spaceMode);
        NativeEngine.setSpaceParameters(spaceMix, spaceDecay);
        NativeEngine.setTape(tape);
        NativeEngine.setTapeParameters(tapeWow, tapeFlutter, tapeDrive);
        NativeEngine.setDreamy(dreamy);
        NativeEngine.setDreamyMode(dreamMode);
        NativeEngine.setDreamyParameters(dreamX, dreamY, dreamMix);
        NativeEngine.setDreamyExtraParameters(dreamP3, dreamP4);
        pianoView.controlChange(103, Math.max(0, Math.min(127, Math.round(dreamX * 1.27f))));
        pianoView.controlChange(104, Math.max(0, Math.min(127, Math.round(dreamY * 1.27f))));
        for (int i=0; i<drumParameters.length; i++) {
            NativeEngine.setDrumParameter(i, drumParameters[i]);
        }
        for (int part=0; part<partBoostDb.length; part++) {
            NativeEngine.setPartFx(part, partBoostDb[part], partDistortion[part]);
            NativeEngine.setPartMixer(part, partMixerVolume[part], partMixerPan[part], partMixerMute[part]);
        }
        NativeEngine.setDrumFx(partBoostDb[7], partDistortion[7]);
        NativeEngine.setFeltReverb(feltReverbMix, feltReverbDecay);
        NativeEngine.setInstrument(instrumentMode);
        if (instrumentMode < 7) {
            NativeEngine.controlChange(10, bowPressure);
            NativeEngine.controlChange(11, bowSpeed);
            NativeEngine.controlChange(74, bowPosition);
            NativeEngine.controlChange(1, vibratoDepth);
            NativeEngine.setAdsr(attackMs, decayMs, sustainPct, releaseMs);
            pianoView.controlChange(10, bowPressure);
            pianoView.controlChange(11, bowSpeed);
            pianoView.controlChange(74, bowPosition);
            pianoView.controlChange(1, vibratoDepth);
        }
        if (manualSustain) {
            NativeEngine.controlChange(64, 127);
            pianoView.controlChange(64, 127);
        }

        midiController = new MidiController(this, this);
        midiController.setPartChannels(partMidiChannels);
        midiController.start();
    }

    private void loadDrumSampleAssets() {
        final String[] files = new String[]{
                "drum_closehat.wav",
                "drum_tom.wav",
                "drum_crash.wav",
                "drum_kick.wav",
                "drum_stick.wav",
                "drum_snaire.wav"
        };

        for (int slot=0; slot<files.length; slot++) {
            try (InputStream in = getAssets().open(files[slot]);
                 ByteArrayOutputStream out = new ByteArrayOutputStream()) {
                byte[] buffer = new byte[64 * 1024];
                int count;
                while ((count = in.read(buffer)) >= 0) {
                    if (count > 0) out.write(buffer, 0, count);
                }
                NativeEngine.loadDrumSample(slot, out.toByteArray());
            } catch (Exception ignored) {
                // Missing sample does not affect C4/C#4/D4 physical drums.
            }
        }
    }

    private void hideSystemUI() {
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
                View.SYSTEM_UI_FLAG_FULLSCREEN |
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        if (android.os.Build.VERSION.SDK_INT >= 30 && getWindow().getInsetsController() != null) {
            getWindow().getInsetsController().hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
            getWindow().getInsetsController().setSystemBarsBehavior(
                    WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
        }
    }

    private String epBankStatus() {
        String ch = partMidiChannels[8] <= 0 ? "OFF" : "CH" + partMidiChannels[8];
        return NativeEngine.isBankLoaded()
                ? "EP-SAMPLE " + NativeEngine.bankStatus() + " " + ch
                : "EP-SAMPLE BANK — " + ch;
    }

    private void loadExistingBank() {
        String saved = getSharedPreferences(PREFS, MODE_PRIVATE).getString(KEY_BANK_URI, null);
        if (saved == null || saved.isEmpty()) {
            if (instrumentMode == 8) pianoView.setBankStatus(epBankStatus());
            return;
        }
        loadBankUri(Uri.parse(saved));
    }

    private void loadBankUri(Uri uri) {
        if (instrumentMode == 8) pianoView.setBankStatus("EP-SAMPLE BANK LOADING…");
        new Thread(() -> {
            boolean ok = false;
            try (ParcelFileDescriptor pfd = getContentResolver().openFileDescriptor(uri, "r")) {
                if (pfd != null) ok = NativeEngine.loadBankFd(pfd.getFd());
            } catch (Exception ignored) {
            }
            final boolean result = ok;
            runOnUiThread(() -> {
                if (instrumentMode == 8) {
                    pianoView.setBankStatus(result ? epBankStatus() : "EP-SAMPLE BANK ERROR");
                }
            });
        }, "BankOpen").start();
    }

    @Override public void onToggleDreamy() {
        dreamy = !dreamy;
        NativeEngine.setDreamy(dreamy);
        pianoView.setDreamy(dreamy);
        getSharedPreferences(PREFS, MODE_PRIVATE).edit().putBoolean(KEY_DREAMY, dreamy).apply();
    }

    @Override public void onCycleBooster() {
        boosterStep = (boosterStep + 1) % 4;
        boostDb = boosterStep * 2;
        NativeEngine.setBoosterStep(boosterStep);
        NativeEngine.setBoostDb(boostDb);
        pianoView.setBoosterStep(boosterStep);
        pianoView.setBoostDb(boostDb);
        getSharedPreferences(PREFS, MODE_PRIVATE).edit()
                .putInt(KEY_BOOST, boosterStep).putInt(KEY_BOOST_DB, boostDb).apply();
    }

    @Override public void onCycleSpace() {
        spaceMode = (spaceMode + 1) % 4;
        NativeEngine.setSpaceMode(spaceMode);
        pianoView.setSpaceMode(spaceMode);
        getSharedPreferences(PREFS, MODE_PRIVATE).edit().putInt(KEY_SPACE, spaceMode).apply();
    }

    @Override public void onToggleTape() {
        tape = !tape;
        NativeEngine.setTape(tape);
        pianoView.setTape(tape);
        getSharedPreferences(PREFS, MODE_PRIVATE).edit().putBoolean(KEY_TAPE, tape).apply();
    }

    @Override public void onEditEffect(int effect) {
        if (effect == PianoView.EFFECT_BOOST) showBoostDialog();
        else if (effect == PianoView.EFFECT_SPACE) showSpaceDialog();
        else if (effect == PianoView.EFFECT_TAPE) showTapeDialog();
        else if (effect == PianoView.EFFECT_DREAMY) showDreamyDialog();
    }

    @Override public void onOpenConfig() {
        showConfigDialog();
    }

    @Override public void onOpenMixer() {
        pianoView.setRecorderOpen(false);
        mixerView.setMixerState(partMixerVolume, partMixerPan, partMixerMute);
        mixerView.setVisibility(View.VISIBLE);
        mixerView.bringToFront();
    }

    @Override public void onMixerClose() {
        mixerView.setVisibility(View.GONE);
        hideSystemUI();
    }

    @Override public void onMixerChanged(int part, int volume, int pan, boolean muted) {
        if (part < 0 || part >= partMixerVolume.length) return;
        partMixerVolume[part] = Math.max(0, Math.min(127, volume));
        partMixerPan[part] = Math.max(0, Math.min(127, pan));
        partMixerMute[part] = muted;
        NativeEngine.setPartMixer(part, partMixerVolume[part], partMixerPan[part], partMixerMute[part]);

        if (part == instrumentMode) {
            pianoView.controlChange(7, partMixerVolume[part]);
        }

        getSharedPreferences(PREFS, MODE_PRIVATE).edit()
                .putInt(KEY_MIX_VOL_PREFIX + part, partMixerVolume[part])
                .putInt(KEY_MIX_PAN_PREFIX + part, partMixerPan[part])
                .putBoolean(KEY_MIX_MUTE_PREFIX + part, partMixerMute[part])
                .apply();
    }

    private LinearLayout dialogRoot() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        int p = Math.round(18f * getResources().getDisplayMetrics().density);
        root.setPadding(p, p / 2, p, p / 2);
        return root;
    }

    private ScrollView scrollDialogView(LinearLayout root) {
        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.addView(root, new ScrollView.LayoutParams(
                ScrollView.LayoutParams.MATCH_PARENT,
                ScrollView.LayoutParams.WRAP_CONTENT));
        return scroll;
    }

    private void addSlider(LinearLayout root, String name, int max, int value, java.util.function.IntConsumer onChange) {
        TextView label = new TextView(this);
        label.setText(name + "  " + value);
        label.setTextSize(16f);
        root.addView(label);
        SeekBar bar = new SeekBar(this);
        bar.setMax(max);
        bar.setProgress(Math.max(0, Math.min(max, value)));
        bar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                label.setText(name + "  " + progress);
                if (fromUser) onChange.accept(progress);
            }
            @Override public void onStartTrackingTouch(SeekBar seekBar) { }
            @Override public void onStopTrackingTouch(SeekBar seekBar) { }
        });
        root.addView(bar);
    }

    private void showBoostDialog() {
        LinearLayout root = dialogRoot();
        addSlider(root, "BOOST dB", 6, boostDb, v -> {
            boostDb = v;
            NativeEngine.setBoostDb(v);
            pianoView.setBoostDb(v);
            getSharedPreferences(PREFS, MODE_PRIVATE).edit().putInt(KEY_BOOST_DB, v).apply();
        });
        new AlertDialog.Builder(this).setTitle("BOOST").setView(scrollDialogView(root))
                .setPositiveButton("CLOSE", null).show();
    }

    private void showSpaceDialog() {
        LinearLayout root = dialogRoot();
        addSlider(root, "MIX %", 100, spaceMix, v -> {
            spaceMix = v;
            NativeEngine.setSpaceParameters(spaceMix, spaceDecay);
            getSharedPreferences(PREFS, MODE_PRIVATE).edit().putInt(KEY_SPACE_MIX, v).apply();
        });
        addSlider(root, "DECAY %", 100, spaceDecay, v -> {
            spaceDecay = v;
            NativeEngine.setSpaceParameters(spaceMix, spaceDecay);
            getSharedPreferences(PREFS, MODE_PRIVATE).edit().putInt(KEY_SPACE_DECAY, v).apply();
        });
        new AlertDialog.Builder(this).setTitle("SPACE").setView(scrollDialogView(root))
                .setPositiveButton("CLOSE", null).show();
    }

    private void showTapeDialog() {
        LinearLayout root = dialogRoot();
        addSlider(root, "WOW %", 100, tapeWow, v -> {
            tapeWow = v;
            NativeEngine.setTapeParameters(tapeWow, tapeFlutter, tapeDrive);
            getSharedPreferences(PREFS, MODE_PRIVATE).edit().putInt(KEY_TAPE_WOW, v).apply();
        });
        addSlider(root, "FLUTTER %", 100, tapeFlutter, v -> {
            tapeFlutter = v;
            NativeEngine.setTapeParameters(tapeWow, tapeFlutter, tapeDrive);
            getSharedPreferences(PREFS, MODE_PRIVATE).edit().putInt(KEY_TAPE_FLUTTER, v).apply();
        });
        addSlider(root, "DRIVE %", 100, tapeDrive, v -> {
            tapeDrive = v;
            NativeEngine.setTapeParameters(tapeWow, tapeFlutter, tapeDrive);
            getSharedPreferences(PREFS, MODE_PRIVATE).edit().putInt(KEY_TAPE_DRIVE, v).apply();
        });
        new AlertDialog.Builder(this).setTitle("TAPE").setView(scrollDialogView(root))
                .setPositiveButton("CLOSE", null).show();
    }

    private void applyDreamySettings() {
        dreamMode = Math.max(0, Math.min(DREAM_MODE_NAMES.length - 1, dreamMode));
        dreamX = Math.max(0, Math.min(100, dreamX));
        dreamY = Math.max(0, Math.min(100, dreamY));
        dreamP3 = Math.max(0, Math.min(100, dreamP3));
        dreamP4 = Math.max(0, Math.min(100, dreamP4));
        dreamMix = Math.max(0, Math.min(100, dreamMix));

        NativeEngine.setDreamyMode(dreamMode);
        NativeEngine.setDreamyParameters(dreamX, dreamY, dreamMix);
        NativeEngine.setDreamyExtraParameters(dreamP3, dreamP4);

        // Existing performance XY / MIDI assignments continue to address P1/P2.
        pianoView.controlChange(103, Math.max(0, Math.min(127, Math.round(dreamX * 1.27f))));
        pianoView.controlChange(104, Math.max(0, Math.min(127, Math.round(dreamY * 1.27f))));
    }

    private void persistDreamySettings() {
        getSharedPreferences(PREFS, MODE_PRIVATE).edit()
                .putInt(KEY_DREAM_MODE, dreamMode)
                .putInt(KEY_DREAM_X, dreamX)
                .putInt(KEY_DREAM_Y, dreamY)
                .putInt(KEY_DREAM_P3, dreamP3)
                .putInt(KEY_DREAM_P4, dreamP4)
                .putInt(KEY_DREAM_MIX, dreamMix)
                .apply();
    }

    private void showDreamyDialog() {
        LinearLayout root = dialogRoot();

        TextView modeLabel = new TextView(this);
        modeLabel.setText("MODE");
        modeLabel.setTextSize(16f);
        root.addView(modeLabel);

        Spinner modeSpinner = new Spinner(this);
        modeSpinner.setAdapter(new ArrayAdapter<>(this,
                android.R.layout.simple_spinner_dropdown_item, DREAM_MODE_NAMES));
        modeSpinner.setSelection(dreamMode);
        root.addView(modeSpinner);

        TextView description = new TextView(this);
        description.setTextSize(13f);
        description.setPadding(0, 6, 0, 12);
        root.addView(description);

        final TextView[] labels = new TextView[5];
        final SeekBar[] bars = new SeekBar[5];
        final boolean[] updating = {true};

        for (int i=0; i<5; i++) {
            final int index = i;
            TextView label = new TextView(this);
            label.setTextSize(15f);
            labels[i] = label;
            root.addView(label);

            SeekBar bar = new SeekBar(this);
            bar.setMax(100);
            bars[i] = bar;
            bar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                @Override public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                    if (updating[0] || !fromUser) return;
                    int v = Math.max(0, Math.min(100, progress));
                    if (index == 0) dreamX = v;
                    else if (index == 1) dreamY = v;
                    else if (index == 2) dreamP3 = v;
                    else if (index == 3) dreamP4 = v;
                    else dreamMix = v;

                    String name = index < 4
                            ? DREAM_PARAM_NAMES[dreamMode][index]
                            : "MIX";
                    labels[index].setText(name + "  " + v + "%");
                    applyDreamySettings();
                    persistDreamySettings();
                }
                @Override public void onStartTrackingTouch(SeekBar seekBar) { }
                @Override public void onStopTrackingTouch(SeekBar seekBar) { }
            });
            root.addView(bar);
        }

        final Runnable refresh = () -> {
            updating[0] = true;
            int[] values = new int[]{dreamX, dreamY, dreamP3, dreamP4, dreamMix};
            for (int i=0; i<5; i++) {
                String name = i < 4 ? DREAM_PARAM_NAMES[dreamMode][i] : "MIX";
                labels[i].setText(name + "  " + values[i] + "%");
                bars[i].setProgress(values[i]);
            }
            description.setText(DREAM_MODE_DESCRIPTIONS[dreamMode] +
                    "\nP1/P2 remain mapped to Dreamy X/Y (MIDI CC103/104).");
            updating[0] = false;
        };

        Button defaults = new Button(this);
        defaults.setText("MODE DEFAULT");
        defaults.setOnClickListener(v -> {
            int[] d = DREAM_DEFAULTS[dreamMode];
            dreamX = d[0];
            dreamY = d[1];
            dreamP3 = d[2];
            dreamP4 = d[3];
            dreamMix = d[4];
            refresh.run();
            applyDreamySettings();
            persistDreamySettings();
        });
        root.addView(defaults);

        final boolean[] firstSelection = {true};
        modeSpinner.setOnItemSelectedListener(new android.widget.AdapterView.OnItemSelectedListener() {
            @Override public void onItemSelected(android.widget.AdapterView<?> parent, View view,
                                                 int position, long id) {
                int mode = Math.max(0, Math.min(DREAM_MODE_NAMES.length - 1, position));
                if (firstSelection[0]) {
                    firstSelection[0] = false;
                    dreamMode = mode;
                    refresh.run();
                    return;
                }
                dreamMode = mode;
                refresh.run();
                applyDreamySettings();
                persistDreamySettings();
            }
            @Override public void onNothingSelected(android.widget.AdapterView<?> parent) { }
        });

        refresh.run();

        new AlertDialog.Builder(this)
                .setTitle("DREAMY / TEXTURE MODES")
                .setView(scrollDialogView(root))
                .setPositiveButton("CLOSE", null)
                .show();
    }

    private int audioBufferSelection(float value) {
        int best = 0;
        float bestDiff = Float.MAX_VALUE;
        for (int i=0; i<AUDIO_BUFFER_VALUES.length; i++) {
            float d = Math.abs(AUDIO_BUFFER_VALUES[i] - value);
            if (d < bestDiff) { bestDiff = d; best = i; }
        }
        return best;
    }

    private String audioBufferStats(int lastResult) {
        int fpb = NativeEngine.audioFramesPerBurst();
        int frames = NativeEngine.audioBufferSizeFrames();
        int capacity = NativeEngine.audioBufferCapacityFrames();
        int xruns = NativeEngine.audioXRunCount();
        String actual = fpb > 0
                ? String.format(java.util.Locale.US, "%.2f", frames / (float) fpb)
                : "N/A";
        String result = lastResult < 0 ? "\nRequest result: " + lastResult : "";
        return "Frames per burst: " + fpb +
                "\nActual buffer: " + frames + " frames" +
                "\nActual bursts: " + actual +
                "\nCapacity: " + capacity + " frames" +
                "\nXRuns: " + Math.max(0, xruns) + result;
    }

    private void showConfigDialog() {
        LinearLayout root = dialogRoot();

        TextView audioLabel = new TextView(this);
        audioLabel.setText("AUDIO BUFFER");
        audioLabel.setTextSize(16f);
        root.addView(audioLabel);

        Spinner audioSpinner = new Spinner(this);
        audioSpinner.setAdapter(new ArrayAdapter<>(this,
                android.R.layout.simple_spinner_dropdown_item, AUDIO_BUFFER_LABELS));
        final float originalAudioBuffer = audioBufferBursts;
        final int[] lastAudioResult = { 0 };
        audioSpinner.setSelection(audioBufferSelection(audioBufferBursts));
        root.addView(audioSpinner);

        TextView audioStats = new TextView(this);
        audioStats.setTextSize(13.5f);
        audioStats.setPadding(0, 0, 0, 14);
        audioStats.setText(audioBufferStats(0));
        root.addView(audioStats);

        audioSpinner.setOnItemSelectedListener(new android.widget.AdapterView.OnItemSelectedListener() {
            @Override public void onItemSelected(android.widget.AdapterView<?> parent, View view,
                                                 int position, long id) {
                float requested = AUDIO_BUFFER_VALUES[Math.max(0,
                        Math.min(AUDIO_BUFFER_VALUES.length - 1, position))];
                lastAudioResult[0] = NativeEngine.setAudioBufferBursts(requested);
                audioStats.setText(audioBufferStats(lastAudioResult[0]));
            }
            @Override public void onNothingSelected(android.widget.AdapterView<?> parent) { }
        });

        TextView instrumentLabel = new TextView(this);
        instrumentLabel.setText("INSTRUMENT");
        instrumentLabel.setTextSize(16f);
        root.addView(instrumentLabel);

        Spinner instrumentSpinner = new Spinner(this);
        instrumentSpinner.setAdapter(new ArrayAdapter<>(this,
                android.R.layout.simple_spinner_dropdown_item, INSTRUMENT_NAMES));
        instrumentSpinner.setSelection(instrumentMode);
        root.addView(instrumentSpinner);

        TextView midiLabel = new TextView(this);
        midiLabel.setText("MIDI CHANNEL FOR SELECTED INSTRUMENT");
        midiLabel.setTextSize(16f);
        root.addView(midiLabel);

        Spinner channelSpinner = new Spinner(this);
        String[] channels = new String[17];
        channels[0] = "OFF";
        for (int i=1;i<=16;i++) channels[i] = "CH " + i;
        channelSpinner.setAdapter(new ArrayAdapter<>(this,
                android.R.layout.simple_spinner_dropdown_item, channels));
        channelSpinner.setSelection(partMidiChannels[instrumentMode]);
        root.addView(channelSpinner);

        instrumentSpinner.setOnItemSelectedListener(new android.widget.AdapterView.OnItemSelectedListener() {
            @Override public void onItemSelected(android.widget.AdapterView<?> parent, View view, int position, long id) {
                int part = Math.max(0, Math.min(8, position));
                channelSpinner.setSelection(partMidiChannels[part]);
            }
            @Override public void onNothingSelected(android.widget.AdapterView<?> parent) { }
        });

        TextView routingHelp = new TextView(this);
        routingHelp.setText("Same CH on multiple instruments = layer. OFF = no external MIDI.");
        routingHelp.setTextSize(13f);
        root.addView(routingHelp);

        CheckBox sustain = new CheckBox(this);
        sustain.setText("Manual SUSTAIN");
        sustain.setChecked(manualSustain);
        root.addView(sustain);

        TextView presetLabel = new TextView(this);
        presetLabel.setText("\nPRESET");
        presetLabel.setTextSize(16f);
        root.addView(presetLabel);

        Spinner presetSpinner = new Spinner(this);
        String[] presetSlots = new String[PRESET_SLOTS];
        for (int i=0; i<PRESET_SLOTS; i++) presetSlots[i] = "SLOT " + (i + 1);
        presetSpinner.setAdapter(new ArrayAdapter<>(this,
                android.R.layout.simple_spinner_dropdown_item, presetSlots));
        root.addView(presetSpinner);

        LinearLayout presetButtons = new LinearLayout(this);
        presetButtons.setOrientation(LinearLayout.HORIZONTAL);
        Button savePreset = new Button(this);
        savePreset.setText("PRESET SAVE");
        Button loadPreset = new Button(this);
        loadPreset.setText("PRESET LOAD");
        presetButtons.addView(savePreset, new LinearLayout.LayoutParams(0,
                LinearLayout.LayoutParams.WRAP_CONTENT, 1f));
        presetButtons.addView(loadPreset, new LinearLayout.LayoutParams(0,
                LinearLayout.LayoutParams.WRAP_CONTENT, 1f));
        root.addView(presetButtons);

        savePreset.setOnClickListener(v -> {
            int slot = presetSpinner.getSelectedItemPosition();
            savePreset(slot);
            Toast.makeText(this, "PRESET " + (slot + 1) + " SAVED", Toast.LENGTH_SHORT).show();
        });
        loadPreset.setOnClickListener(v -> {
            int slot = presetSpinner.getSelectedItemPosition();
            if (loadPreset(slot)) {
                instrumentSpinner.setSelection(instrumentMode);
                channelSpinner.setSelection(partMidiChannels[instrumentMode]);
                sustain.setChecked(manualSustain);
                Toast.makeText(this, "PRESET " + (slot + 1) + " LOADED", Toast.LENGTH_SHORT).show();
            } else {
                Toast.makeText(this, "PRESET " + (slot + 1) + " EMPTY", Toast.LENGTH_SHORT).show();
            }
        });

        TextView cc = new TextView(this);
        cc.setText("\nMIDI CC\n" +
                "1 Modulation / 7 Volume / 10 Control 1 / 11 Control 2\n" +
                "64 Sustain / 74 Control 3\n" +
                "20 Boost / 21 Space Mode / 22 Space Mix / 23 Space Decay\n" +
                "24 Tape On-Off / 25 Wow / 26 Flutter / 27 Drive\n" +
                "28 Dreamy On-Off / 103 Dreamy X / 104 Dreamy Y / 105 Dreamy Mix");
        cc.setTextSize(14f);
        root.addView(cc);

        final AlertDialog[] holder = new AlertDialog[1];
        AlertDialog dialog = new AlertDialog.Builder(this).setTitle("CONFIG").setView(scrollDialogView(root))
                .setPositiveButton("APPLY", (d, which) -> {
                    int ai = Math.max(0, Math.min(AUDIO_BUFFER_VALUES.length - 1,
                            audioSpinner.getSelectedItemPosition()));
                    audioBufferBursts = AUDIO_BUFFER_VALUES[ai];
                    NativeEngine.setAudioBufferBursts(audioBufferBursts);

                    int selectedPart = Math.max(0, Math.min(8,
                            instrumentSpinner.getSelectedItemPosition()));
                    int newChannel = channelSpinner.getSelectedItemPosition();
                    if (newChannel != partMidiChannels[selectedPart]) {
                        NativeEngine.controlChangePart(selectedPart, 123, 0);
                    }
                    partMidiChannels[selectedPart] = newChannel;
                    if (midiController != null) midiController.setPartChannels(partMidiChannels);

                    applyInstrument(selectedPart);
                    manualSustain = sustain.isChecked();
                    int sus = manualSustain ? 127 : 0;
                    NativeEngine.controlChange(64, sus);
                    pianoView.controlChange(64, sus);

                    getSharedPreferences(PREFS, MODE_PRIVATE).edit()
                            .putFloat(KEY_AUDIO_BUFFER_BURSTS, audioBufferBursts)
                            .putInt(KEY_PART_MIDI_PREFIX + selectedPart, partMidiChannels[selectedPart])
                            .putBoolean(KEY_MANUAL_SUSTAIN, manualSustain).apply();
                })
                .setNegativeButton("CANCEL", (d, which) -> {
                    NativeEngine.setAudioBufferBursts(originalAudioBuffer);
                }).create();
        holder[0] = dialog;

        final Runnable refreshStats = new Runnable() {
            @Override public void run() {
                AlertDialog current = holder[0];
                if (current == null || !current.isShowing()) return;
                audioStats.setText(audioBufferStats(lastAudioResult[0]));
                audioStats.postDelayed(this, 500L);
            }
        };
        dialog.setOnShowListener(d -> audioStats.post(refreshStats));
        dialog.setOnCancelListener(d -> NativeEngine.setAudioBufferBursts(originalAudioBuffer));
        dialog.show();
    }

    private String[] modelControlNames() {
        switch (instrumentMode) {
            case 1: return new String[]{"EMBOUCHURE", "BREATH", "JET COLOR", "VIBRATO"};
            case 2: return new String[]{"REED PRESSURE", "BREATH", "BRIGHTNESS", "VIBRATO"};
            case 3: return new String[]{"HAMMER SOFTNESS", "STRIKE", "TONE", "MODULATION"};
            case 4: return new String[]{"REED PRESSURE", "BELLOWS", "MUSETTE", "TREMOLO"};
            case 5: return new String[]{"MALLET HARDNESS", "STRIKE", "TONE", "MODULATION"};
            case 6: return new String[]{"STRING DAMP", "PLUCK FORCE", "PLUCK POSITION", "VIBRATO"};
            case 7: return new String[]{"KICK TUNE", "HIHAT TUNE", "SNARE TUNE", "DECAY"};
            default: return new String[]{"BOW PRESSURE", "BOW SPEED", "BOW POSITION", "VIBRATO"};
        }
    }

    private String modelDescription() {
        switch (instrumentMode) {
            case 1: return "Jet-drive + lossy bore digital waveguide";
            case 2: return "Nonlinear single reed + two-section bore waveguide";
            case 3: return "Nonlinear felt contact + stiff-string modal resonators";
            case 4: return "Self-excited free reeds + pressure/airflow coupling";
            case 5: return "Mallet contact + inharmonic bar modal resonators";
            case 6: return "Fractional-delay pizzicato string + double-bass body / bridge modes";
            case 7: return "C4 Kick / C#4 Hi-hat / D4 Snare\nIndependent modal tuning: +/-12 semitones";
            case 8: return "EPBANK1 sample engine / 8 velocity layers / 3 RR / sustain + release";
            default: return "4 independent bowed-string voices / shared violin body";
        }
    }

    private void applyInstrument(int mode) {
        mode = Math.max(0, Math.min(8, mode));
        if (pianoView != null) pianoView.clearForegroundForInstrumentSwitch();

        instrumentMode = mode;
        NativeEngine.setInstrument(instrumentMode);

        if (epBackground != null) epBackground.setVisibility(instrumentMode == 8 ? View.VISIBLE : View.GONE);
        if (videoLayer != null) {
            videoLayer.setVisibility(instrumentMode == 8 ? View.GONE : View.VISIBLE);
            if (instrumentMode < 8) videoLayer.setInstrument(instrumentMode);
        }
        if (performanceXYView != null) performanceXYView.setInstrumentMode(instrumentMode);
        if (pianoView != null) {
            pianoView.setInstrumentName(
                    INSTRUMENT_NAMES[instrumentMode],
                    INSTRUMENT_BUTTONS[instrumentMode]);
            pianoView.setInstrumentMidiChannel(partMidiChannels[instrumentMode]);
            pianoView.setSampleMode(instrumentMode == 8);
            pianoView.controlChange(7, partMixerVolume[instrumentMode]);
            if (instrumentMode == 8) pianoView.setBankStatus(epBankStatus());
        }
        if (drumEditorView != null) {
            drumEditorView.setValues(drumParameters);
            drumEditorView.setDrumFx(partBoostDb[7], partDistortion[7]);
            drumEditorView.setDrumsVisible(instrumentMode == 7);
        }

        getSharedPreferences(PREFS, MODE_PRIVATE).edit()
                .putInt(KEY_INSTRUMENT, instrumentMode).apply();
    }

    @Override public void onRecorderRecord() {
        NativeEngine.recorderToggleRecording();
        pianoView.invalidate();
    }

    @Override public void onRecorderClear() {
        NativeEngine.recorderClear();
        pianoView.invalidate();
    }

    @Override public void onRecorderRandom() {
        NativeEngine.recorderToggleRandom();
        pianoView.invalidate();
    }

    @Override public void onRecorderClock() {
        NativeEngine.recorderToggleClock();
        pianoView.invalidate();
    }

    @Override public void onRecorderBpm(int bpm) {
        NativeEngine.recorderSetBpm(bpm);
        pianoView.invalidate();
    }

    @Override public void onRecorderTile(int slot) {
        if (NativeEngine.recorderIsRecording()
                && slot != NativeEngine.recorderRecordingSlot()
                && NativeEngine.recorderValidSamples(slot) == 0) {
            NativeEngine.recorderRecordSlot(slot);
        } else {
            NativeEngine.recorderPlaySlot(slot);
        }
        pianoView.invalidate();
    }

    @Override public void onChooseBank() {
        if (instrumentMode == 8) {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("application/octet-stream");
            intent.putExtra(Intent.EXTRA_MIME_TYPES,
                    new String[]{"application/octet-stream", "application/x-binary", "*/*"});
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                    | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
            startActivityForResult(intent, PICK_BANK);
            return;
        }

        LinearLayout root = dialogRoot();
        String[] names = modelControlNames();

        if (instrumentMode == 7) {
            if (drumEditorView != null) {
                drumEditorView.setValues(drumParameters);
                drumEditorView.setDrumFx(partBoostDb[7], partDistortion[7]);
                drumEditorView.setDrumsVisible(true);
            }
            return;
        }

        addSlider(root, names[0], 127, bowPressure, v -> {
            bowPressure = v;
            NativeEngine.controlChange(10, v);
            pianoView.controlChange(10, v);
        });
        addSlider(root, names[1], 127, bowSpeed, v -> {
            bowSpeed = v;
            NativeEngine.controlChange(11, v);
            pianoView.controlChange(11, v);
        });
        addSlider(root, names[2], 127, bowPosition, v -> {
            bowPosition = v;
            NativeEngine.controlChange(74, v);
            pianoView.controlChange(74, v);
        });
        addSlider(root, names[3], 127, vibratoDepth, v -> {
            vibratoDepth = v;
            NativeEngine.controlChange(1, v);
            pianoView.controlChange(1, v);
        });
        addSlider(root, "ATTACK ms", 2000, attackMs, v -> {
            attackMs = v;
            NativeEngine.setAdsr(attackMs, decayMs, sustainPct, releaseMs);
            getSharedPreferences(PREFS, MODE_PRIVATE).edit().putInt(KEY_ATTACK_MS, v).apply();
        });
        addSlider(root, "DECAY ms", 2000, decayMs, v -> {
            decayMs = v;
            NativeEngine.setAdsr(attackMs, decayMs, sustainPct, releaseMs);
            getSharedPreferences(PREFS, MODE_PRIVATE).edit().putInt(KEY_DECAY_MS, v).apply();
        });
        addSlider(root, "SUSTAIN %", 100, sustainPct, v -> {
            sustainPct = v;
            NativeEngine.setAdsr(attackMs, decayMs, sustainPct, releaseMs);
            getSharedPreferences(PREFS, MODE_PRIVATE).edit().putInt(KEY_SUSTAIN_PCT, v).apply();
        });
        addSlider(root, "RELEASE ms", 3000, releaseMs, v -> {
            releaseMs = v;
            NativeEngine.setAdsr(attackMs, decayMs, sustainPct, releaseMs);
            getSharedPreferences(PREFS, MODE_PRIVATE).edit().putInt(KEY_RELEASE_MS, v).apply();
        });

        addSlider(root, "BOOSTER dB", 18, partBoostDb[instrumentMode], v -> {
            partBoostDb[instrumentMode] = v;
            NativeEngine.setPartFx(instrumentMode, partBoostDb[instrumentMode],
                    partDistortion[instrumentMode]);
            getSharedPreferences(PREFS, MODE_PRIVATE).edit()
                    .putInt(KEY_PART_BOOST_PREFIX + instrumentMode, v).apply();
        });
        addSlider(root, "DISTORTION", 127, partDistortion[instrumentMode], v -> {
            partDistortion[instrumentMode] = v;
            NativeEngine.setPartFx(instrumentMode, partBoostDb[instrumentMode],
                    partDistortion[instrumentMode]);
            getSharedPreferences(PREFS, MODE_PRIVATE).edit()
                    .putInt(KEY_PART_DIST_PREFIX + instrumentMode, v).apply();
        });
        if (instrumentMode == 3) {
            addSlider(root, "REVERB MIX", 100, feltReverbMix, v -> {
                feltReverbMix = v;
                NativeEngine.setFeltReverb(feltReverbMix, feltReverbDecay);
                getSharedPreferences(PREFS, MODE_PRIVATE).edit()
                        .putInt(KEY_FELT_REVERB_MIX, v).apply();
            });
            addSlider(root, "REVERB DECAY", 100, feltReverbDecay, v -> {
                feltReverbDecay = v;
                NativeEngine.setFeltReverb(feltReverbMix, feltReverbDecay);
                getSharedPreferences(PREFS, MODE_PRIVATE).edit()
                        .putInt(KEY_FELT_REVERB_DECAY, v).apply();
            });
        }
        new AlertDialog.Builder(this)
                .setTitle(INSTRUMENT_NAMES[instrumentMode] + " MODEL")
                .setMessage(modelDescription())
                .setView(scrollDialogView(root))
                .setPositiveButton("CLOSE", null)
                .show();
    }

    @Override public void onDrumParameterChanged(int parameter, int value) {
        if (parameter < 0 || parameter >= drumParameters.length) return;
        value = Math.max(0, Math.min(127, value));
        drumParameters[parameter] = value;
        NativeEngine.setDrumParameter(parameter, value);
        getSharedPreferences(PREFS, MODE_PRIVATE).edit()
                .putInt(KEY_DRUM_PARAM_PREFIX + parameter, value).apply();
    }

    @Override public void onPerformanceXY(boolean active, int part, int x, int y) {
        NativeEngine.setPerformanceXY(active, part, x, y);
    }

    @Override public void onDrumFxChanged(int boostDb, int distortion) {
        drumBoostDb = Math.max(0, Math.min(18, boostDb));
        drumDistortion = Math.max(0, Math.min(127, distortion));
        partBoostDb[7] = drumBoostDb;
        partDistortion[7] = drumDistortion;
        NativeEngine.setDrumFx(drumBoostDb, drumDistortion);
        NativeEngine.setPartFx(7, drumBoostDb, drumDistortion);
        getSharedPreferences(PREFS, MODE_PRIVATE).edit()
                .putInt(KEY_DRUM_BOOST_DB, drumBoostDb)
                .putInt(KEY_DRUM_DISTORTION, drumDistortion)
                .putInt(KEY_PART_BOOST_PREFIX + 7, drumBoostDb)
                .putInt(KEY_PART_DIST_PREFIX + 7, drumDistortion)
                .apply();
    }


    private void savePreset(int slot) {
        if (slot < 0 || slot >= PRESET_SLOTS) return;
        String p = "slot_" + slot + "_";
        android.content.SharedPreferences.Editor e =
                getSharedPreferences(PRESET_PREFS, MODE_PRIVATE).edit();

        e.putBoolean(p + "valid", true)
                .putInt(p + "instrument", instrumentMode)
                .putBoolean(p + "manual_sustain", manualSustain)
                .putInt(p + "boost_db", boostDb)
                .putInt(p + "space_mode", spaceMode)
                .putInt(p + "space_mix", spaceMix)
                .putInt(p + "space_decay", spaceDecay)
                .putBoolean(p + "tape", tape)
                .putInt(p + "tape_wow", tapeWow)
                .putInt(p + "tape_flutter", tapeFlutter)
                .putInt(p + "tape_drive", tapeDrive)
                .putBoolean(p + "dreamy", dreamy)
                .putInt(p + "dream_x", dreamX)
                .putInt(p + "dream_y", dreamY)
                .putInt(p + "dream_p3", dreamP3)
                .putInt(p + "dream_p4", dreamP4)
                .putInt(p + "dream_mode", dreamMode)
                .putInt(p + "dream_mix", dreamMix)
                .putInt(p + "control1", bowPressure)
                .putInt(p + "control2", bowSpeed)
                .putInt(p + "control3", bowPosition)
                .putInt(p + "mod", vibratoDepth)
                .putInt(p + "attack", attackMs)
                .putInt(p + "decay", decayMs)
                .putInt(p + "sustain", sustainPct)
                .putInt(p + "release", releaseMs)
                .putInt(p + "drum_boost", partBoostDb[7])
                .putInt(p + "drum_dist", partDistortion[7])
                .putInt(p + "felt_reverb_mix", feltReverbMix)
                .putInt(p + "felt_reverb_decay", feltReverbDecay);
        for (int i=0; i<partMidiChannels.length; i++) {
            e.putInt(p + "midi_" + i, partMidiChannels[i]);
            e.putInt(p + "part_boost_" + i, partBoostDb[i]);
            e.putInt(p + "part_dist_" + i, partDistortion[i]);
        }
        for (int i=0; i<drumParameters.length; i++) {
            e.putInt(p + "drum_" + i, drumParameters[i]);
        }
        e.apply();
    }

    private boolean loadPreset(int slot) {
        if (slot < 0 || slot >= PRESET_SLOTS) return false;
        String p = "slot_" + slot + "_";
        android.content.SharedPreferences sp = getSharedPreferences(PRESET_PREFS, MODE_PRIVATE);
        if (!sp.getBoolean(p + "valid", false)) return false;

        instrumentMode = Math.max(0, Math.min(8, sp.getInt(p + "instrument", instrumentMode)));
        manualSustain = sp.getBoolean(p + "manual_sustain", manualSustain);
        boostDb = Math.max(0, Math.min(6, sp.getInt(p + "boost_db", boostDb)));
        boosterStep = Math.min(3, Math.round(boostDb / 2f));
        spaceMode = Math.max(0, Math.min(3, sp.getInt(p + "space_mode", spaceMode)));
        spaceMix = Math.max(0, Math.min(100, sp.getInt(p + "space_mix", spaceMix)));
        spaceDecay = Math.max(0, Math.min(100, sp.getInt(p + "space_decay", spaceDecay)));
        tape = sp.getBoolean(p + "tape", tape);
        tapeWow = Math.max(0, Math.min(100, sp.getInt(p + "tape_wow", tapeWow)));
        tapeFlutter = Math.max(0, Math.min(100, sp.getInt(p + "tape_flutter", tapeFlutter)));
        tapeDrive = Math.max(0, Math.min(100, sp.getInt(p + "tape_drive", tapeDrive)));
        dreamy = sp.getBoolean(p + "dreamy", dreamy);
        dreamX = Math.max(0, Math.min(100, sp.getInt(p + "dream_x", dreamX)));
        dreamY = Math.max(0, Math.min(100, sp.getInt(p + "dream_y", dreamY)));
        dreamP3 = Math.max(0, Math.min(100, sp.getInt(p + "dream_p3", dreamP3)));
        dreamP4 = Math.max(0, Math.min(100, sp.getInt(p + "dream_p4", dreamP4)));
        dreamMode = Math.max(0, Math.min(DREAM_MODE_NAMES.length - 1,
                sp.getInt(p + "dream_mode", dreamMode)));
        dreamMix = Math.max(0, Math.min(100, sp.getInt(p + "dream_mix", dreamMix)));
        bowPressure = Math.max(0, Math.min(127, sp.getInt(p + "control1", bowPressure)));
        bowSpeed = Math.max(0, Math.min(127, sp.getInt(p + "control2", bowSpeed)));
        bowPosition = Math.max(0, Math.min(127, sp.getInt(p + "control3", bowPosition)));
        vibratoDepth = Math.max(0, Math.min(127, sp.getInt(p + "mod", vibratoDepth)));
        attackMs = Math.max(0, Math.min(5000, sp.getInt(p + "attack", attackMs)));
        decayMs = Math.max(0, Math.min(5000, sp.getInt(p + "decay", decayMs)));
        sustainPct = Math.max(0, Math.min(100, sp.getInt(p + "sustain", sustainPct)));
        releaseMs = Math.max(0, Math.min(5000, sp.getInt(p + "release", releaseMs)));
        drumBoostDb = Math.max(0, Math.min(18, sp.getInt(p + "drum_boost", drumBoostDb)));
        drumDistortion = Math.max(0, Math.min(127, sp.getInt(p + "drum_dist", drumDistortion)));
        feltReverbMix = Math.max(0, Math.min(100,
                sp.getInt(p + "felt_reverb_mix", feltReverbMix)));
        feltReverbDecay = Math.max(0, Math.min(100,
                sp.getInt(p + "felt_reverb_decay", feltReverbDecay)));

        for (int i=0; i<partMidiChannels.length; i++) {
            partMidiChannels[i] = Math.max(0, Math.min(16,
                    sp.getInt(p + "midi_" + i, partMidiChannels[i])));
            int boostFallback = (i == 7) ? drumBoostDb : partBoostDb[i];
            int distFallback = (i == 7) ? drumDistortion : partDistortion[i];
            partBoostDb[i] = Math.max(0, Math.min(18,
                    sp.getInt(p + "part_boost_" + i, boostFallback)));
            partDistortion[i] = Math.max(0, Math.min(127,
                    sp.getInt(p + "part_dist_" + i, distFallback)));
        }
        drumBoostDb = partBoostDb[7];
        drumDistortion = partDistortion[7];
        for (int i=0; i<drumParameters.length; i++) {
            drumParameters[i] = Math.max(0, Math.min(127, sp.getInt(p + "drum_" + i, drumParameters[i])));
        }

        if (midiController != null) midiController.setPartChannels(partMidiChannels);
        applyInstrument(instrumentMode);

        NativeEngine.setBoostDb(boostDb);
        NativeEngine.setSpaceMode(spaceMode);
        NativeEngine.setSpaceParameters(spaceMix, spaceDecay);
        NativeEngine.setTape(tape);
        NativeEngine.setTapeParameters(tapeWow, tapeFlutter, tapeDrive);
        NativeEngine.setDreamy(dreamy);
        NativeEngine.setDreamyMode(dreamMode);
        NativeEngine.setDreamyParameters(dreamX, dreamY, dreamMix);
        NativeEngine.setDreamyExtraParameters(dreamP3, dreamP4);
        for (int i=0; i<drumParameters.length; i++) NativeEngine.setDrumParameter(i, drumParameters[i]);
        for (int part=0; part<partBoostDb.length; part++) {
            NativeEngine.setPartFx(part, partBoostDb[part], partDistortion[part]);
        }
        NativeEngine.setDrumFx(partBoostDb[7], partDistortion[7]);
        NativeEngine.setFeltReverb(feltReverbMix, feltReverbDecay);

        if (instrumentMode < 7) {
            NativeEngine.controlChange(10, bowPressure);
            NativeEngine.controlChange(11, bowSpeed);
            NativeEngine.controlChange(74, bowPosition);
            NativeEngine.controlChange(1, vibratoDepth);
            NativeEngine.setAdsr(attackMs, decayMs, sustainPct, releaseMs);
        }
        NativeEngine.controlChange(64, manualSustain ? 127 : 0);

        pianoView.setBoostDb(boostDb);
        pianoView.setSpaceMode(spaceMode);
        pianoView.setTape(tape);
        pianoView.setDreamy(dreamy);
        pianoView.controlChange(10, bowPressure);
        pianoView.controlChange(11, bowSpeed);
        pianoView.controlChange(74, bowPosition);
        pianoView.controlChange(1, vibratoDepth);
        pianoView.controlChange(64, manualSustain ? 127 : 0);
        pianoView.controlChange(103, Math.max(0, Math.min(127, Math.round(dreamX * 1.27f))));
        pianoView.controlChange(104, Math.max(0, Math.min(127, Math.round(dreamY * 1.27f))));
        if (drumEditorView != null) {
            drumEditorView.setValues(drumParameters);
            drumEditorView.setDrumFx(partBoostDb[7], partDistortion[7]);
        }

        persistLoadedPresetAsCurrent();
        return true;
    }

    private void persistLoadedPresetAsCurrent() {
        android.content.SharedPreferences.Editor e = getSharedPreferences(PREFS, MODE_PRIVATE).edit()
                .putInt(KEY_INSTRUMENT, instrumentMode)
                .putBoolean(KEY_MANUAL_SUSTAIN, manualSustain)
                .putInt(KEY_BOOST_DB, boostDb)
                .putInt(KEY_BOOST, boosterStep)
                .putInt(KEY_SPACE, spaceMode)
                .putInt(KEY_SPACE_MIX, spaceMix)
                .putInt(KEY_SPACE_DECAY, spaceDecay)
                .putBoolean(KEY_TAPE, tape)
                .putInt(KEY_TAPE_WOW, tapeWow)
                .putInt(KEY_TAPE_FLUTTER, tapeFlutter)
                .putInt(KEY_TAPE_DRIVE, tapeDrive)
                .putBoolean(KEY_DREAMY, dreamy)
                .putInt(KEY_DREAM_X, dreamX)
                .putInt(KEY_DREAM_Y, dreamY)
                .putInt(KEY_DREAM_MIX, dreamMix)
                .putInt(KEY_ATTACK_MS, attackMs)
                .putInt(KEY_DECAY_MS, decayMs)
                .putInt(KEY_SUSTAIN_PCT, sustainPct)
                .putInt(KEY_RELEASE_MS, releaseMs)
                .putInt(KEY_DRUM_BOOST_DB, partBoostDb[7])
                .putInt(KEY_DRUM_DISTORTION, partDistortion[7])
                .putInt(KEY_FELT_REVERB_MIX, feltReverbMix)
                .putInt(KEY_FELT_REVERB_DECAY, feltReverbDecay);
        for (int i=0; i<partMidiChannels.length; i++) {
            e.putInt(KEY_PART_MIDI_PREFIX + i, partMidiChannels[i]);
            e.putInt(KEY_PART_BOOST_PREFIX + i, partBoostDb[i]);
            e.putInt(KEY_PART_DIST_PREFIX + i, partDistortion[i]);
        }
        for (int i=0; i<drumParameters.length; i++) e.putInt(KEY_DRUM_PARAM_PREFIX + i, drumParameters[i]);
        e.apply();
    }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != PICK_BANK || resultCode != RESULT_OK || data == null) return;
        Uri uri = data.getData();
        if (uri == null) return;
        try {
            final int flags = data.getFlags() &
                    (Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            getContentResolver().takePersistableUriPermission(uri, flags & Intent.FLAG_GRANT_READ_URI_PERMISSION);
        } catch (SecurityException ignored) {
        }
        getSharedPreferences(PREFS, MODE_PRIVATE).edit().putString(KEY_BANK_URI, uri.toString()).apply();
        loadBankUri(uri);
    }

    @Override protected void onResume() {
        super.onResume();
        if (videoLayer != null) videoLayer.resumeFromLifecycle();
    }

    private void panicAllParts() {
        for (int part=0; part<partMidiChannels.length; part++) NativeEngine.controlChangePart(part, 123, 0);
    }

    @Override protected void onPause() {
        if (performanceXYView != null) performanceXYView.cancelEffect();
        if (pianoView != null) pianoView.panicAuditionKeyboard();
        if (videoLayer != null) videoLayer.pauseForLifecycle();
        panicAllParts();
        super.onPause();
    }

    @Override protected void onDestroy() {
        if (midiController != null) midiController.stop();
        if (videoLayer != null) videoLayer.release();
        panicAllParts();
        NativeEngine.stop();
        super.onDestroy();
    }

    @Override public void onNoteOn(int part, int note, int velocity) {
        if (part != instrumentMode) return;
        runOnUiThread(() -> pianoView.noteOn(note, velocity));
    }

    @Override public void onNoteOff(int part, int note, int velocity) {
        if (part != instrumentMode) return;
        runOnUiThread(() -> pianoView.noteOff(note));
    }

    @Override public void onPolyPressure(int part, int note, int value) {
        if (part != instrumentMode) return;
        runOnUiThread(() -> pianoView.polyPressure(note, value));
    }

    @Override public void onChannelPressure(int part, int value) {
        if (part != instrumentMode) return;
        runOnUiThread(() -> pianoView.setChannelPressure(value));
    }

    @Override public void onControlChange(int part, int cc, int value) {
        runOnUiThread(() -> {
            final boolean selectedPart = part == instrumentMode;

            if (cc == 7 && part >= 0 && part < partMixerVolume.length) {
                partMixerVolume[part] = Math.max(0, Math.min(127, value));
                if (mixerView != null) mixerView.setPartVolume(part, partMixerVolume[part]);
            }

            if (selectedPart) {
                pianoView.controlChange(cc, value);
                if (instrumentMode == 7) {
                    if (cc == 10) drumParameters[0] = value;
                    else if (cc == 11) drumParameters[4] = value;
                    else if (cc == 74) drumParameters[8] = value;
                    else if (cc == 1) {
                        drumParameters[1] = value;
                        drumParameters[5] = value;
                        drumParameters[9] = value;
                    } else if (cc == 64) manualSustain = value >= 64;
                    if (drumEditorView != null) drumEditorView.setValues(drumParameters);
                } else if (instrumentMode < 7) {
                    if (cc == 1) vibratoDepth = value;
                    else if (cc == 10) bowPressure = value;
                    else if (cc == 11) bowSpeed = value;
                    else if (cc == 74) bowPosition = value;
                    else if (cc == 64) manualSustain = value >= 64;
                } else if (instrumentMode == 8 && cc == 64) {
                    manualSustain = value >= 64;
                }
            }

            // Shared FX bus controls are reflected in the UI regardless of
            // which assigned part generated them.
            if (cc == 20) { boostDb = Math.round(value * 6f / 127f); pianoView.setBoostDb(boostDb); }
            else if (cc == 21) { spaceMode = Math.round(value * 3f / 127f); pianoView.setSpaceMode(spaceMode); }
            else if (cc == 22) spaceMix = Math.round(value * 100f / 127f);
            else if (cc == 23) spaceDecay = Math.round(value * 100f / 127f);
            else if (cc == 24) { tape = value >= 64; pianoView.setTape(tape); }
            else if (cc == 25) tapeWow = Math.round(value * 100f / 127f);
            else if (cc == 26) tapeFlutter = Math.round(value * 100f / 127f);
            else if (cc == 27) tapeDrive = Math.round(value * 100f / 127f);
            else if (cc == 28) { dreamy = value >= 64; pianoView.setDreamy(dreamy); }
            else if (cc == 103) dreamX = Math.round(value * 100f / 127f);
            else if (cc == 104) dreamY = Math.round(value * 100f / 127f);
            else if (cc == 105) dreamMix = Math.round(value * 100f / 127f);
        });
    }

    @Override public void onPitchBend(int part, int value14) {
        if (part != instrumentMode) return;
        runOnUiThread(() -> pianoView.setPitchBend(value14));
    }

    @Override public void onConnectionCountChanged(int count) {
        runOnUiThread(() -> pianoView.setMidiConnections(count));
    }

}
