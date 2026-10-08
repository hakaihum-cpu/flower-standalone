#pragma once
#include <aaudio/AAudio.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include "SampleBank.h"
#include "PhysicalViolin.h"
#include "InstrumentModels.h"
#include "DreamyEffect.h"
#include "SpaceEffect.h"
#include "TapeEffect.h"
#include "IntegratedRecorder.h"

class AudioEngine {
public:
    static AudioEngine& instance();
    bool start();
    void stop();

    // SAMPLE parts occupy MIDI parts 8..15 (CH9..16 by default).
    // Slot loads happen into an inactive mmap bank and are swapped on the
    // audio thread without holding a file-I/O mutex in the callback.
    static constexpr int SAMPLE_BANK_COUNT = 8;
    bool loadBank(const std::string& path);              // legacy slot 0
    bool loadBankFd(int fd);                             // legacy slot 0
    bool loadBankSlotFd(int slot, int fd);
    bool bankLoaded() const { return bankLoaded(0); }    // legacy slot 0
    bool bankLoaded(int slot) const;
    std::string bankStatus() const { return bankStatus(0); }
    std::string bankStatus(int slot) const;

    void noteOn(int note, int velocity);
    void noteOff(int note, int velocity);
    void polyPressure(int note, int pressure);
    void channelPressure(int pressure);
    void controlChange(int cc, int value);
    void pitchBend(int value14);
    void setDreamy(bool enabled);
    void setBoosterStep(int step);
    void setBoostDb(int db);
    void setSpaceMode(int mode);
    void setSpaceParameters(int mix, int decay);
    void setTape(bool enabled);
    void setTapeParameters(int wow, int flutter, int drive);
    void setDreamyParameters(int x, int y, int mix);
    void setDreamyMode(int mode);
    void setDreamyExtraParameters(int p3, int p4);
    void setAdsr(int attackMs, int decayMs, int sustainPct, int releaseMs);
    void setInstrument(int instrument);
    void setDrumParameter(int parameter, int value);
    void setDrumFx(int boostDb, int distortion);
    void setPartFx(int part, int boostDb, int distortion);
    void setPartMixer(int part, int volume, int pan, bool muted);
    void setDrumSampleMixer(int slot, int volume, int pan, bool muted);
    void setFeltReverb(int mix, int decay);
    void setPerformanceXY(bool active, int part, int x, int y);
    int setAudioOutputDevice(int deviceId);
    int audioDeviceId() const;
    std::string audioPerformanceModeName() const;
    std::string audioSharingModeName() const;
    int setAudioBufferBursts(float bursts);
    int audioFramesPerBurst() const;
    int audioBufferSizeFrames() const;
    int audioBufferCapacityFrames() const;
    int audioXRunCount() const;
    int audioSampleRate() const { return sampleRate_; }
    float partMeter(int part) const;
    bool restartAudioPreservingState();
    bool loadDrumSample(int slot, const uint8_t* data, size_t size);
    void noteOnPart(int part, int note, int velocity);
    void noteOffPart(int part, int note, int velocity);
    void polyPressurePart(int part, int note, int pressure);
    void channelPressurePart(int part, int pressure);
    void controlChangePart(int part, int cc, int value);
    void pitchBendPart(int part, int value14);

    // 8-track / 8-bar MIDI sequencer trial. Playback is driven from the
    // audio callback; UI only edits/queries musical event data.
    static constexpr int SEQ_TRACK_COUNT = 8;
    static constexpr int SEQ_BARS = 8;
    static constexpr int SEQ_PPQN = 96;
    static constexpr int SEQ_TICKS_PER_BAR = SEQ_PPQN * 4;
    static constexpr int SEQ_LOOP_TICKS = SEQ_TICKS_PER_BAR * SEQ_BARS;
    void sequencerPlay();
    void sequencerStop();
    void sequencerToggleRecord();
    void sequencerSetClick(bool enabled);
    void sequencerSetBpm(int bpm);
    void sequencerSetSelectedTrack(int track);
    void sequencerSetTrackPart(int track, int part);
    void sequencerClearTrack(int track);
    void sequencerToggleGridNote(int track, int step, int note, int velocity);
    void sequencerSetGridNote(int track, int step, int note, int velocityOrZero);
    void sequencerSetNoteDuration(int track, int noteIndex, int durationTick);
    void sequencerDeleteNote(int track, int noteIndex);
    void sequencerSetLoopBars(int bars);
    int sequencerLoopBars() const { return seqLoopBars_.load(std::memory_order_relaxed); }
    void sequencerSetTrackSend(int track, int reverb, int delay);
    bool sequencerPlaying() const { return seqPlaying_.load(std::memory_order_relaxed); }
    bool sequencerRecording() const { return seqRecording_.load(std::memory_order_relaxed); }
    bool sequencerClick() const { return seqClick_.load(std::memory_order_relaxed); }
    int sequencerBpm() const { return seqBpm_.load(std::memory_order_relaxed); }
    int sequencerSelectedTrack() const { return seqSelectedTrack_.load(std::memory_order_relaxed); }
    int sequencerPlayheadTick() const { return seqCurrentTick_.load(std::memory_order_relaxed); }
    int sequencerTrackPart(int track) const;
    std::vector<int> sequencerNotes(int track) const;

