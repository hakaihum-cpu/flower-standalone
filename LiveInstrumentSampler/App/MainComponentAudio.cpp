#include "MainComponent.h"

#include <cmath>
#include <filesystem>
#include <fstream>

namespace {
void setLabel(juce::Label& l, const juce::String& text) { l.setText(text, juce::dontSendNotification); }
juce::String passText(const juce::String& detail = {}) { return detail.isEmpty() ? "PASS" : "PASS - " + detail; }
juce::String failText(const juce::String& detail) { return "FAIL - " + detail; }
}

void MainComponent::refreshDriverList() {
    driverBox_.clear();
    const auto types = audio_.getDriverTypes();
    int asioId = 0;
    for (int i = 0; i < types.size(); ++i) {
        driverBox_.addItem(types[i], i + 1);
        if (types[i].equalsIgnoreCase("ASIO")) asioId = i + 1;
    }
    driverBox_.setSelectedId(asioId > 0 ? asioId : (types.isEmpty() ? 0 : 1));
    refreshDeviceLists();
}

void MainComponent::refreshDeviceLists() {
    const auto type = driverBox_.getText();
    const auto inputs = audio_.getDeviceNamesForType(type, true);
    const auto outputs = audio_.getDeviceNamesForType(type, false);
    inputDeviceBox_.clear(); outputDeviceBox_.clear();
    for (int i = 0; i < inputs.size(); ++i) inputDeviceBox_.addItem(inputs[i], i + 1);
    for (int i = 0; i < outputs.size(); ++i) outputDeviceBox_.addItem(outputs[i], i + 1);
    if (!inputs.isEmpty()) inputDeviceBox_.setSelectedId(1);
    if (!outputs.isEmpty()) outputDeviceBox_.setSelectedId(1);
    refreshChannelAndBufferLists();
}

void MainComponent::refreshChannelAndBufferLists() {
    inputPairBox_.clear(); outputPairBox_.clear(); bufferBox_.clear();
    inputChannelChoices_.clear(); outputChannelChoices_.clear();
    const auto caps = audio_.getDeviceCapabilities(driverBox_.getText(), inputDeviceBox_.getText(), outputDeviceBox_.getText());

    for (int i = 0; i < caps.inputChannels.size(); ++i) {
        inputChannelChoices_.push_back({i, 1});
        inputPairBox_.addItem("MONO " + juce::String(i + 1) + ": " + caps.inputChannels[i], static_cast<int>(inputChannelChoices_.size()));
    }
    for (int i = 0; i + 1 < caps.inputChannels.size(); i += 2) {
        inputChannelChoices_.push_back({i, 2});
        inputPairBox_.addItem("STEREO " + juce::String(i + 1) + "+" + juce::String(i + 2) + ": "
                              + caps.inputChannels[i] + " / " + caps.inputChannels[i + 1],
                              static_cast<int>(inputChannelChoices_.size()));
    }

    for (int i = 0; i < caps.outputChannels.size(); ++i) {
        outputChannelChoices_.push_back({i, 1});
        outputPairBox_.addItem("MONO " + juce::String(i + 1) + ": " + caps.outputChannels[i], static_cast<int>(outputChannelChoices_.size()));
    }
    for (int i = 0; i + 1 < caps.outputChannels.size(); i += 2) {
        outputChannelChoices_.push_back({i, 2});
        outputPairBox_.addItem("STEREO " + juce::String(i + 1) + "+" + juce::String(i + 2) + ": "
                               + caps.outputChannels[i] + " / " + caps.outputChannels[i + 1],
                               static_cast<int>(outputChannelChoices_.size()));
    }

    if (!inputChannelChoices_.empty()) {
        const int stereoId = caps.inputChannels.size() >= 2 ? caps.inputChannels.size() + 1 : 1;
        inputPairBox_.setSelectedId(stereoId);
    }
    if (!outputChannelChoices_.empty()) {
        const int stereoId = caps.outputChannels.size() >= 2 ? caps.outputChannels.size() + 1 : 1;
        outputPairBox_.setSelectedId(stereoId);
    }

    juce::Array<int> buffers = caps.bufferSizes;
    if (buffers.isEmpty()) buffers.addArray({64, 128, 256, 512, 1024});
    int defaultId = 0;
    for (int i = 0; i < buffers.size(); ++i) {
        bufferBox_.addItem(juce::String(buffers[i]), i + 1);
        if (buffers[i] == 128) defaultId = i + 1;
    }
    bufferBox_.setSelectedId(defaultId > 0 ? defaultId : (buffers.isEmpty() ? 0 : 1));

    bool supports48 = false;
    for (double r : caps.sampleRates) if (std::abs(r - 48000.0) < 0.5) supports48 = true;
    setLabel(audioStatusLabel_, supports48 || caps.sampleRates.isEmpty() ? "EP-SAMPLE capture rate: 48000 Hz" : "WARNING: selected device does not report 48 kHz support");
}

