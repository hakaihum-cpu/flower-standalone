#include "MainComponent.h"

#include <cmath>
#include <filesystem>
#include <fstream>

namespace {
void setLabel(juce::Label& l, const juce::String& text) { l.setText(text, juce::dontSendNotification); }
juce::String passText(const juce::String& detail = {}) { return detail.isEmpty() ? "PASS" : "PASS — " + detail; }
juce::String failText(const juce::String& detail) { return "FAIL — " + detail; }
}

MainComponent::MainComponent() {
    configureUi();
    populateStartNotes();
    refreshDriverList();
    startTimerHz(20);
}

MainComponent::~MainComponent() {
    audio_.stopRecording();
    if (worker_.joinable()) {
        worker_.request_stop();
        worker_.join();
    }
}

void MainComponent::configureUi() {
    setSize(980, 900);
    setLabel(titleLabel_, "LIVE INSTRUMENT → EP-SAMPLE");
    titleLabel_.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    setLabel(stepLabel_, "ASIOを設定 → 鍵盤数を決める → 1鍵ずつREC/SET → FINISH");
    stepLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

    auto add = [this](juce::Component& c) { addAndMakeVisible(c); };
    add(titleLabel_); add(stepLabel_);
    add(audioGroup_); add(sessionGroup_); add(recordGroup_); add(finishGroup_);

    juce::Component* audioComponents[] = {
        &driverBox_, &inputDeviceBox_, &outputDeviceBox_, &inputPairBox_, &outputPairBox_, &bufferBox_,
        &driverLabel_, &inputDeviceLabel_, &outputDeviceLabel_, &inputPairLabel_, &outputPairLabel_, &bufferLabel_,
        &applyAudioButton_, &audioStatusLabel_, &asioCheckButton_, &toneHeardButton_, &toneNotHeardButton_, &toneAgainButton_,
        &diagEnumerateLabel_, &diagOpenLabel_, &diagInputLabel_, &diagOutputLabel_, &diagReopenLabel_
    };
    for (auto* component : audioComponents) add(*component);

    setLabel(driverLabel_, "DRIVER"); setLabel(inputDeviceLabel_, "INPUT DEVICE"); setLabel(outputDeviceLabel_, "OUTPUT DEVICE");
    setLabel(inputPairLabel_, "INPUT CH"); setLabel(outputPairLabel_, "OUTPUT CH"); setLabel(bufferLabel_, "BUFFER");
    audioStatusLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    juce::Label* diagnosticLabels[] = { &diagEnumerateLabel_, &diagOpenLabel_, &diagInputLabel_, &diagOutputLabel_, &diagReopenLabel_ };
    for (auto* label : diagnosticLabels)
        label->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    setLabel(diagEnumerateLabel_, "ASIO driver: —"); setLabel(diagOpenLabel_, "48 kHz open: —");
    setLabel(diagInputLabel_, "input signal: —"); setLabel(diagOutputLabel_, "output tone: —"); setLabel(diagReopenLabel_, "reopen: —");
    toneHeardButton_.setEnabled(false); toneNotHeardButton_.setEnabled(false); toneAgainButton_.setEnabled(false);

    driverBox_.onChange = [this] { refreshDeviceLists(); };
    inputDeviceBox_.onChange = [this] { refreshChannelAndBufferLists(); };
    outputDeviceBox_.onChange = [this] { refreshChannelAndBufferLists(); };
    applyAudioButton_.onClick = [this] { applyAudioSettings(); };
    asioCheckButton_.onClick = [this] { runAsioCheck(); };
    toneHeardButton_.onClick = [this] { markToneHeard(true); };
    toneNotHeardButton_.onClick = [this] { markToneHeard(false); };
    toneAgainButton_.onClick = [this] { audio_.playOutputTestTone(); };

    juce::Component* sessionComponents[] = {
        &startNoteBox_, &keyCountEditor_, &bankNameEditor_, &startNoteLabel_,
        &keyCountLabel_, &bankNameLabel_, &startSessionButton_, &sessionPathLabel_
    };
    for (auto* component : sessionComponents) add(*component);
    setLabel(startNoteLabel_, "START NOTE"); setLabel(keyCountLabel_, "KEYS"); setLabel(bankNameLabel_, "BANK NAME");
    keyCountEditor_.setInputRestrictions(3, "0123456789"); keyCountEditor_.setText("61");
    bankNameEditor_.setText("Instrument");
    sessionPathLabel_.setColour(juce::Label::textColourId, juce::Colours::grey);
    startSessionButton_.onClick = [this] { startSession(); };

    juce::Component* recordComponents[] = {
        &currentKeyLabel_, &recButton_, &stopButton_, &setButton_, &retakeButton_, &waveform_,
        &trimStartEditor_, &trimEndEditor_, &trimStartLabel_, &trimEndLabel_, &recordHintLabel_
    };
    for (auto* component : recordComponents) add(*component);
    currentKeyLabel_.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    setLabel(currentKeyLabel_, "STARTを押すと録音を始められます");
    setLabel(trimStartLabel_, "START sec"); setLabel(trimEndLabel_, "END sec");
    trimStartEditor_.setInputRestrictions(10, "0123456789."); trimEndEditor_.setInputRestrictions(10, "0123456789.");
    setLabel(recordHintLabel_, "REC → STOP → 波形の範囲を指定 → SET。やり直しは RETAKE。");
    recordHintLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    recButton_.onClick = [this] { beginRecording(); };
    stopButton_.onClick = [this] { stopRecording(); };
    setButton_.onClick = [this] { confirmTake(); };
    retakeButton_.onClick = [this] { retake(); };
    stopButton_.setEnabled(false); setButton_.setEnabled(false); retakeButton_.setEnabled(false); recButton_.setEnabled(false);
    trimStartEditor_.setEnabled(false); trimEndEditor_.setEnabled(false);
    waveform_.onTrimChanged = [this](double s, double e) { syncTrimEditorsFromWaveform(s, e); };
    trimStartEditor_.onReturnKey = [this] { syncWaveformFromTrimEditors(); };
    trimEndEditor_.onReturnKey = [this] { syncWaveformFromTrimEditors(); };
    trimStartEditor_.onFocusLost = [this] { syncWaveformFromTrimEditors(); };
    trimEndEditor_.onFocusLost = [this] { syncWaveformFromTrimEditors(); };

    juce::Component* finishComponents[] = {
        &finishButton_, &exportMasterButton_, &importTunedButton_, &openFolderButton_, &finishStatusLabel_
    };
    for (auto* component : finishComponents) add(*component);
    finishButton_.setEnabled(false); exportMasterButton_.setEnabled(false); importTunedButton_.setEnabled(false); openFolderButton_.setEnabled(false);
    finishButton_.onClick = [this] { finishDirectBin(); };
    exportMasterButton_.onClick = [this] { exportMaster(); };
    importTunedButton_.onClick = [this] { importTunedAndBuild(); };
    openFolderButton_.onClick = [this] { openSessionFolder(); };
    finishStatusLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
}

