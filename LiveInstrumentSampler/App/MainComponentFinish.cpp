#include "MainComponent.h"

#include <cmath>
#include <filesystem>
#include <fstream>

namespace {
void setLabel(juce::Label& l, const juce::String& text) { l.setText(text, juce::dontSendNotification); }
juce::String passText(const juce::String& detail = {}) { return detail.isEmpty() ? "PASS" : "PASS - " + detail; }
juce::String failText(const juce::String& detail) { return "FAIL - " + detail; }
}

void MainComponent::finishDirectBin() {
    if (currentIndex_ != static_cast<int>(takes_.size())) return;
    const auto out = sessionDir_.getChildFile(safeBankName() + "_direct_epbank.bin");
    const auto spec = spec_;
    const auto takes = takes_;
    runBackground("Building DIRECT BIN...", [spec, takes, out] {
        lis::buildEpBankFromTakes(spec, takes, std::filesystem::path(out.getFullPathName().toStdString()));
    }, [this, out] {
        setLabel(finishStatusLabel_, passText("BIN: " + out.getFullPathName()));
        exportMasterButton_.setEnabled(true);
    });
}

void MainComponent::exportMaster() {
    const auto name = safeBankName();
    masterWavFile_ = sessionDir_.getChildFile(name + "_master.wav");
    epMapFile_ = sessionDir_.getChildFile(name + "_master.epmap.json");
    const auto spec = spec_;
    const auto takes = takes_;
    const auto wav = masterWavFile_;
    const auto map = epMapFile_;
    runBackground("Building MASTER WAV...", [spec, takes, wav, map] {
        lis::exportMasterWav(spec, takes,
                             std::filesystem::path(wav.getFullPathName().toStdString()),
                             std::filesystem::path(map.getFullPathName().toStdString()));
    }, [this, wav, map] {
        setLabel(finishStatusLabel_, passText("MASTER WAV + epmap: " + wav.getFullPathName()));
        importTunedButton_.setEnabled(true);
    });
}

void MainComponent::importTunedAndBuild() {
    if (!epMapFile_.existsAsFile()) {
        setLabel(finishStatusLabel_, failText("EXPORT MASTER WAV first"));
        return;
    }
    fileChooser_.launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<MainComponent>(this)](const juce::FileChooser& chooser) {
            if (safe == nullptr) return;
            const auto tuned = chooser.getResult();
            if (!tuned.existsAsFile()) return;
            const auto out = safe->sessionDir_.getChildFile(safe->safeBankName() + "_tuned_epbank.bin");
            const auto map = safe->epMapFile_;
            const auto spec = safe->spec_;
            safe->runBackground("Building BIN from TUNED WAV...", [spec, tuned, map, out] {
                int sr = 0, start = 0, count = 0;
                const auto segs = lis::parseEpMapJson(std::filesystem::path(map.getFullPathName().toStdString()), sr, start, count);
                if (sr != spec.sampleRate || start != spec.startNote || count != spec.keyCount)
                    throw std::runtime_error("epmap does not match this session");
                const auto audio = lis::readWav(std::filesystem::path(tuned.getFullPathName().toStdString()));
                lis::buildEpBankFromMaster(spec, audio, segs, std::filesystem::path(out.getFullPathName().toStdString()));
            }, [safe, out] {
                if (safe != nullptr) setLabel(safe->finishStatusLabel_, passText("TUNED BIN: " + out.getFullPathName()));
            });
        });
}

void MainComponent::openSessionFolder() {
    if (sessionDir_.isDirectory()) sessionDir_.revealToUser();
}

void MainComponent::runBackground(const juce::String& taskName, std::function<void()> job, std::function<void()> onSuccess) {
    if (busy_.exchange(true)) return;
    setBusy(true, taskName);
    if (worker_.joinable()) worker_.join();
    auto safe = juce::Component::SafePointer<MainComponent>(this);
    worker_ = std::jthread([this, safe, job = std::move(job), onSuccess = std::move(onSuccess)] {
        juce::String error;
        try { job(); }
        catch (const std::exception& e) { error = e.what(); }
        catch (...) { error = "Unknown error"; }
        juce::MessageManager::callAsync([this, safe, error, onSuccess] {
            if (safe == nullptr) return;
            busy_.store(false);
            setBusy(false);
            if (error.isNotEmpty()) setLabel(finishStatusLabel_, failText(error));
            else if (onSuccess) onSuccess();
        });
    });
}

void MainComponent::setBusy(bool busy, const juce::String& message) {
    finishButton_.setEnabled(!busy && currentIndex_ == static_cast<int>(takes_.size()) && !takes_.empty());
    exportMasterButton_.setEnabled(!busy && currentIndex_ == static_cast<int>(takes_.size()) && !takes_.empty());
    importTunedButton_.setEnabled(!busy && epMapFile_.existsAsFile());
    if (busy) setLabel(finishStatusLabel_, message);
    if (currentIndex_ >= 0 && currentIndex_ < static_cast<int>(takes_.size())) recButton_.setEnabled(!busy && !audio_.isRecording());
}
