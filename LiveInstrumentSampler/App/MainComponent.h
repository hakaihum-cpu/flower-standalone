#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <thread>
#include <vector>

#include "AudioEngine.h"
#include "WaveformTrimComponent.h"
#include "../Core/SamplerCore.h"

class MainComponent final : public juce::Component,
                            private juce::Timer {
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    enum class DiagnosticPhase { idle, inputListen, waitingForToneConfirm, complete };

    void configureUi();
    void populateStartNotes();
    void refreshDriverList();
    void refreshDeviceLists();
    void refreshChannelAndBufferLists();
    void applyAudioSettings();
    void runAsioCheck();
    void markToneHeard(bool heard);
    void timerCallback() override;

    void startSession();
    void beginRecording();
    void stopRecording();
    void confirmTake();
    void retake();
    void updateCurrentKeyUi();
    void saveSessionManifest();

    void finishDirectBin();
    void exportMaster();
    void importTunedAndBuild();
    void openSessionFolder();

    void syncTrimEditorsFromWaveform(double start, double end);
    void syncWaveformFromTrimEditors();
    void runBackground(const juce::String& taskName, std::function<void()> job, std::function<void()> onSuccess = {});
    void setBusy(bool busy, const juce::String& message = {});

    int selectedFirstInputChannel() const;
    int selectedFirstOutputChannel() const;
    int selectedInputChannelCount();
    int selectedOutputChannelCount();
    int selectedBufferSize() const;
    juce::String safeBankName() const;

    AudioEngine audio_;

    juce::Label titleLabel_;
    juce::Label stepLabel_;

    juce::GroupComponent audioGroup_{"audio", "1. AUDIO / ASIO"};
    juce::ComboBox driverBox_, inputDeviceBox_, outputDeviceBox_, inputPairBox_, outputPairBox_, bufferBox_;
    juce::Label driverLabel_, inputDeviceLabel_, outputDeviceLabel_, inputPairLabel_, outputPairLabel_, bufferLabel_;
    juce::TextButton applyAudioButton_{"APPLY AUDIO"};
    juce::Label audioStatusLabel_;

    juce::TextButton asioCheckButton_{"RUN ASIO CHECK"};
    juce::TextButton toneHeardButton_{"I HEARD THE TONE"};
    juce::TextButton toneNotHeardButton_{"NO TONE"};
    juce::TextButton toneAgainButton_{"PLAY TONE AGAIN"};
    juce::Label diagEnumerateLabel_, diagOpenLabel_, diagInputLabel_, diagOutputLabel_, diagReopenLabel_;
    DiagnosticPhase diagPhase_ = DiagnosticPhase::idle;
    double diagStartedMs_ = 0.0;
    float diagMaxInput_ = 0.0f;

    juce::GroupComponent sessionGroup_{"session", "2. SESSION"};
    juce::ComboBox startNoteBox_;
    juce::TextEditor keyCountEditor_;
    juce::TextEditor bankNameEditor_;
    juce::Label startNoteLabel_, keyCountLabel_, bankNameLabel_;
    juce::TextButton startSessionButton_{"START"};
    juce::Label sessionPathLabel_;

    juce::GroupComponent recordGroup_{"record", "3. RECORD EACH KEY"};
    juce::Label currentKeyLabel_;
    juce::TextButton recButton_{"REC"};
    juce::TextButton stopButton_{"STOP"};
    juce::TextButton setButton_{"SET"};
    juce::TextButton retakeButton_{"RETAKE"};
    WaveformTrimComponent waveform_;
    juce::TextEditor trimStartEditor_, trimEndEditor_;
    juce::Label trimStartLabel_, trimEndLabel_;
    juce::Label recordHintLabel_;
    bool syncingTrimEditors_ = false;

    juce::GroupComponent finishGroup_{"finish", "4. FINISH / TUNING"};
    juce::TextButton finishButton_{"FINISH → DIRECT BIN"};
    juce::TextButton exportMasterButton_{"EXPORT MASTER WAV"};
    juce::TextButton importTunedButton_{"IMPORT TUNED WAV → BIN"};
    juce::TextButton openFolderButton_{"OPEN SESSION FOLDER"};
    juce::Label finishStatusLabel_;

    lis::SessionSpec spec_;
    std::vector<lis::KeyTake> takes_;
    int currentIndex_ = -1;
    juce::File sessionDir_;
    juce::File currentRawFile_;
    juce::File masterWavFile_;
    juce::File epMapFile_;
    juce::FileChooser fileChooser_{"Select tuned master WAV"};

    std::atomic<bool> busy_{false};
    std::jthread worker_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
