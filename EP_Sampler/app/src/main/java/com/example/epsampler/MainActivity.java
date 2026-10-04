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
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.TextView;


public class MainActivity extends Activity implements MidiController.Listener, PianoView.ActionListener {
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
    private static final String KEY_PART_MIDI_PREFIX = "part_midi_ch_";
    private static final String KEY_MANUAL_SUSTAIN = "manual_sustain";
    private static final String KEY_ATTACK_MS = "attack_ms";
    private static final String KEY_DECAY_MS = "decay_ms";
    private static final String KEY_SUSTAIN_PCT = "sustain_pct";
    private static final String KEY_RELEASE_MS = "release_ms";
    private static final String KEY_INSTRUMENT = "instrument_mode";
    private static final String KEY_DRUM_KICK_TUNE = "drum_kick_tune";
    private static final String KEY_DRUM_HAT_TUNE = "drum_hat_tune";
    private static final String KEY_DRUM_SNARE_TUNE = "drum_snare_tune";
    private static final String KEY_DRUM_DECAY = "drum_decay";

    private static final String[] INSTRUMENT_NAMES = new String[] {
            "VIOLIN", "FLUTE", "SAXOPHONE", "FELT PIANO",
            "PIANICA / ACCORDION", "XYLOPHONE", "WOOD BASS", "DRUMS"
    };
    private static final String[] INSTRUMENT_BUTTONS = new String[] {
            "VIOLIN", "FLUTE", "SAX", "FELT",
            "ACCORD", "XYLO", "BASS", "DRUMS"
    };
    private PianoView pianoView;
    private PerformanceVideoLayer videoLayer;
    private MidiController midiController;
    private volatile boolean dreamy = false;
    private int boosterStep = 0;
    private int spaceMode = 0;
    private boolean tape = false;
    private int boostDb = 0;
    private int spaceMix = 50, spaceDecay = 50;
    private int tapeWow = 50, tapeFlutter = 50, tapeDrive = 50;
    private int dreamX = 28, dreamY = 28, dreamMix = 34;
    private final int[] partMidiChannels = new int[]{1,2,3,4,5,6,7,8};
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
    private int drumKickTune = 64;
    private int drumHatTune = 64;
    private int drumSnareTune = 64;
    private int drumDecay = 64;

    @Override protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        hideSystemUI();
        videoLayer = new PerformanceVideoLayer(this);
        pianoView = new PianoView(this);
        pianoView.setPerformanceVideoLayer(videoLayer);
        pianoView.setActionListener(this);

