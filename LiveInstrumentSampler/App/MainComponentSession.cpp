#include "MainComponent.h"

#include <cmath>
#include <filesystem>
#include <fstream>

namespace {
void setLabel(juce::Label& l, const juce::String& text) { l.setText(text, juce::dontSendNotification); }
juce::String passText(const juce::String& detail = {}) { return detail.isEmpty() ? "PASS" : "PASS — " + detail; }
juce::String failText(const juce::String& detail) { return "FAIL — " + detail; }
}

juce::String MainComponent::safeBankName() const {
    auto s = bankNameEditor_.getText().trim();
    if (s.isEmpty()) s = "Instrument";
    juce::String out;
    for (auto c : s) if (juce::CharacterFunctions::isLetterOrDigit(c) || c == '-' || c == '_') out += c; else out += '_';
    return out;
}

void MainComponent::startSession() {
    if (!audio_.currentDeviceIsOpenAt48k()) {
        setLabel(audioStatusLabel_, failText("APPLY AUDIO first; recording requires an open 48 kHz device"));
        return;
    }
    const int start = startNoteBox_.getSelectedId() - 1;
    const int keys = keyCountEditor_.getText().getIntValue();
    if (start < 21 || keys < 1 || keys > 88 || start + keys - 1 > 108) {
        setLabel(sessionPathLabel_, failText("EP-SAMPLE range is A0-C8 (MIDI 21-108); KEYS must fit inside it"));
        return;
    }

    spec_ = {};
    spec_.startNote = start;
    spec_.keyCount = keys;
    spec_.sampleRate = 48000;
    spec_.silenceSeconds = 0.5;
    spec_.releaseTailSeconds = 0.5;

    const auto stamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
    sessionDir_ = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                    .getChildFile("EP Live Sampler")
                    .getChildFile(safeBankName() + "_" + stamp);
    sessionDir_.createDirectory();
    sessionDir_.getChildFile("RAW").createDirectory();
    sessionDir_.getChildFile("RAW").getChildFile("RETAKES").createDirectory();

    takes_.clear();
    for (int i = 0; i < keys; ++i) {
        lis::KeyTake t;
        t.midiNote = start + i;
        takes_.push_back(t);
    }
    currentIndex_ = 0;
    startNoteBox_.setEnabled(false);
    keyCountEditor_.setEnabled(false);
    bankNameEditor_.setEnabled(false);
    startSessionButton_.setEnabled(false);
    driverBox_.setEnabled(false);
    inputDeviceBox_.setEnabled(false);
    outputDeviceBox_.setEnabled(false);
    inputPairBox_.setEnabled(false);
    outputPairBox_.setEnabled(false);
    bufferBox_.setEnabled(false);
    applyAudioButton_.setEnabled(false);
    asioCheckButton_.setEnabled(false);
    toneAgainButton_.setEnabled(false);
    toneHeardButton_.setEnabled(false);
    toneNotHeardButton_.setEnabled(false);
    openFolderButton_.setEnabled(true);
    setLabel(sessionPathLabel_, sessionDir_.getFullPathName());
    updateCurrentKeyUi();
    saveSessionManifest();
}

void MainComponent::updateCurrentKeyUi() {
    waveform_.clear(); trimStartEditor_.clear(); trimEndEditor_.clear();
    trimStartEditor_.setEnabled(false); trimEndEditor_.setEnabled(false);
    setButton_.setEnabled(false); retakeButton_.setEnabled(false); stopButton_.setEnabled(false);

    if (currentIndex_ >= 0 && currentIndex_ < static_cast<int>(takes_.size())) {
        const auto midi = takes_[static_cast<std::size_t>(currentIndex_)].midiNote;
        setLabel(currentKeyLabel_, "KEY " + juce::String(currentIndex_ + 1) + " / " + juce::String(spec_.keyCount)
                                    + "     " + juce::String(lis::noteName(midi)) + "  (MIDI " + juce::String(midi) + ")");
        recButton_.setEnabled(!busy_.load());
        finishButton_.setEnabled(false);
    } else if (currentIndex_ == static_cast<int>(takes_.size())) {
        setLabel(currentKeyLabel_, "ALL KEYS SET — FINISHでBINを作成できます");
        recButton_.setEnabled(false);
        finishButton_.setEnabled(!busy_.load());
    }
}

void MainComponent::beginRecording() {
    if (currentIndex_ < 0 || currentIndex_ >= static_cast<int>(takes_.size())) return;
    const auto midi = takes_[static_cast<std::size_t>(currentIndex_)].midiNote;
    currentRawFile_ = sessionDir_.getChildFile("RAW").getChildFile(juce::String(midi).paddedLeft('0', 3) + "_" + juce::String(lis::noteName(midi)).replace("#", "s") + "_RAW.wav");
    const auto err = audio_.startRecording(currentRawFile_);
    if (err.isNotEmpty()) {
        setLabel(recordHintLabel_, failText(err));
        return;
    }
    recButton_.setEnabled(false); stopButton_.setEnabled(true); setButton_.setEnabled(false); retakeButton_.setEnabled(false);
    setLabel(recordHintLabel_, "RECORDING... 演奏が終わったら STOP");
}

