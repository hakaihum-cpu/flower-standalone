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
    juce::AudioDeviceManager deviceManager_;
    juce::AudioDeviceManager::LevelMeter::Ptr inputLevelMeter_;
    juce::TimeSliceThread writerThread_{"LiveSampler WAV writer"};
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter_;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> activeWriter_{nullptr};
    std::atomic<bool> recording_{false};
    std::atomic<int> writerDropouts_{0};
    int xRunStart_ = -1;
    int xRunEnd_ = -1;
    juce::AudioBuffer<float> stereoScratch_;
    double currentSampleRate_ = 0.0;
    int currentBlockSize_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