void MainComponent::populateStartNotes() {
    startNoteBox_.clear();
    for (int n = 21; n <= 108; ++n)
        startNoteBox_.addItem(juce::String(lis::noteName(n)) + " (" + juce::String(n) + ")", n + 1);
    startNoteBox_.setSelectedId(36 + 1); // C2
}

void MainComponent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff0e0e0e));
}

void MainComponent::resized() {
    auto area = getLocalBounds().reduced(18);
    titleLabel_.setBounds(area.removeFromTop(34));
    stepLabel_.setBounds(area.removeFromTop(24));
    area.removeFromTop(8);

    auto audioArea = area.removeFromTop(235);
    audioGroup_.setBounds(audioArea);
    auto a = audioArea.reduced(14, 30);
    const int labelW = 92;
    const int rowH = 28;
    auto row1 = a.removeFromTop(rowH);
    driverLabel_.setBounds(row1.removeFromLeft(labelW)); driverBox_.setBounds(row1.removeFromLeft(145)); row1.removeFromLeft(8);
    inputDeviceLabel_.setBounds(row1.removeFromLeft(95)); inputDeviceBox_.setBounds(row1.removeFromLeft(190)); row1.removeFromLeft(8);
    outputDeviceLabel_.setBounds(row1.removeFromLeft(105)); outputDeviceBox_.setBounds(row1.removeFromLeft(190));
    a.removeFromTop(6);
    auto row2 = a.removeFromTop(rowH);
    inputPairLabel_.setBounds(row2.removeFromLeft(labelW)); inputPairBox_.setBounds(row2.removeFromLeft(250)); row2.removeFromLeft(10);
    outputPairLabel_.setBounds(row2.removeFromLeft(90)); outputPairBox_.setBounds(row2.removeFromLeft(250)); row2.removeFromLeft(10);
    bufferLabel_.setBounds(row2.removeFromLeft(58)); bufferBox_.setBounds(row2.removeFromLeft(80)); row2.removeFromLeft(8);
    applyAudioButton_.setBounds(row2.removeFromLeft(130));
    a.removeFromTop(6);
    audioStatusLabel_.setBounds(a.removeFromTop(24));
    a.removeFromTop(4);
    auto diagButtons = a.removeFromTop(30);
    asioCheckButton_.setBounds(diagButtons.removeFromLeft(145)); diagButtons.removeFromLeft(8);
    toneAgainButton_.setBounds(diagButtons.removeFromLeft(145)); diagButtons.removeFromLeft(8);
    toneHeardButton_.setBounds(diagButtons.removeFromLeft(170)); diagButtons.removeFromLeft(8);
    toneNotHeardButton_.setBounds(diagButtons.removeFromLeft(100));
    a.removeFromTop(4);
    auto diag1 = a.removeFromTop(23); diagEnumerateLabel_.setBounds(diag1.removeFromLeft(280)); diagOpenLabel_.setBounds(diag1.removeFromLeft(500));
    auto diag2 = a.removeFromTop(23); diagInputLabel_.setBounds(diag2.removeFromLeft(360)); diagOutputLabel_.setBounds(diag2.removeFromLeft(300)); diagReopenLabel_.setBounds(diag2);

    area.removeFromTop(8);
    auto sessionArea = area.removeFromTop(100);
    sessionGroup_.setBounds(sessionArea);
    auto s = sessionArea.reduced(14, 30);
    auto sr = s.removeFromTop(30);
    startNoteLabel_.setBounds(sr.removeFromLeft(85)); startNoteBox_.setBounds(sr.removeFromLeft(125)); sr.removeFromLeft(12);
    keyCountLabel_.setBounds(sr.removeFromLeft(48)); keyCountEditor_.setBounds(sr.removeFromLeft(58)); sr.removeFromLeft(12);
    bankNameLabel_.setBounds(sr.removeFromLeft(85)); bankNameEditor_.setBounds(sr.removeFromLeft(190)); sr.removeFromLeft(12);
    startSessionButton_.setBounds(sr.removeFromLeft(120));
    sessionPathLabel_.setBounds(s.removeFromTop(24));

    area.removeFromTop(8);
    auto recordArea = area.removeFromTop(360);
    recordGroup_.setBounds(recordArea);
    auto r = recordArea.reduced(14, 30);
    auto keyRow = r.removeFromTop(36);
    currentKeyLabel_.setBounds(keyRow.removeFromLeft(520));
    recButton_.setBounds(keyRow.removeFromLeft(90)); keyRow.removeFromLeft(6);
    stopButton_.setBounds(keyRow.removeFromLeft(90)); keyRow.removeFromLeft(6);
    setButton_.setBounds(keyRow.removeFromLeft(90)); keyRow.removeFromLeft(6);
    retakeButton_.setBounds(keyRow.removeFromLeft(100));
    r.removeFromTop(6);
    waveform_.setBounds(r.removeFromTop(220));
    r.removeFromTop(5);
    auto tr = r.removeFromTop(30);
    trimStartLabel_.setBounds(tr.removeFromLeft(75)); trimStartEditor_.setBounds(tr.removeFromLeft(100)); tr.removeFromLeft(15);
    trimEndLabel_.setBounds(tr.removeFromLeft(65)); trimEndEditor_.setBounds(tr.removeFromLeft(100));
    recordHintLabel_.setBounds(r.removeFromTop(30));

    area.removeFromTop(8);
    auto finishArea = area.removeFromTop(120);
    finishGroup_.setBounds(finishArea);
    auto f = finishArea.reduced(14, 30);
    auto fr = f.removeFromTop(34);
    finishButton_.setBounds(fr.removeFromLeft(190)); fr.removeFromLeft(8);
    exportMasterButton_.setBounds(fr.removeFromLeft(190)); fr.removeFromLeft(8);
    importTunedButton_.setBounds(fr.removeFromLeft(220)); fr.removeFromLeft(8);
    openFolderButton_.setBounds(fr.removeFromLeft(190));
    finishStatusLabel_.setBounds(f.removeFromTop(38));
}
