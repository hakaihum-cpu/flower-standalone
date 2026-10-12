#include "WavIO.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace lis {
namespace {

std::uint16_t readU16(std::istream& in) {
    std::array<unsigned char, 2> b{};
    in.read(reinterpret_cast<char*>(b.data()), 2);
    if (!in) throw std::runtime_error("Unexpected EOF while reading WAV");
    return static_cast<std::uint16_t>(b[0] | (static_cast<std::uint16_t>(b[1]) << 8));
}

std::uint32_t readU32(std::istream& in) {
    std::array<unsigned char, 4> b{};
    in.read(reinterpret_cast<char*>(b.data()), 4);
    if (!in) throw std::runtime_error("Unexpected EOF while reading WAV");
    return static_cast<std::uint32_t>(b[0]) |
           (static_cast<std::uint32_t>(b[1]) << 8) |
           (static_cast<std::uint32_t>(b[2]) << 16) |
           (static_cast<std::uint32_t>(b[3]) << 24);
}

void writeU16(std::ostream& out, std::uint16_t v) {
    const unsigned char b[2] = {
        static_cast<unsigned char>(v & 0xff),
        static_cast<unsigned char>((v >> 8) & 0xff)
    };
    out.write(reinterpret_cast<const char*>(b), 2);
}

void writeU32(std::ostream& out, std::uint32_t v) {
    const unsigned char b[4] = {
        static_cast<unsigned char>(v & 0xff),
        static_cast<unsigned char>((v >> 8) & 0xff),
        static_cast<unsigned char>((v >> 16) & 0xff),
        static_cast<unsigned char>((v >> 24) & 0xff)
    };
    out.write(reinterpret_cast<const char*>(b), 4);
}

bool idEquals(const char id[4], const char* s) {
    return std::memcmp(id, s, 4) == 0;
}

float decodePcmSample(const unsigned char* p, int bits) {
    switch (bits) {
        case 16: {
            std::int16_t v = static_cast<std::int16_t>(
                static_cast<std::uint16_t>(p[0]) |
                (static_cast<std::uint16_t>(p[1]) << 8));
            return static_cast<float>(v) / 32768.0f;
        }
        case 24: {
            std::int32_t v = static_cast<std::int32_t>(p[0]) |
                             (static_cast<std::int32_t>(p[1]) << 8) |
                             (static_cast<std::int32_t>(p[2]) << 16);
            if (v & 0x00800000) v |= ~0x00ffffff;
            return static_cast<float>(v) / 8388608.0f;
        }
        case 32: {
            std::int32_t v = static_cast<std::int32_t>(
                static_cast<std::uint32_t>(p[0]) |
                (static_cast<std::uint32_t>(p[1]) << 8) |
                (static_cast<std::uint32_t>(p[2]) << 16) |
                (static_cast<std::uint32_t>(p[3]) << 24));
            return static_cast<float>(static_cast<double>(v) / 2147483648.0);
        }
        default:
            throw std::runtime_error("Unsupported PCM bit depth");
    }
}

} // namespace

AudioData readWav(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open WAV: " + path.string());

    char riff[4]{};
    in.read(riff, 4);
    (void) readU32(in);
    char wave[4]{};
    in.read(wave, 4);
    if (!in || !idEquals(riff, "RIFF") || !idEquals(wave, "WAVE"))
        throw std::runtime_error("Not a RIFF/WAVE file: " + path.string());

    std::uint16_t audioFormat = 0;
    std::uint16_t channels = 0;
    std::uint32_t sampleRate = 0;
    std::uint16_t bitsPerSample = 0;
    std::vector<unsigned char> data;

    while (in && (!audioFormat || data.empty())) {
        char id[4]{};
        in.read(id, 4);
        if (!in) break;
        const auto size = readU32(in);
        const auto chunkStart = in.tellg();

        if (idEquals(id, "fmt ")) {
            audioFormat = readU16(in);
            channels = readU16(in);
            sampleRate = readU32(in);
            (void) readU32(in);
            (void) readU16(in);
            bitsPerSample = readU16(in);
        } else if (idEquals(id, "data")) {
            data.resize(size);
            in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
            if (!in) throw std::runtime_error("Truncated WAV data: " + path.string());
        }

        in.clear();
        in.seekg(chunkStart + static_cast<std::streamoff>(size + (size & 1u)));
    }

    if (channels < 1 || channels > 32 || sampleRate == 0 || data.empty())
        throw std::runtime_error("Incomplete WAV: " + path.string());

    if (audioFormat != 1 && audioFormat != 3)
        throw std::runtime_error("Only PCM/IEEE-float WAV is supported: " + path.string());

    if (audioFormat == 1 && bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 32)
        throw std::runtime_error("Unsupported PCM WAV bit depth: " + std::to_string(bitsPerSample));
    if (audioFormat == 3 && bitsPerSample != 32)
        throw std::runtime_error("Only 32-bit float WAV is supported");

    const std::size_t bytesPerSample = static_cast<std::size_t>(bitsPerSample / 8);
    const std::size_t sampleCount = data.size() / bytesPerSample;
    if (sampleCount % channels != 0)
        throw std::runtime_error("WAV sample data is not channel-aligned");

    AudioData out;
    out.sampleRate = static_cast<int>(sampleRate);
    out.channels = static_cast<int>(channels);
    out.interleaved.resize(sampleCount);

    for (std::size_t i = 0; i < sampleCount; ++i) {
        const auto* p = data.data() + i * bytesPerSample;
        if (audioFormat == 3) {
            float v{};
            std::uint32_t bits = static_cast<std::uint32_t>(p[0]) |
                                 (static_cast<std::uint32_t>(p[1]) << 8) |
                                 (static_cast<std::uint32_t>(p[2]) << 16) |
                                 (static_cast<std::uint32_t>(p[3]) << 24);
            std::memcpy(&v, &bits, sizeof(v));
            out.interleaved[i] = std::isfinite(v) ? std::clamp(v, -1.0f, 1.0f) : 0.0f;
        } else {
            out.interleaved[i] = decodePcmSample(p, bitsPerSample);
        }
    }

    return out;
}

