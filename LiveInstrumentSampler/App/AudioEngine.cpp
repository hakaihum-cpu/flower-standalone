#include "AudioEngine.h"

AudioEngine::AudioEngine() {
    writerThread_.startThread();
    deviceManager_.initialise(2, 2, nullptr, true);
    deviceManager_.closeAudioDevice();
    inputLevelMeter_ = deviceManager_.getInputLevelGetter();
    deviceManager_.addAudioCallback(this);
}

AudioEngine::~AudioEngine() {
    stopRecording();
    deviceManager_.removeAudioCallback(this);
    deviceManager_.closeAudioDevice();
    writerThread_.stopThread(2000);
}

juce::StringArray AudioEngine::getDriverTypes() {
    juce::OwnedArray<juce::AudioIODeviceType> types;
    deviceManager_.createAudioDeviceTypes(types);
    juce::StringArray out;
    for (auto* t : types)
        out.addIfNotAlreadyThere(t->getTypeName());
    return out;
}

juce::StringArray AudioEngine::getDeviceNamesForType(const juce::String& typeName, bool inputs) {
    juce::OwnedArray<juce::AudioIODeviceType> types;
    deviceManager_.createAudioDeviceTypes(types);
    for (auto* t : types) {
        if (t->getTypeName() == typeName) {
            t->scanForDevices();
            return t->getDeviceNames(inputs);
        }
    }
    return {};
}

AudioEngine::DeviceCapabilities AudioEngine::getDeviceCapabilities(const juce::String& typeName,
                                                                     const juce::String& inputDevice,
                                                                     const juce::String& outputDevice) {
    juce::OwnedArray<juce::AudioIODeviceType> types;
    deviceManager_.createAudioDeviceTypes(types);
    for (auto* t : types) {
        if (t->getTypeName() != typeName)
            continue;
        t->scanForDevices();
        std::unique_ptr<juce::AudioIODevice> dev(t->createDevice(outputDevice, inputDevice));
        if (dev == nullptr)
            return {};
        DeviceCapabilities caps;
        caps.inputChannels = dev->getInputChannelNames();
        caps.outputChannels = dev->getOutputChannelNames();
        caps.bufferSizes = dev->getAvailableBufferSizes();
        caps.sampleRates = dev->getAvailableSampleRates();
        return caps;
    }
    return {};
}

juce::String AudioEngine::applyDeviceSetup(const juce::String& driverType,
                                           const juce::String& inputDevice,
                                           const juce::String& outputDevice,
                                           int firstInputChannel,
                                           int inputChannelCount,
                                           int firstOutputChannel,
                                           int outputChannelCount,
                                           int bufferSize) {
    stopRecording();
    deviceManager_.closeAudioDevice();
    deviceManager_.setCurrentAudioDeviceType(driverType, true);

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    setup.inputDeviceName = inputDevice;
    setup.outputDeviceName = outputDevice;
    setup.sampleRate = 48000.0;
    setup.bufferSize = bufferSize;
    setup.useDefaultInputChannels = false;
    setup.useDefaultOutputChannels = false;
    setup.inputChannels.clear();
    setup.outputChannels.clear();

    for (int i = 0; i < inputChannelCount; ++i)
        if (firstInputChannel + i >= 0)
            setup.inputChannels.setBit(firstInputChannel + i);
    for (int i = 0; i < outputChannelCount; ++i)
        if (firstOutputChannel + i >= 0)
            setup.outputChannels.setBit(firstOutputChannel + i);

    auto err = deviceManager_.setAudioDeviceSetup(setup, true);
    if (err.isNotEmpty())
        return err;

    auto* d = deviceManager_.getCurrentAudioDevice();
    if (d == nullptr || !d->isOpen())
        return "Audio device did not open.";
    if (std::abs(d->getCurrentSampleRate() - 48000.0) > 0.5)
        return "Device opened, but not at 48000 Hz.";
    return {};
}

juce::String AudioEngine::startRecording(const juce::File& file) {
    stopRecording();
    auto* device = deviceManager_.getCurrentAudioDevice();
    if (device == nullptr || !device->isOpen())
        return "Open an audio device first.";
    if (std::abs(device->getCurrentSampleRate() - 48000.0) > 0.5)
        return "Recording requires 48000 Hz.";

    file.getParentDirectory().createDirectory();
    file.deleteFile();
    auto stream = file.createOutputStream();
    if (stream == nullptr)
        return "Could not create RAW WAV.";

    juce::WavAudioFormat wav;
    const auto options = juce::AudioFormatWriterOptions{}
                             .withSampleRate(48000.0)
                             .withNumChannels(2)
                             .withBitsPerSample(24);
    auto writer = wav.createWriterFor(stream, options);
    if (writer == nullptr)
        return "Could not create 24-bit WAV writer.";

    {
        const juce::ScopedLock sl(writerLock_);
        threadedWriter_ = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(writer.release(), writerThread_, 32768);
        activeWriter_ = threadedWriter_.get();
    }
    writerDropouts_.store(0, std::memory_order_release);
    xRunStart_ = deviceManager_.getXRunCount();
    xRunEnd_ = xRunStart_;
    recording_.store(true, std::memory_order_release);
    return {};
}

