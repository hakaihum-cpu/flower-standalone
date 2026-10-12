#include "../Core/SamplerCore.h"
#include "../Core/WavIO.h"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;

static constexpr double kPi = 3.1415926535897932384626433832795;

static lis::AudioData makeTone(int sr, int channels, double seconds, double hz) {
    lis::AudioData a;
    a.sampleRate = sr;
    a.channels = channels;
    const auto frames = static_cast<std::uint64_t>(std::llround(seconds * sr));
    a.interleaved.resize(static_cast<std::size_t>(frames) * channels);
    for (std::uint64_t i = 0; i < frames; ++i) {
        const float v = static_cast<float>(0.25 * std::sin(2.0 * kPi * hz * static_cast<double>(i) / sr));
        for (int c = 0; c < channels; ++c)
            a.interleaved[static_cast<std::size_t>(i) * channels + c] = v;
    }
    return a;
}

#pragma pack(push,1)
struct Header {
    char magic[8];
    std::uint32_t version, sampleRate, channels, bitsPerSample, entryCount, reserved;
    std::uint64_t dataOffset;
};
struct Entry {
    std::uint8_t note, velocity, rr, part;
    std::uint32_t frames;
    std::uint64_t offset, bytes;
};
#pragma pack(pop)

static void require(bool cond, const char* msg) {
    if (!cond) throw std::runtime_error(msg);
}

static void requireThrows(const std::function<void()>& fn, const char* msg) {
    bool threw = false;
    try { fn(); } catch (...) { threw = true; }
    require(threw, msg);
}

int main() {
    const auto root = fs::temp_directory_path() / "live_instrument_sampler_core_test";
    fs::remove_all(root);
    fs::create_directories(root / "raw");

    lis::SessionSpec spec;
    spec.startNote = 60;
    spec.keyCount = 3;
    spec.sampleRate = 48000;
    spec.silenceSeconds = 0.5;
    spec.releaseTailSeconds = 0.5;

    std::vector<lis::KeyTake> takes;
    for (int i = 0; i < 3; ++i) {
        const auto wav = root / "raw" / (std::to_string(60 + i) + ".wav");
        lis::writeWav24(wav, makeTone(48000, i == 0 ? 1 : 2, 1.2, 220.0 * (i + 1)));
        lis::KeyTake t;
        t.midiNote = 60 + i;
        t.rawWav = wav;
        t.trimStartFrame = 4800;
        t.trimEndFrame = 48000;
        t.confirmed = true;
        takes.push_back(t);
    }

    require(lis::noteName(60) == "C4", "noteName C4 failed");
    lis::validateSession(spec, takes);

    lis::SessionSpec c2_61;
    c2_61.startNote = 36;
    c2_61.keyCount = 61;
    c2_61.sampleRate = 48000;
    c2_61.silenceSeconds = 0.5;
    c2_61.releaseTailSeconds = 0.5;
    std::vector<lis::KeyTake> c2Takes;
    c2Takes.reserve(61);
    for (int i = 0; i < 61; ++i) {
        lis::KeyTake t;
        t.midiNote = 36 + i;
        t.rawWav = takes[0].rawWav;
        t.trimStartFrame = 4800;
        t.trimEndFrame = 48000;
        t.confirmed = true;
        c2Takes.push_back(t);
    }
    lis::validateSession(c2_61, c2Takes);

    auto tooHigh = c2_61;
    tooHigh.startNote = 60;
    requireThrows([&] { lis::validateSession(tooHigh, c2Takes); }, "range above C8 must fail");

    const auto masterWav = root / "master.wav";
    const auto mapJson = root / "master.epmap.json";
    auto master = lis::exportMasterWav(spec, takes, masterWav, mapJson);
    require(master.segments.size() == 3, "master segment count");
    require(master.segments[0].startFrame == 0, "first segment start");
    require(master.segments[0].lengthFrames == 43200, "trimmed length");
    require(master.segments[1].startFrame == 43200 + 24000, "second segment start");
    require(master.totalFrames == 3ull * (43200ull + 24000ull), "master total frames");

    const auto rereadMaster = lis::readWav(masterWav);
    require(rereadMaster.sampleRate == 48000 && rereadMaster.channels == 2, "master format");
    require(rereadMaster.frames() == master.totalFrames, "master frame count");

    int mapRate = 0, mapStart = 0, mapCount = 0;
    auto parsed = lis::parseEpMapJson(mapJson, mapRate, mapStart, mapCount);
    require(mapRate == 48000 && mapStart == 60 && mapCount == 3, "map scalar parse");
    require(parsed.size() == 3 && parsed[2].midiNote == 62, "map segment parse");

    const auto directBin = root / "direct.bin";
    lis::buildEpBankFromTakes(spec, takes, directBin);
    std::ifstream bin(directBin, std::ios::binary);
    Header h{};
    bin.read(reinterpret_cast<char*>(&h), sizeof(h));
    require(std::memcmp(h.magic, "EPBANK1\0", 8) == 0, "bank magic");
    require(h.version == 1 && h.sampleRate == 48000 && h.channels == 2 && h.bitsPerSample == 24, "bank header format");
    require(h.entryCount == 3u * 8u * 3u * 2u, "bank entry count");
    require(h.dataOffset == sizeof(Header) + static_cast<std::uint64_t>(h.entryCount) * sizeof(Entry), "bank data offset");
    for (std::uint32_t i = 0; i < h.entryCount; ++i) {
        Entry e{};
        bin.read(reinterpret_cast<char*>(&e), sizeof(e));
        require(e.offset + e.bytes <= fs::file_size(directBin), "entry bounds");
        require(e.note >= 60 && e.note <= 62, "entry note range");
        require(e.rr >= 1 && e.rr <= 3, "entry rr range");
        require(e.part <= 1, "entry part");
    }

    bin.close();

    const auto tunedBin = root / "tuned.bin";
    lis::buildEpBankFromMaster(spec, rereadMaster, parsed, tunedBin);
    require(fs::file_size(tunedBin) == fs::file_size(directBin), "tuned/direct size mismatch");

    auto shortMaster = rereadMaster;
    shortMaster.interleaved.resize(shortMaster.interleaved.size() / 2);
    requireThrows([&] { lis::buildEpBankFromMaster(spec, shortMaster, parsed, root / "short.bin"); },
                  "short tuned master must fail");

    fs::remove_all(root);
    std::cout << "ALL CORE TESTS PASSED\n";
    return 0;
}