    void recorderToggleRecording();
    void recorderToggleRandom();
    void recorderClear();
    void recorderPlaySlot(int slot);
    void recorderRecordSlot(int slot);
    void recorderToggleClock();
    void recorderSetBpm(int bpm);
    void recorderMidiRealtime(int status);
    bool recorderRecording() const { return recorder_.isRecording(); }
    bool recorderRandom() const { return recorder_.isRandom(); }
    bool recorderMidiClock() const { return recorder_.isMidiClockMode(); }
    int recorderBpm() const { return recorder_.internalBpm(); }
    int recorderRecordingSlot() const { return recorder_.recordingSlot(); }
    bool recorderSlotPlaying(int slot) const { return recorder_.isSlotPlaying(slot); }
    float recorderSlotProgress(int slot) const { return recorder_.slotProgress(slot); }
    int recorderValidSamples(int slot) const { return recorder_.validSamples(slot); }
    float recorderPeak(int slot, int bin) const { return recorder_.peak(slot, bin); }
    float recorderInputLevel() const { return recorder_.inputLevel(); }

private:
    AudioEngine();
    ~AudioEngine();
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    struct Event {
        enum Type : uint8_t {
            NOTE_ON, NOTE_OFF, POLY_AT, CH_AT, CC, PITCH,
            DREAMY, BOOST, BOOST_DB, SPACE_MODE, SPACE_PARAMS,
            TAPE, TAPE_PARAMS, DREAMY_PARAMS, DREAMY_MODE, DREAMY_EXTRA, ADSR, INSTRUMENT,
            PART_NOTE_ON, PART_NOTE_OFF, PART_POLY_AT, PART_CH_AT, PART_CC, PART_PITCH,
            DRUM_PARAM, DRUM_FX, PART_FX, PART_MIXER, DRUM_SAMPLE_MIXER,
            FELT_REVERB, PERFORMANCE_XY,
            SEQ_PLAY, SEQ_STOP, SEQ_RECORD, SEQ_CLICK, SEQ_BPM,
            SEQ_SELECT_TRACK, SEQ_ASSIGN_PART, SEQ_CLEAR_TRACK, SEQ_TOGGLE_GRID,
            SEQ_SET_GRID, SEQ_SET_DURATION, SEQ_DELETE_NOTE, SEQ_LOOP_BARS, SEQ_TRACK_SEND
        } type;
        int a=0,b=0,c=0,d=0;
    };

    static constexpr uint32_t QUEUE = 1024;
    std::array<Event, QUEUE> queue_{};
    std::atomic<uint32_t> write_{0}, read_{0};
    void push(Event e);
    bool pop(Event& e);