void AudioEngine::stopRecording() {
    const bool wasRecording = recording_.exchange(false, std::memory_order_acq_rel);
    {
        const juce::ScopedLock sl(writerLock_);
        activeWriter_ = nullptr;
        threadedWriter_.reset();
    }
    if (wasRecording)
        xRunEnd_ = deviceManager_.getXRunCount();
}

int AudioEngine::recordingXRuns() const noexcept {
    if (xRunStart_ < 0 || xRunEnd_ < 0)
        return 0;
    return juce::jmax(0, xRunEnd_ - xRunStart_);
}

float AudioEngine::currentInputLevel() const noexcept {
    return inputLevelMeter_ != nullptr ? static_cast<float>(inputLevelMeter_->getCurrentLevel()) : 0.0f;
}

bool AudioEngine::currentDeviceIsOpenAt48k() const noexcept {
    auto* d = deviceManager_.getCurrentAudioDevice();
    return d != nullptr && d->isOpen() && std::abs(d->getCurrentSampleRate() - 48000.0) < 0.5;
}

juce::String AudioEngine::currentDeviceSummary() const {
    auto* d = deviceManager_.getCurrentAudioDevice();
    if (d == nullptr) return "No device";
    return d->getTypeName() + " / " + d->getName() + " / "
        + juce::String(d->getCurrentSampleRate(), 0) + " Hz / "
        + juce::String(d->getCurrentBufferSizeSamples()) + " samples";
}

juce::String AudioEngine::reopenCurrentDevice() {
    const auto setup = deviceManager_.getAudioDeviceSetup();
    const auto type = deviceManager_.getCurrentAudioDeviceType();
    deviceManager_.closeAudioDevice();
    deviceManager_.setCurrentAudioDeviceType(type, true);
    const auto err = deviceManager_.setAudioDeviceSetup(setup, true);
    if (err.isNotEmpty()) return err;
    return currentDeviceIsOpenAt48k() ? juce::String{} : juce::String{"Device did not reopen at 48 kHz."};
}

void AudioEngine::playOutputTestTone() {
    deviceManager_.playTestSound();
}

void AudioEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                   int numInputChannels,
                                                   float* const* outputChannelData,
                                                   int numOutputChannels,
                                                   int numSamples,
                                                   const juce::AudioIODeviceCallbackContext&) {
    for (int c = 0; c < numOutputChannels; ++c)
        if (outputChannelData[c] != nullptr)
            juce::FloatVectorOperations::clear(outputChannelData[c], numSamples);

    if (!recording_.load(std::memory_order_acquire) || numInputChannels <= 0)
        return;

    if (stereoScratch_.getNumSamples() < numSamples) {
        writerDropouts_.fetch_add(1, std::memory_order_relaxed);
        return; // resized only in audioDeviceAboutToStart, never allocate here
    }

    stereoScratch_.clear();
    const float* left = nullptr;
    const float* right = nullptr;
    for (int c = 0; c < numInputChannels; ++c) {
        if (inputChannelData[c] == nullptr)
            continue;
        if (left == nullptr) left = inputChannelData[c];
        else if (right == nullptr) { right = inputChannelData[c]; break; }
    }
    if (left == nullptr) return;
    if (right == nullptr) right = left;
    stereoScratch_.copyFrom(0, 0, left, numSamples);
    stereoScratch_.copyFrom(1, 0, right, numSamples);
    const float* channels[2] = { stereoScratch_.getReadPointer(0), stereoScratch_.getReadPointer(1) };

    const juce::ScopedLock sl(writerLock_);
    if (activeWriter_ != nullptr && !activeWriter_->write(channels, numSamples))
        writerDropouts_.fetch_add(1, std::memory_order_relaxed);
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device) {
    currentSampleRate_ = device != nullptr ? device->getCurrentSampleRate() : 0.0;
    currentBlockSize_ = device != nullptr ? device->getCurrentBufferSizeSamples() : 0;
    stereoScratch_.setSize(2, juce::jmax(1, currentBlockSize_), false, false, true);
}

void AudioEngine::audioDeviceStopped() {
    stopRecording();
    currentSampleRate_ = 0.0;
    currentBlockSize_ = 0;
}