int MainComponent::selectedFirstInputChannel() const {
    const int i = inputPairBox_.getSelectedItemIndex();
    return i >= 0 && i < static_cast<int>(inputChannelChoices_.size()) ? inputChannelChoices_[static_cast<std::size_t>(i)].first : 0;
}
int MainComponent::selectedFirstOutputChannel() const {
    const int i = outputPairBox_.getSelectedItemIndex();
    return i >= 0 && i < static_cast<int>(outputChannelChoices_.size()) ? outputChannelChoices_[static_cast<std::size_t>(i)].first : 0;
}
int MainComponent::selectedInputChannelCount() {
    const int i = inputPairBox_.getSelectedItemIndex();
    return i >= 0 && i < static_cast<int>(inputChannelChoices_.size()) ? inputChannelChoices_[static_cast<std::size_t>(i)].count : 1;
}
int MainComponent::selectedOutputChannelCount() {
    const int i = outputPairBox_.getSelectedItemIndex();
    return i >= 0 && i < static_cast<int>(outputChannelChoices_.size()) ? outputChannelChoices_[static_cast<std::size_t>(i)].count : 1;
}
int MainComponent::selectedBufferSize() const { return bufferBox_.getText().getIntValue() > 0 ? bufferBox_.getText().getIntValue() : 128; }

void MainComponent::applyAudioSettings() {
    const auto err = audio_.applyDeviceSetup(driverBox_.getText(), inputDeviceBox_.getText(), outputDeviceBox_.getText(),
                                              selectedFirstInputChannel(), selectedInputChannelCount(),
                                              selectedFirstOutputChannel(), selectedOutputChannelCount(), selectedBufferSize());
    setLabel(audioStatusLabel_, err.isEmpty() ? passText(audio_.currentDeviceSummary()) : failText(err));
}

void MainComponent::runAsioCheck() {
    setLabel(diagEnumerateLabel_, "ASIO driver: checking...");
    setLabel(diagOpenLabel_, "48 kHz open: -"); setLabel(diagInputLabel_, "input signal: -");
    setLabel(diagOutputLabel_, "output tone: -"); setLabel(diagReopenLabel_, "reopen: -");
    toneHeardButton_.setEnabled(false); toneNotHeardButton_.setEnabled(false); toneAgainButton_.setEnabled(false);

    const auto types = audio_.getDriverTypes();
    int asioIndex = -1;
    for (int i = 0; i < types.size(); ++i) if (types[i].equalsIgnoreCase("ASIO")) asioIndex = i;
    if (asioIndex < 0) {
        setLabel(diagEnumerateLabel_, failText("JUCE/Windows ASIO type is unavailable"));
        diagPhase_ = DiagnosticPhase::idle;
        return;
    }
    setLabel(diagEnumerateLabel_, passText("ASIO type found"));
    if (!driverBox_.getText().equalsIgnoreCase("ASIO")) {
        for (int i = 0; i < driverBox_.getNumItems(); ++i)
            if (driverBox_.getItemText(i).equalsIgnoreCase("ASIO")) driverBox_.setSelectedItemIndex(i);
    }
    if (inputDeviceBox_.getText().isEmpty() || outputDeviceBox_.getText().isEmpty()) {
        setLabel(diagOpenLabel_, failText("Select ASIO input/output device"));
        return;
    }

    const auto err = audio_.applyDeviceSetup("ASIO", inputDeviceBox_.getText(), outputDeviceBox_.getText(),
                                              selectedFirstInputChannel(), selectedInputChannelCount(),
                                              selectedFirstOutputChannel(), selectedOutputChannelCount(), selectedBufferSize());
    if (err.isNotEmpty()) {
        setLabel(diagOpenLabel_, failText(err));
        return;
    }
    setLabel(diagOpenLabel_, passText(audio_.currentDeviceSummary()));
    setLabel(diagInputLabel_, "INPUT TEST - play the instrument for 3 seconds");
    diagMaxInput_ = 0.0f;
    diagStartedMs_ = juce::Time::getMillisecondCounterHiRes();
    diagPhase_ = DiagnosticPhase::inputListen;
}

void MainComponent::markToneHeard(bool heard) {
    if (diagPhase_ != DiagnosticPhase::waitingForToneConfirm) return;
    setLabel(diagOutputLabel_, heard ? passText("user confirmed audible") : failText("user could not hear test tone"));
    toneHeardButton_.setEnabled(false); toneNotHeardButton_.setEnabled(false); toneAgainButton_.setEnabled(false);
    if (!heard) {
        diagPhase_ = DiagnosticPhase::complete;
        return;
    }
    const auto err = audio_.reopenCurrentDevice();
    setLabel(diagReopenLabel_, err.isEmpty() ? passText(audio_.currentDeviceSummary()) : failText(err));
    diagPhase_ = DiagnosticPhase::complete;
}

void MainComponent::timerCallback() {
    if (diagPhase_ == DiagnosticPhase::inputListen) {
        diagMaxInput_ = juce::jmax(diagMaxInput_, audio_.currentInputLevel());
        const auto elapsed = juce::Time::getMillisecondCounterHiRes() - diagStartedMs_;
        if (elapsed >= 3000.0) {
            if (diagMaxInput_ > 0.002f) {
                setLabel(diagInputLabel_, passText("signal detected, peak " + juce::String(diagMaxInput_, 4)));
                setLabel(diagOutputLabel_, "OUTPUT TEST - confirm that you can hear the test tone");
                audio_.playOutputTestTone();
                toneHeardButton_.setEnabled(true); toneNotHeardButton_.setEnabled(true); toneAgainButton_.setEnabled(true);
                diagPhase_ = DiagnosticPhase::waitingForToneConfirm;
            } else {
                setLabel(diagInputLabel_, failText("no input signal detected; check INPUT CH and instrument level"));
                diagPhase_ = DiagnosticPhase::complete;
            }
        }
    }
}
