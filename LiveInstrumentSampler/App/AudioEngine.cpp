#include "AudioEngine.h"

#include <cmath>
#include <limits>

AudioEngine::AudioEngine() {
    previewFormatManager_.registerBasicFormats();
    writerThread_.startThread();
    deviceManager_.initialise(2, 2, nullptr, true);
    deviceManager_.closeAudioDevice();
    inputLevelMeter_ = deviceManager_.getInputLevelGetter();
    deviceManager_.addAudioCallback(this);
}

AudioEngine::~AudioEngine() {
    stopPreview();
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
    stopPreview();
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
    stopPreview();
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
    auto* writer = wav.createWriterFor(stream.release(), 48000.0, 2, 24, {}, 0);
    if (writer == nullptr)
        return "Could not create 24-bit WAV writer.";

    {
        const juce::ScopedLock sl(writerLock_);
        threadedWriter_ = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(writer, writerThread_, 32768);
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

void AudioEngine::setInputGainDb(float db) noexcept {
    inputGainDb_.store(juce::jlimit(-24.0f, 24.0f, db));
}

float AudioEngine::currentProcessedPeakDb() const noexcept {
    const auto peak = processedPeak_.load();
    return peak > 0.00001f ? juce::Decibels::gainToDecibels(peak, -100.0f) : -100.0f;
}

juce::String AudioEngine::startPreview(const juce::File& file, double startSeconds, double endSeconds) {
    stopPreview();

    if (recording_.load())
        return "Stop recording before preview.";

    auto* device = deviceManager_.getCurrentAudioDevice();
    if (device == nullptr || !device->isOpen())
        return "Open an audio device first.";

    if (!file.existsAsFile())
        return "Preview file does not exist.";

    std::unique_ptr<juce::AudioFormatReader> reader(previewFormatManager_.createReaderFor(file));
    if (reader == nullptr)
        return "Could not read preview WAV.";

    if (std::abs(reader->sampleRate - 48000.0) > 0.5)
        return "Preview requires a 48000 Hz WAV.";

    const auto startFrame = juce::jlimit<juce::int64>(
        0, reader->lengthInSamples,
        static_cast<juce::int64>(std::llround(startSeconds * reader->sampleRate)));
    const auto endFrame = juce::jlimit<juce::int64>(
        startFrame, reader->lengthInSamples,
        static_cast<juce::int64>(std::llround(endSeconds * reader->sampleRate)));

    const auto frameCount64 = endFrame - startFrame;
    if (frameCount64 <= 0)
        return "Preview range is empty.";
    if (frameCount64 > static_cast<juce::int64>(std::numeric_limits<int>::max()))
        return "Preview range is too long.";

    const int frameCount = static_cast<int>(frameCount64);
    auto data = std::make_shared<PreviewData>();
    data->audio.setSize(2, frameCount);
    data->audio.clear();

    if (!reader->read(&data->audio, 0, frameCount, startFrame, true, true))
        return "Could not read the selected preview range.";

    std::atomic_store_explicit(&previewData_, data, std::memory_order_release);
    previewPosition_.store(0);
    previewing_.store(true);
    return {};
}

void AudioEngine::stopPreview() {
    previewing_.store(false);
    previewPosition_.store(0);
    std::atomic_store_explicit(&previewData_, std::shared_ptr<PreviewData>{}, std::memory_order_release);
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

    const bool recording = recording_.load(std::memory_order_acquire);
    const bool monitoring = monitoringEnabled_.load(std::memory_order_acquire);

    const float* left = nullptr;
    const float* right = nullptr;
    for (int c = 0; c < numInputChannels; ++c) {
        if (inputChannelData[c] == nullptr)
            continue;
        if (left == nullptr) left = inputChannelData[c];
        else if (right == nullptr) { right = inputChannelData[c]; break; }
    }

    if (left != nullptr) {
        if (right == nullptr)
            right = left;

        if (stereoScratch_.getNumSamples() >= numSamples) {
            const float gain = std::pow(10.0f, inputGainDb_.load(std::memory_order_relaxed) / 20.0f);
            float peak = 0.0f;
            bool clipped = false;

            for (int i = 0; i < numSamples; ++i) {
                const float l = left[i] * gain;
                const float r = right[i] * gain;
                peak = juce::jmax(peak, juce::jmax(std::abs(l), std::abs(r)));
                clipped = clipped || std::abs(l) > 1.0f || std::abs(r) > 1.0f;
                stereoScratch_.setSample(0, i, juce::jlimit(-1.0f, 1.0f, l));
                stereoScratch_.setSample(1, i, juce::jlimit(-1.0f, 1.0f, r));
            }

            processedPeak_.store(peak, std::memory_order_relaxed);
            if (clipped)
                inputClipped_.store(true, std::memory_order_relaxed);

            if (monitoring) {
                int activeOut = 0;
                for (int c = 0; c < numOutputChannels; ++c) {
                    if (outputChannelData[c] == nullptr)
                        continue;
                    const int src = activeOut == 0 ? 0 : 1;
                    juce::FloatVectorOperations::add(outputChannelData[c], stereoScratch_.getReadPointer(src), numSamples);
                    ++activeOut;
                }
            }

            if (recording) {
                const float* channels[2] = { stereoScratch_.getReadPointer(0), stereoScratch_.getReadPointer(1) };
                const juce::ScopedLock sl(writerLock_);
                if (activeWriter_ != nullptr && !activeWriter_->write(channels, numSamples))
                    writerDropouts_.fetch_add(1, std::memory_order_relaxed);
            }
        } else {
            processedPeak_.store(0.0f, std::memory_order_relaxed);
            if (recording)
                writerDropouts_.fetch_add(1, std::memory_order_relaxed);
        }
    } else {
        processedPeak_.store(0.0f, std::memory_order_relaxed);
    }

    if (previewing_.load(std::memory_order_acquire)) {
        auto data = std::atomic_load_explicit(&previewData_, std::memory_order_acquire);
        if (data != nullptr) {
            const int position = previewPosition_.load(std::memory_order_relaxed);
            const int remaining = data->audio.getNumSamples() - position;
            const int amount = juce::jmin(numSamples, juce::jmax(0, remaining));

            if (amount > 0) {
                int activeOut = 0;
                for (int c = 0; c < numOutputChannels; ++c) {
                    if (outputChannelData[c] == nullptr)
                        continue;
                    const int src = activeOut == 0 ? 0 : 1;
                    juce::FloatVectorOperations::add(
                        outputChannelData[c],
                        data->audio.getReadPointer(src, position),
                        amount);
                    ++activeOut;
                }
                previewPosition_.store(position + amount, std::memory_order_relaxed);
            }

            if (amount <= 0 || position + amount >= data->audio.getNumSamples())
                previewing_.store(false, std::memory_order_release);
        } else {
            previewing_.store(false, std::memory_order_release);
        }
    }

    for (int c = 0; c < numOutputChannels; ++c) {
        if (outputChannelData[c] == nullptr)
            continue;
        for (int i = 0; i < numSamples; ++i)
            outputChannelData[c][i] = juce::jlimit(-1.0f, 1.0f, outputChannelData[c][i]);
    }
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device) {
    currentSampleRate_ = device != nullptr ? device->getCurrentSampleRate() : 0.0;
    currentBlockSize_ = device != nullptr ? device->getCurrentBufferSizeSamples() : 0;
    stereoScratch_.setSize(2, juce::jmax(1, currentBlockSize_), false, false, true);
}

void AudioEngine::audioDeviceStopped() {
    stopPreview();
    stopRecording();
    currentSampleRate_ = 0.0;
    currentBlockSize_ = 0;
}
