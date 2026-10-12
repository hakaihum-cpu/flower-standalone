#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace lis {

struct AudioData {
    int sampleRate = 0;
    int channels = 0;
    std::vector<float> interleaved;

    [[nodiscard]] std::uint64_t frames() const noexcept {
        return channels > 0 ? static_cast<std::uint64_t>(interleaved.size() / static_cast<std::size_t>(channels)) : 0;
    }
};

AudioData readWav(const std::filesystem::path& path);
void writeWav24(const std::filesystem::path& path, const AudioData& audio);

AudioData sliceToStereo(const AudioData& src, std::uint64_t startFrame, std::uint64_t endFrame);
AudioData silenceStereo(int sampleRate, std::uint64_t frames);
void appendAudio(AudioData& dstStereo, const AudioData& srcStereo);

std::int32_t floatToS24(float v) noexcept;

} // namespace lis