std::int32_t floatToS24(float v) noexcept {
    v = std::clamp(v, -1.0f, 0.99999988f);
    const auto scaled = static_cast<long long>(std::llround(static_cast<double>(v) * 8388608.0));
    return static_cast<std::int32_t>(std::clamp<long long>(scaled, -8388608, 8388607));
}

void writeWav24(const std::filesystem::path& path, const AudioData& audio) {
    if (audio.sampleRate <= 0 || (audio.channels != 1 && audio.channels != 2))
        throw std::runtime_error("writeWav24 expects mono or stereo audio");
    if (audio.interleaved.size() % static_cast<std::size_t>(audio.channels) != 0)
        throw std::runtime_error("Audio buffer is not channel-aligned");

    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Cannot create WAV: " + path.string());

    const std::uint16_t channels = static_cast<std::uint16_t>(audio.channels);
    const std::uint16_t bits = 24;
    const std::uint16_t blockAlign = static_cast<std::uint16_t>(channels * 3);
    const std::uint32_t byteRate = static_cast<std::uint32_t>(audio.sampleRate) * blockAlign;
    const auto dataBytes64 = static_cast<std::uint64_t>(audio.interleaved.size()) * 3ull;
    if (dataBytes64 > 0xffffffffull - 36ull)
        throw std::runtime_error("WAV exceeds the classic RIFF 4 GiB limit; shorten the selected takes");
    const std::uint32_t dataBytes = static_cast<std::uint32_t>(dataBytes64);
    const std::uint32_t riffSize = 4u + (8u + 16u) + (8u + dataBytes);

    out.write("RIFF", 4);
    writeU32(out, riffSize);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    writeU32(out, 16);
    writeU16(out, 1);
    writeU16(out, channels);
    writeU32(out, static_cast<std::uint32_t>(audio.sampleRate));
    writeU32(out, byteRate);
    writeU16(out, blockAlign);
    writeU16(out, bits);
    out.write("data", 4);
    writeU32(out, dataBytes);

    for (float f : audio.interleaved) {
        const auto v = floatToS24(f);
        const unsigned char b[3] = {
            static_cast<unsigned char>(v & 0xff),
            static_cast<unsigned char>((v >> 8) & 0xff),
            static_cast<unsigned char>((v >> 16) & 0xff)
        };
        out.write(reinterpret_cast<const char*>(b), 3);
    }

    if (!out) throw std::runtime_error("Failed writing WAV: " + path.string());
}

AudioData sliceToStereo(const AudioData& src, std::uint64_t startFrame, std::uint64_t endFrame) {
    if (src.channels < 1 || src.channels > 2)
        throw std::runtime_error("Only mono/stereo source WAVs are supported");
    if (startFrame >= endFrame || endFrame > src.frames())
        throw std::runtime_error("Invalid trim range");

    AudioData out;
    out.sampleRate = src.sampleRate;
    out.channels = 2;
    const auto count = endFrame - startFrame;
    out.interleaved.resize(static_cast<std::size_t>(count) * 2u);

    for (std::uint64_t f = 0; f < count; ++f) {
        const auto sf = startFrame + f;
        const auto srcBase = static_cast<std::size_t>(sf) * static_cast<std::size_t>(src.channels);
        const float l = src.interleaved[srcBase];
        const float r = src.channels == 2 ? src.interleaved[srcBase + 1] : l;
        const auto dstBase = static_cast<std::size_t>(f) * 2u;
        out.interleaved[dstBase] = l;
        out.interleaved[dstBase + 1] = r;
    }
    return out;
}

AudioData silenceStereo(int sampleRate, std::uint64_t frames) {
    AudioData out;
    out.sampleRate = sampleRate;
    out.channels = 2;
    out.interleaved.assign(static_cast<std::size_t>(frames) * 2u, 0.0f);
    return out;
}

void appendAudio(AudioData& dstStereo, const AudioData& srcStereo) {
    if (srcStereo.channels != 2) throw std::runtime_error("appendAudio source must be stereo");
    if (dstStereo.channels == 0) {
        dstStereo.sampleRate = srcStereo.sampleRate;
        dstStereo.channels = 2;
    }
    if (dstStereo.channels != 2 || dstStereo.sampleRate != srcStereo.sampleRate)
        throw std::runtime_error("appendAudio format mismatch");
    dstStereo.interleaved.insert(dstStereo.interleaved.end(), srcStereo.interleaved.begin(), srcStereo.interleaved.end());
}

} // namespace lis
