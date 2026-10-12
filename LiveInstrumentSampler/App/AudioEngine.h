#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>

class AudioEngine final : public juce::AudioIODeviceCallback {
public:
    struct DeviceCapabilities {
        juce::StringArray inputChannels;
        juce::StringArray outputChannels;
        juce::Array<int> bufferSizes;
        juce::Array<double> sampleRates;
    };

    AudioEngine();
    ~AudioEngine() override;

    juce::AudioDeviceManager& deviceManager() noexcept { return deviceManager_; }

    juce::StringArray getDriverTypes();
    juce::StringArray getDeviceNamesForType(const juce::String& typeName, bool inputs);
    DeviceCapabilities getDeviceCapabilities(const juce::String& typeName,
                                             const juce::String& inputDevice,
                                             const juce::String& outputDevice);
    juce::String applyDeviceSetup(const juce::String& driverType,
                                  const juce::String& inputDevice,
                                  const juce::String& outputDevice,
                                  int firstInputChannel,
                                  int inputChannelCount,
                                  int firstOutputChannel,
                                  int outputChannelCount,
                                  int bufferSize);

    juce::String startRecording(const juce::File& file);
    void stopRecording();
    bool isRecording() const noexcept { return recording_.load(); }
    int recordingWriteDropouts() const noexcept { return writerDropouts_.load(); }
    int recordingXRuns() const noexcept;

    void setMonitoringEnabled(bool enabled) noexcept { monitoringEnabled_.store(enabled); }
    bool isMonitoringEnabled() const noexcept { return monitoringEnabled_.load(); }
    void setInputGainDb(float db) noexcept;
    float inputGainDb() const noexcept { return inputGainDb_.load(); }
    float currentProcessedPeakDb() const noexcept;
    bool consumeInputClip() noexcept { return inputClipped_.exchange(false); }

    juce::String startPreview(const juce::File& file, double startSeconds, double endSeconds);
    void stopPreview();
    bool isPreviewing() const noexcept { return previewing_.load(); }

    float currentInputLevel() const noexcept;
    bool currentDeviceIsOpenAt48k() const noexcept;
    juce::String currentDeviceSummary() const;
    juce::String reopenCurrentDevice();
    void playOutputTestTone();

    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

private:
    struct PreviewData {
        juce::AudioBuffer<float> audio;
    };

    juce::AudioDeviceManager deviceManager_;
    juce::AudioDeviceManager::LevelMeter::Ptr inputLevelMeter_;
    juce::AudioFormatManager previewFormatManager_;
    juce::TimeSliceThread writerThread_{"LiveSampler WAV writer"};
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter_;
    juce::CriticalSection writerLock_;
    juce::AudioFormatWriter::ThreadedWriter* activeWriter_ = nullptr;
    std::atomic<bool> recording_{false};
    std::atomic<int> writerDropouts_{0};
    int xRunStart_ = -1;
    int xRunEnd_ = -1;

    std::atomic<bool> monitoringEnabled_{false};
    std::atomic<float> inputGainDb_{0.0f};
    std::atomic<float> processedPeak_{0.0f};
    std::atomic<bool> inputClipped_{false};

    std::shared_ptr<PreviewData> previewData_;
    std::atomic<int> previewPosition_{0};
    std::atomic<bool> previewing_{false};

    juce::AudioBuffer<float> stereoScratch_;
    double currentSampleRate_ = 0.0;
    int currentBlockSize_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