        FrameLayout root = new FrameLayout(this);
        root.addView(videoLayer, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));
        root.addView(pianoView, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));
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
        dreamX = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_DREAM_X, 28);
        dreamY = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_DREAM_Y, 28);
        dreamMix = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_DREAM_MIX, 34);
        for (int i=0; i<partMidiChannels.length; i++) {
            partMidiChannels[i] = Math.max(0, Math.min(16,
                    getSharedPreferences(PREFS, MODE_PRIVATE)
                            .getInt(KEY_PART_MIDI_PREFIX + i, i + 1)));
        }
        manualSustain = getSharedPreferences(PREFS, MODE_PRIVATE).getBoolean(KEY_MANUAL_SUSTAIN, false);
        attackMs = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_ATTACK_MS, 20);
        decayMs = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_DECAY_MS, 120);
        sustainPct = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_SUSTAIN_PCT, 90);
        releaseMs = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_RELEASE_MS, 300);
        instrumentMode = Math.max(0, Math.min(7,
                getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_INSTRUMENT, 0)));
        drumKickTune = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_DRUM_KICK_TUNE, 64);
        drumHatTune = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_DRUM_HAT_TUNE, 64);
        drumSnareTune = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_DRUM_SNARE_TUNE, 64);
        drumDecay = getSharedPreferences(PREFS, MODE_PRIVATE).getInt(KEY_DRUM_DECAY, 64);

        pianoView.setBoosterStep(boosterStep);
        pianoView.setBoostDb(boostDb);
        pianoView.setSpaceMode(spaceMode);
        pianoView.setTape(tape);
        pianoView.setDreamy(dreamy);
        pianoView.setInstrumentName(INSTRUMENT_NAMES[instrumentMode], INSTRUMENT_BUTTONS[instrumentMode]);
        pianoView.setInstrumentMidiChannel(partMidiChannels[instrumentMode]);
        videoLayer.setInstrument(instrumentMode);

        NativeEngine.start();
        NativeEngine.setBoosterStep(boosterStep);
        NativeEngine.setBoostDb(boostDb);
        NativeEngine.setSpaceMode(spaceMode);
        NativeEngine.setSpaceParameters(spaceMix, spaceDecay);
        NativeEngine.setTape(tape);
        NativeEngine.setTapeParameters(tapeWow, tapeFlutter, tapeDrive);
        NativeEngine.setDreamy(dreamy);
        NativeEngine.setDreamyParameters(dreamX, dreamY, dreamMix);
        pianoView.controlChange(103, Math.max(0, Math.min(127, Math.round(dreamX * 1.27f))));
        pianoView.controlChange(104, Math.max(0, Math.min(127, Math.round(dreamY * 1.27f))));
        NativeEngine.setInstrument(instrumentMode);
        if (instrumentMode == 7) {
            NativeEngine.controlChange(10, drumKickTune);
            NativeEngine.controlChange(11, drumHatTune);
            NativeEngine.controlChange(74, drumSnareTune);
            NativeEngine.controlChange(1, drumDecay);
            pianoView.controlChange(10, drumKickTune);
            pianoView.controlChange(11, drumHatTune);
            pianoView.controlChange(74, drumSnareTune);
            pianoView.controlChange(1, drumDecay);
        } else {
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

    private void loadExistingBank() {
        String saved = getSharedPreferences(PREFS, MODE_PRIVATE).getString(KEY_BANK_URI, null);
        if (saved == null || saved.isEmpty()) {
            pianoView.setBankStatus("BANK —");
            return;
        }
        loadBankUri(Uri.parse(saved));
    }

    private void loadBankUri(Uri uri) {
        pianoView.setBankStatus("BANK LOADING…");
        new Thread(() -> {
            boolean ok = false;
            try (ParcelFileDescriptor pfd = getContentResolver().openFileDescriptor(uri, "r")) {
                if (pfd != null) ok = NativeEngine.loadBankFd(pfd.getFd());
            } catch (Exception ignored) {
            }
            final boolean result = ok;
            runOnUiThread(() -> pianoView.setBankStatus(result ? NativeEngine.bankStatus() : "BANK ERROR"));
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

    private LinearLayout dialogRoot() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        int p = Math.round(18f * getResources().getDisplayMetrics().density);
        root.setPadding(p, p / 2, p, p / 2);
        return root;
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
        new AlertDialog.Builder(this).setTitle("BOOST").setView(root)
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
        new AlertDialog.Builder(this).setTitle("SPACE").setView(root)
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
        new AlertDialog.Builder(this).setTitle("TAPE").setView(root)
                .setPositiveButton("CLOSE", null).show();
    }

    private void showDreamyDialog() {
        LinearLayout root = dialogRoot();
        addSlider(root, "X %", 100, dreamX, v -> {
            dreamX = v;
            NativeEngine.setDreamyParameters(dreamX, dreamY, dreamMix);
            pianoView.controlChange(103, Math.max(0, Math.min(127, Math.round(v * 1.27f))));
            getSharedPreferences(PREFS, MODE_PRIVATE).edit().putInt(KEY_DREAM_X, v).apply();
        });
        addSlider(root, "Y %", 100, dreamY, v -> {
            dreamY = v;
            NativeEngine.setDreamyParameters(dreamX, dreamY, dreamMix);
            pianoView.controlChange(104, Math.max(0, Math.min(127, Math.round(v * 1.27f))));
            getSharedPreferences(PREFS, MODE_PRIVATE).edit().putInt(KEY_DREAM_Y, v).apply();
        });
        addSlider(root, "MIX %", 100, dreamMix, v -> {
            dreamMix = v;
            NativeEngine.setDreamyParameters(dreamX, dreamY, dreamMix);
            getSharedPreferences(PREFS, MODE_PRIVATE).edit().putInt(KEY_DREAM_MIX, v).apply();
        });
        new AlertDialog.Builder(this).setTitle("DREAMY").setView(root)
                .setPositiveButton("CLOSE", null).show();
    }

    private void showConfigDialog() {
        LinearLayout root = dialogRoot();

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
                int part = Math.max(0, Math.min(7, position));
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

        TextView cc = new TextView(this);
        cc.setText("\nMIDI CC\n" +
                "1 Modulation / 7 Volume / 10 Control 1 / 11 Control 2\n" +
                "64 Sustain / 74 Control 3\n" +
                "20 Boost / 21 Space Mode / 22 Space Mix / 23 Space Decay\n" +
                "24 Tape On-Off / 25 Wow / 26 Flutter / 27 Drive\n" +
                "28 Dreamy On-Off / 103 Dreamy X / 104 Dreamy Y / 105 Dreamy Mix");
        cc.setTextSize(14f);
        root.addView(cc);

        new AlertDialog.Builder(this).setTitle("CONFIG").setView(root)
                .setPositiveButton("APPLY", (dialog, which) -> {
                    int selectedPart = Math.max(0, Math.min(7,
                            instrumentSpinner.getSelectedItemPosition()));
                    int newChannel = channelSpinner.getSelectedItemPosition();
                    if (newChannel != partMidiChannels[selectedPart]) {
                        // A channel reassignment is a routing boundary. Clear
                        // this part once so notes held on the old channel cannot stick.
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
                            .putInt(KEY_PART_MIDI_PREFIX + selectedPart, partMidiChannels[selectedPart])
                            .putBoolean(KEY_MANUAL_SUSTAIN, manualSustain).apply();
                })
                .setNegativeButton("CANCEL", null).show();
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
            case 6: return "Plucked lossy string + upright-bass body modes";
            case 7: return "C4 Kick / C#4 Hi-hat / D4 Snare\nIndependent modal tuning: +/-12 semitones";
            default: return "4 independent bowed-string voices / shared violin body";
        }
    }

    private void applyInstrument(int mode) {
        mode = Math.max(0, Math.min(7, mode));
        if (pianoView != null) pianoView.clearForegroundForInstrumentSwitch();

        instrumentMode = mode;
        NativeEngine.setInstrument(instrumentMode);
        if (instrumentMode == 7) {
            NativeEngine.controlChange(10, drumKickTune);
            NativeEngine.controlChange(11, drumHatTune);
            NativeEngine.controlChange(74, drumSnareTune);
            NativeEngine.controlChange(1, drumDecay);
        }

        if (videoLayer != null) videoLayer.setInstrument(instrumentMode);
        if (pianoView != null) {
            pianoView.setInstrumentName(
                    INSTRUMENT_NAMES[instrumentMode],
                    INSTRUMENT_BUTTONS[instrumentMode]);
            pianoView.setInstrumentMidiChannel(partMidiChannels[instrumentMode]);
        }

        getSharedPreferences(PREFS, MODE_PRIVATE).edit()
                .putInt(KEY_INSTRUMENT, instrumentMode).apply();
    }

    @Override public void onChooseBank() {
        LinearLayout root = dialogRoot();
        String[] names = modelControlNames();

        if (instrumentMode == 7) {
            addSlider(root, names[0], 127, drumKickTune, v -> {
                drumKickTune = v;
                NativeEngine.controlChange(10, v);
                pianoView.controlChange(10, v);
                getSharedPreferences(PREFS, MODE_PRIVATE).edit()
                        .putInt(KEY_DRUM_KICK_TUNE, v).apply();
            });
            addSlider(root, names[1], 127, drumHatTune, v -> {
                drumHatTune = v;
                NativeEngine.controlChange(11, v);
                pianoView.controlChange(11, v);
                getSharedPreferences(PREFS, MODE_PRIVATE).edit()
                        .putInt(KEY_DRUM_HAT_TUNE, v).apply();
            });
            addSlider(root, names[2], 127, drumSnareTune, v -> {
                drumSnareTune = v;
                NativeEngine.controlChange(74, v);
                pianoView.controlChange(74, v);
                getSharedPreferences(PREFS, MODE_PRIVATE).edit()
                        .putInt(KEY_DRUM_SNARE_TUNE, v).apply();
            });
            addSlider(root, names[3], 127, drumDecay, v -> {
                drumDecay = v;
                NativeEngine.controlChange(1, v);
                pianoView.controlChange(1, v);
                getSharedPreferences(PREFS, MODE_PRIVATE).edit()
                        .putInt(KEY_DRUM_DECAY, v).apply();
            });
            new AlertDialog.Builder(this)
                    .setTitle("DRUMS MODEL")
                    .setMessage(modelDescription())
                    .setView(root)
                    .setPositiveButton("CLOSE", null)
                    .show();
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
        new AlertDialog.Builder(this)
                .setTitle(INSTRUMENT_NAMES[instrumentMode] + " MODEL")
                .setMessage(modelDescription())
                .setView(root)
                .setPositiveButton("CLOSE", null)
                .show();
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
        for (int part=0; part<8; part++) NativeEngine.controlChangePart(part, 123, 0);
    }

    @Override protected void onPause() {
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

            if (selectedPart) {
                pianoView.controlChange(cc, value);
                if (instrumentMode == 7) {
                    if (cc == 1) drumDecay = value;
                    else if (cc == 10) drumKickTune = value;
                    else if (cc == 11) drumHatTune = value;
                    else if (cc == 74) drumSnareTune = value;
                    else if (cc == 64) manualSustain = value >= 64;
                } else {
                    if (cc == 1) vibratoDepth = value;
                    else if (cc == 10) bowPressure = value;
                    else if (cc == 11) bowSpeed = value;
                    else if (cc == 74) bowPosition = value;
                    else if (cc == 64) manualSustain = value >= 64;
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