void MainComponent::stopRecording() {
    audio_.stopRecording();
    stopButton_.setEnabled(false);
    if (!currentRawFile_.existsAsFile() || currentRawFile_.getSize() <= 44) {
        setLabel(recordHintLabel_, failText("RAW WAV was not recorded"));
        recButton_.setEnabled(true);
        return;
    }
    if (!waveform_.loadFile(currentRawFile_)) {
        setLabel(recordHintLabel_, failText("Cannot read recorded WAV"));
        recButton_.setEnabled(true);
        return;
    }
    auto& take = takes_[static_cast<std::size_t>(currentIndex_)];
    take.rawWav = std::filesystem::path(currentRawFile_.getFullPathName().toStdString());
    take.confirmed = false;
    syncTrimEditorsFromWaveform(0.0, waveform_.durationSeconds());
    trimStartEditor_.setEnabled(true); trimEndEditor_.setEnabled(true);
    setButton_.setEnabled(true); retakeButton_.setEnabled(true);
    setLabel(recordHintLabel_, "波形のSTART/ENDをドラッグ、または秒数を入力して SET");
}

void MainComponent::syncTrimEditorsFromWaveform(double start, double end) {
    if (syncingTrimEditors_) return;
    syncingTrimEditors_ = true;
    trimStartEditor_.setText(juce::String(start, 4), false);
    trimEndEditor_.setText(juce::String(end, 4), false);
    syncingTrimEditors_ = false;
}

void MainComponent::syncWaveformFromTrimEditors() {
    if (syncingTrimEditors_ || waveform_.durationSeconds() <= 0.0) return;
    waveform_.setTrimSeconds(trimStartEditor_.getText().getDoubleValue(), trimEndEditor_.getText().getDoubleValue());
}

void MainComponent::confirmTake() {
    if (currentIndex_ < 0 || currentIndex_ >= static_cast<int>(takes_.size()) || waveform_.durationSeconds() <= 0.0) return;
    syncWaveformFromTrimEditors();
    const auto start = waveform_.startSeconds();
    const auto end = waveform_.endSeconds();
    if (end - start < 0.02) {
        setLabel(recordHintLabel_, failText("Selected range is too short"));
        return;
    }
    auto& t = takes_[static_cast<std::size_t>(currentIndex_)];
    t.trimStartFrame = static_cast<std::uint64_t>(std::llround(start * 48000.0));
    t.trimEndFrame = static_cast<std::uint64_t>(std::llround(end * 48000.0));
    t.confirmed = true;
    saveSessionManifest();
    ++currentIndex_;
    setLabel(recordHintLabel_, "SETしました。次の鍵盤へ進みます。");
    updateCurrentKeyUi();
}

void MainComponent::retake() {
    audio_.stopRecording();
    if (currentRawFile_.existsAsFile()) {
        const auto retakesDir = sessionDir_.getChildFile("RAW").getChildFile("RETAKES");
        retakesDir.createDirectory();
        const auto stamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
        const auto archived = retakesDir.getNonexistentChildFile(
            currentRawFile_.getFileNameWithoutExtension() + "_RETAKE_" + stamp,
            currentRawFile_.getFileExtension(), false);
        if (!currentRawFile_.moveFileTo(archived)) {
            setLabel(recordHintLabel_, failText("Could not archive the rejected RAW take"));
            return;
        }
    }
    if (currentIndex_ >= 0 && currentIndex_ < static_cast<int>(takes_.size()))
        takes_[static_cast<std::size_t>(currentIndex_)] = lis::KeyTake{takes_[static_cast<std::size_t>(currentIndex_)].midiNote};
    saveSessionManifest();
    waveform_.clear();
    trimStartEditor_.clear(); trimEndEditor_.clear(); trimStartEditor_.setEnabled(false); trimEndEditor_.setEnabled(false);
    setButton_.setEnabled(false); retakeButton_.setEnabled(false); recButton_.setEnabled(true);
    setLabel(recordHintLabel_, "RETAKE ready — 元RAWはRETAKESへ保存しました。RECで録り直してください");
}

void MainComponent::saveSessionManifest() {
    if (!sessionDir_.isDirectory()) return;
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty("format", "LIVE_INSTRUMENT_SAMPLER_SESSION_V1");
    root->setProperty("startNote", spec_.startNote);
    root->setProperty("keyCount", spec_.keyCount);
    root->setProperty("sampleRate", spec_.sampleRate);
    root->setProperty("bankName", safeBankName());
    juce::Array<juce::var> arr;
    for (const auto& t : takes_) {
        juce::DynamicObject::Ptr o = new juce::DynamicObject();
        o->setProperty("midiNote", t.midiNote);
        o->setProperty("note", juce::String(lis::noteName(t.midiNote)));
        o->setProperty("rawWav", juce::String(t.rawWav.string()));
        o->setProperty("trimStartFrame", static_cast<juce::int64>(t.trimStartFrame));
        o->setProperty("trimEndFrame", static_cast<juce::int64>(t.trimEndFrame));
        o->setProperty("set", t.confirmed);
        arr.add(juce::var(o.get()));
    }
    root->setProperty("takes", juce::var(arr));
    sessionDir_.getChildFile("session.json").replaceWithText(juce::JSON::toString(juce::var(root.get()), true));
}