    PhysicalViolin violin_;
    std::array<InstrumentModels, 7> modelParts_{};
    int selectedInstrument_=0;
    static constexpr int PART_COUNT = 16;
    std::array<int, PART_COUNT> partVolume_{{112,112,112,112,112,112,112,112,127,127,127,127,127,127,127,127}};
    std::array<int, PART_COUNT> partPan_{{64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64}};
    std::array<bool, PART_COUNT> partMute_{{false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false}};
    std::array<int, PART_COUNT> partSustain_{{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
    std::array<int, PART_COUNT> partPitch_{{8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192}};
    // 0..100000 linear peak, using integer atomics to keep callback exchange trivial.
    std::array<std::atomic<int>, PART_COUNT> partMeterQ_{};
    DreamyEffect dreamy_;
    SpaceEffect space_;
    SpaceEffect feltPianoReverb_;
    TapeEffect tape_;
    IntegratedRecorder recorder_;

    struct DrumSample {
        std::vector<float> left;
        std::vector<float> right;
        int sampleRate = 48000;
        bool loaded = false;
    };
    struct DrumSampleVoice {
        bool active = false;
        int slot = -1;
        double position = 0.0;
        float gain = 1.0f;
    };
    static constexpr int DRUM_SAMPLE_COUNT = 6;
    static constexpr int DRUM_SAMPLE_VOICES = 12;
    std::array<DrumSample, DRUM_SAMPLE_COUNT> drumSamples_{};
    std::array<DrumSampleVoice, DRUM_SAMPLE_VOICES> drumSampleVoices_{};
    std::array<int,DRUM_SAMPLE_COUNT> drumSampleVolume_{{127,127,127,127,127,127}};
    std::array<int,DRUM_SAMPLE_COUNT> drumSamplePan_{{64,64,64,64,64,64}};
    std::array<bool,DRUM_SAMPLE_COUNT> drumSampleMute_{{false,false,false,false,false,false}};
    int drumSampleSteal_ = 0;

    int cc1_=14;
    int cc7_=112;
    int cc10_=74;
    int cc11_=74;
    int cc74_=42;
    int cc64_=0;
    int cc103_=36;
    int cc104_=36;
    int cc106_=64;
    int cc107_=64;
    int dreamyMode_=0;
    int pitch_=8192;
    int boosterStep_=0;
    int boostDb_=0;
    float spaceMix_=0.50f, spaceDecay_=0.50f;
    float tapeWow_=0.50f, tapeFlutter_=0.50f, tapeDrive_=0.50f;
    float dreamyMix_=0.34f;
    std::array<int,PART_COUNT> partBoostDb_{{0,0,0,0,0,0,0,6,0,0,0,0,0,0,0,0}};
    std::array<int,PART_COUNT> partDistortion_{{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
    int feltReverbMix_=28;
    int feltReverbDecay_=58;

    // Momentary touchscreen performance FX.
    bool performanceXYActive_=false;
    int performanceXYPart_=0;
    float performanceXYX_=0.5f;
    float performanceXYY_=0.5f;
    float performanceXYSmoothX_=0.5f;
    float performanceXYSmoothY_=0.5f;
    float performanceXYGate_=0.0f;
    float performanceDelaySamples_=0.0f;
    float stutterLoopSamples_=0.0f;

    std::vector<float> performanceDelayL_;
    std::vector<float> performanceDelayR_;
    int performanceDelayWrite_=0;

    std::vector<float> stutterHistoryL_;
    std::vector<float> stutterHistoryR_;
    int stutterWrite_=0;
    int stutterCaptureEnd_=0;
    double stutterPhase_=0.0;

    // Trial MIDI sequencer. Fixed storage avoids allocation in the audio callback.
    struct SeqNote {
        std::atomic<int> startTick{0};
        std::atomic<int> durationTick{24};
        std::atomic<int> note{60};
        std::atomic<int> velocity{100};
    };
    static constexpr int SEQ_MAX_NOTES_PER_TRACK = 512;
    std::array<std::array<SeqNote,SEQ_MAX_NOTES_PER_TRACK>,SEQ_TRACK_COUNT> seqNotes_{};
    std::array<std::atomic<int>,SEQ_TRACK_COUNT> seqNoteCount_{};
    std::array<std::atomic<int>,SEQ_TRACK_COUNT> seqTrackPart_{};
    std::atomic<bool> seqPlaying_{false};
    std::atomic<bool> seqRecording_{false};
    std::atomic<bool> seqClick_{false};
    std::atomic<int> seqBpm_{120};
    std::atomic<int> seqSelectedTrack_{0};
    std::atomic<int> seqCurrentTick_{0};
    std::atomic<int> seqLoopBars_{2};
    std::array<int,SEQ_TRACK_COUNT> seqSendReverb_{{0,0,0,0,0,0,0,0}};
    std::array<int,SEQ_TRACK_COUNT> seqSendDelay_{{0,0,0,0,0,0,0,0}};
    SpaceEffect seqSendReverbFx_;
    std::vector<float> seqSendDelayL_;
    std::vector<float> seqSendDelayR_;
    int seqSendDelayWrite_=0;
    double seqTickPhase_=0.0;
    int seqLastProcessedTick_=-1;
    std::array<int,128> seqRecordStartTick_{};
    std::array<int,128> seqRecordVelocity_{};
    double seqClickPhase_=0.0;
    float seqClickEnvelope_=0.0f;
    float seqClickFrequency_=1200.0f;

    void seqProcessTick(int tick);
    void seqRenderClick(float& left, float& right);
    void seqRecordNoteOn(int part, int note, int velocity);
    void seqRecordNoteOff(int part, int note);
    void seqFinishPendingRecordedNotes();
    void seqAddRecordedNote(int track, int startTick, int durationTick, int note, int velocity);
    void seqToggleGridNoteInternal(int track, int step, int note, int velocity);
    void seqSetGridNoteInternal(int track, int step, int note, int velocityOrZero);
    void seqSetNoteDurationInternal(int track, int noteIndex, int durationTick);
    void seqDeleteNoteInternal(int track, int noteIndex);
    void seqClearTrackInternal(int track);
    void seqStopSoundingNotes();

    // Eight SAMPLE banks share one 64-voice pool. Each voice snapshots the
    // active double-buffer bank index at NoteOn so no mmap changes underneath it.
    struct EpVoice {
        bool active=false;
        bool releasing=false;
        bool pendingRelease=false;
        bool keyDown=false;
        int bankSlot=0;
        int bankIndex=0;
        int note=0;
        int velocity=0;
        int rr=1;
        double frame=0.0;
        double releaseFrame=0.0;
        float bodyVelocity=0.f;
        float targetBodyVelocity=0.f;
        int ageFrames=0;
        std::array<const SampleBank::Entry*,8> sus{};
        std::array<const SampleBank::Entry*,8> rel{};
    };
    static constexpr std::array<int,8> EP_VELS{{16,32,48,64,80,96,112,127}};
    static constexpr int EP_MAX_VOICES = 64;
    std::array<EpVoice,EP_MAX_VOICES> epVoices_{};
    std::array<std::array<uint8_t,128>,SAMPLE_BANK_COUNT> epRrCounter_{};
    std::array<std::array<int,128>,SAMPLE_BANK_COUNT> epPolyAT_{};
    std::array<int,SAMPLE_BANK_COUNT> epChannelAT_{{0,0,0,0,0,0,0,0}};
    std::array<int,SAMPLE_BANK_COUNT> epExpression_{{127,127,127,127,127,127,127,127}};

    // Two mmap objects per slot: background load writes only the inactive bank.
    std::array<std::array<SampleBank,2>,SAMPLE_BANK_COUNT> epBanks_{};
    std::array<std::atomic<int>,SAMPLE_BANK_COUNT> epActiveBank_{};
    std::array<std::atomic<int>,SAMPLE_BANK_COUNT> epPendingBank_{};
    mutable std::array<std::mutex,SAMPLE_BANK_COUNT> epBankLoadMutex_{};

    AAudioStream* stream_=nullptr;
    int preferredOutputDeviceId_=AAUDIO_UNSPECIFIED;
    int sampleRate_=48000;
    int defaultBufferSizeFrames_=0;
    float requestedBufferBursts_=0.0f;

    aaudio_result_t openOutputStream(
            int deviceId, int requestedRate, AAudioStream** outStream);

    const SampleBank& epBankForVoice(const EpVoice& v) const;
    const SampleBank& epActiveBank(int slot) const;
    void epBeginVoice(int slot, int note, int velocity);
    void epReleaseVoice(int slot, int note);
    void epPolyPressure(int slot, int note, int pressure);
    void epChannelPressure(int slot, int pressure);
    void epSustainChanged(int slot, bool down);
    void epAllNotesOff(int slot);
    void epAllNotesOff();
    int epActiveVoices(int slot) const;
    void epRenderVoice(EpVoice& v, float& l, float& r);
    float epLayerSample(const EpVoice& v, bool release, int layer, double frame, int channel) const;
    static void epBracket(float velocity, int& lo, int& hi, float& mix);

    void handle(const Event& e);
    void handlePartNoteOn(int part, int note, int velocity);
    void handlePartNoteOff(int part, int note);
    void handlePartPolyPressure(int part, int note, int pressure);
    void handlePartChannelPressure(int part, int pressure);
    void handlePartControlChange(int part, int cc, int value);
    void handlePartPitchBend(int part, int value14);
    void allNotesOffPart(int part);
    void triggerDrumSample(int slot, int velocity);
    void stopDrumSamples();
    void processDrumSamples(float& left, float& right);
    int activeDrumSampleVoices() const;
    void resetPerformanceDelay();
    void processPerformanceDelay(float& left, float& right);
    void recordStutterHistory(float left, float right);
    void processPerformanceStutter(float& left, float& right);
    float processPart(int part);
    int activeVoicesPart(int part) const;
    void render(float* out, int32_t frames);
    static aaudio_data_callback_result_t dataCallback(AAudioStream*, void*, void*, int32_t);
    static void errorCallback(AAudioStream*, void*, aaudio_result_t);
};
