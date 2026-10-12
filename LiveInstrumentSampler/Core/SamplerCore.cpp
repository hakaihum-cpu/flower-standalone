#include "SamplerCore.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace lis {
namespace {

#pragma pack(push, 1)
struct BankHeader {
    char magic[8];
    std::uint32_t version;
    std::uint32_t sampleRate;
    std::uint32_t channels;
    std::uint32_t bitsPerSample;
    std::uint32_t entryCount;
    std::uint32_t reserved;
    std::uint64_t dataOffset;
};
struct BankEntry {
    std::uint8_t note;
    std::uint8_t velocity;
    std::uint8_t rr;
    std::uint8_t part;
    std::uint32_t frames;
    std::uint64_t offset;
    std::uint64_t bytes;
};
#pragma pack(pop)

static_assert(sizeof(BankHeader) == 40);
static_assert(sizeof(BankEntry) == 24);

constexpr std::array<int, 8> kVelocities{16, 32, 48, 64, 80, 96, 112, 127};
constexpr std::array<int, 3> kRoundRobins{1, 2, 3};
constexpr std::uint8_t kSustain = 0;
constexpr std::uint8_t kRelease = 1;
constexpr int kBytesPerFrame = 6;

struct PreparedNote {
    int midiNote = 60;
    AudioData audio;
};

void writeS24(std::ostream& out, float sample) {
    const std::int32_t v = floatToS24(sample);
    const unsigned char b[3] = {
        static_cast<unsigned char>(v & 0xff),
        static_cast<unsigned char>((v >> 8) & 0xff),
        static_cast<unsigned char>((v >> 16) & 0xff)
    };
    out.write(reinterpret_cast<const char*>(b), 3);
}

void validateMidiRange(int startNote, int keyCount) {
    constexpr int kEpMinNote = 21;
    constexpr int kEpMaxNote = 108;
    if (startNote < kEpMinNote || startNote > kEpMaxNote)
        throw std::runtime_error("START NOTE must be A0-C8 (MIDI 21-108) for the existing EP-SAMPLE app");
    if (keyCount < 1 || keyCount > 88)
        throw std::runtime_error("KEYS must be 1-88");
    if (startNote + keyCount - 1 > kEpMaxNote)
        throw std::runtime_error("START NOTE + KEYS exceeds C8 (MIDI 108)");
}

void buildBankPrepared(const SessionSpec& spec,
                       const std::vector<PreparedNote>& notes,
                       const std::filesystem::path& outBin) {
    validateMidiRange(spec.startNote, spec.keyCount);
    if (spec.sampleRate != 48000)
        throw std::runtime_error("EPBANK1 writer currently requires 48000 Hz source audio");
    if (static_cast<int>(notes.size()) != spec.keyCount)
        throw std::runtime_error("Prepared note count does not match KEYS");

    for (int i = 0; i < spec.keyCount; ++i) {
        if (notes[static_cast<std::size_t>(i)].midiNote != spec.startNote + i)
            throw std::runtime_error("Prepared notes are not contiguous in MIDI order");
        const auto& a = notes[static_cast<std::size_t>(i)].audio;
        if (a.sampleRate != 48000 || a.channels != 2 || a.frames() == 0)
            throw std::runtime_error("Prepared audio must be non-empty 48kHz stereo");
        if (a.frames() > 0xffffffffull)
            throw std::runtime_error("A single note recording is too long for EPBANK1 v1");
    }

    const std::uint32_t entryCount = static_cast<std::uint32_t>(notes.size() * kVelocities.size() * kRoundRobins.size() * 2u);
    const std::uint64_t dataOffset = sizeof(BankHeader) + static_cast<std::uint64_t>(entryCount) * sizeof(BankEntry);

    std::vector<std::uint64_t> noteOffsets(notes.size());
    std::uint64_t cursor = dataOffset;
    for (std::size_t i = 0; i < notes.size(); ++i) {
        noteOffsets[i] = cursor;
        cursor += notes[i].audio.frames() * kBytesPerFrame;
    }

    std::filesystem::create_directories(outBin.parent_path());
    std::ofstream out(outBin, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Cannot create BIN: " + outBin.string());

    BankHeader h{};
    std::memcpy(h.magic, "EPBANK1\0", 8);
    h.version = 1;
    h.sampleRate = 48000;
    h.channels = 2;
    h.bitsPerSample = 24;
    h.entryCount = entryCount;
    h.reserved = 0;
    h.dataOffset = dataOffset;
    out.write(reinterpret_cast<const char*>(&h), sizeof(h));

    const auto releaseFramesConfigured = static_cast<std::uint64_t>(std::llround(spec.releaseTailSeconds * spec.sampleRate));

    for (std::size_t i = 0; i < notes.size(); ++i) {
        const auto totalFrames = notes[i].audio.frames();
        const auto releaseFrames = std::max<std::uint64_t>(1, std::min(totalFrames, releaseFramesConfigured));
        const auto releaseStart = totalFrames - releaseFrames;
        const auto noteOffset = noteOffsets[i];

        for (int vel : kVelocities) {
            for (int rr : kRoundRobins) {
                BankEntry sus{};
                sus.note = static_cast<std::uint8_t>(notes[i].midiNote);
                sus.velocity = static_cast<std::uint8_t>(vel);
                sus.rr = static_cast<std::uint8_t>(rr);
                sus.part = kSustain;
                sus.frames = static_cast<std::uint32_t>(totalFrames);
                sus.offset = noteOffset;
                sus.bytes = totalFrames * kBytesPerFrame;
                out.write(reinterpret_cast<const char*>(&sus), sizeof(sus));

                BankEntry rel{};
                rel.note = static_cast<std::uint8_t>(notes[i].midiNote);
                rel.velocity = static_cast<std::uint8_t>(vel);
                rel.rr = static_cast<std::uint8_t>(rr);
                rel.part = kRelease;
                rel.frames = static_cast<std::uint32_t>(releaseFrames);
                rel.offset = noteOffset + releaseStart * kBytesPerFrame;
                rel.bytes = releaseFrames * kBytesPerFrame;
                out.write(reinterpret_cast<const char*>(&rel), sizeof(rel));
            }
        }
    }

    if (static_cast<std::uint64_t>(out.tellp()) != dataOffset)
        throw std::runtime_error("EPBANK1 index size mismatch");

    for (const auto& n : notes) {
        for (float f : n.audio.interleaved)
            writeS24(out, f);
    }

    if (!out) throw std::runtime_error("Failed writing BIN: " + outBin.string());
}

std::string jsonEscape(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        switch (c) {
            case '\\': o << "\\\\"; break;
            case '"': o << "\\\""; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default: o << c; break;
        }
    }
    return o.str();
}

} // namespace

std::string noteName(int midiNote) {
    static constexpr std::array<const char*, 12> names{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    if (midiNote < 0 || midiNote > 127) return "?";
    return std::string(names[static_cast<std::size_t>(midiNote % 12)]) + std::to_string(midiNote / 12 - 1);
}

void validateSession(const SessionSpec& spec, const std::vector<KeyTake>& takes) {
    validateMidiRange(spec.startNote, spec.keyCount);
    if (spec.sampleRate != 48000)
        throw std::runtime_error("Session sample rate must be 48000 Hz for EP-SAMPLE");
    if (spec.silenceSeconds < 0.0 || spec.silenceSeconds > 10.0)
        throw std::runtime_error("Invalid master-WAV silence duration");
    if (spec.releaseTailSeconds <= 0.0 || spec.releaseTailSeconds > 10.0)
        throw std::runtime_error("Invalid release-tail duration");
    if (static_cast<int>(takes.size()) != spec.keyCount)
        throw std::runtime_error("Take count does not match KEYS");

    for (int i = 0; i < spec.keyCount; ++i) {
        const auto& t = takes[static_cast<std::size_t>(i)];
        if (t.midiNote != spec.startNote + i)
            throw std::runtime_error("Takes are not contiguous in MIDI order");
        if (!t.confirmed)
            throw std::runtime_error("Not all keys are SET");
        if (t.rawWav.empty() || !std::filesystem::exists(t.rawWav))
            throw std::runtime_error("RAW WAV is missing for " + noteName(t.midiNote));
        if (t.trimStartFrame >= t.trimEndFrame)
            throw std::runtime_error("Invalid trim range for " + noteName(t.midiNote));
    }
}

MasterExportResult exportMasterWav(const SessionSpec& spec,
                                   const std::vector<KeyTake>& takes,
                                   const std::filesystem::path& outWav,
                                   const std::filesystem::path& outMapJson) {
    validateSession(spec, takes);

    AudioData master;
    master.sampleRate = spec.sampleRate;
    master.channels = 2;
    std::vector<MasterSegment> segments;
    const auto silenceFrames = static_cast<std::uint64_t>(std::llround(spec.silenceSeconds * spec.sampleRate));

    for (const auto& t : takes) {
        const auto raw = readWav(t.rawWav);
        if (raw.sampleRate != spec.sampleRate)
            throw std::runtime_error("RAW sample rate is not 48kHz: " + t.rawWav.string());
        const auto trimmed = sliceToStereo(raw, t.trimStartFrame, t.trimEndFrame);
        MasterSegment seg;
        seg.midiNote = t.midiNote;
        seg.startFrame = master.frames();
        seg.lengthFrames = trimmed.frames();
        segments.push_back(seg);
        appendAudio(master, trimmed);
        if (silenceFrames > 0)
            appendAudio(master, silenceStereo(spec.sampleRate, silenceFrames));
    }

    writeWav24(outWav, master);

    std::filesystem::create_directories(outMapJson.parent_path());
    std::ofstream map(outMapJson, std::ios::trunc);
    if (!map) throw std::runtime_error("Cannot create epmap JSON");
    map << "{\n";
    map << "  \"format\": \"EP_SAMPLE_MASTER_MAP_V1\",\n";
    map << "  \"sampleRate\": " << spec.sampleRate << ",\n";
    map << "  \"channels\": 2,\n";
    map << "  \"bitsPerSample\": 24,\n";
    map << "  \"startNote\": " << spec.startNote << ",\n";
    map << "  \"keyCount\": " << spec.keyCount << ",\n";
    map << "  \"silenceFrames\": " << silenceFrames << ",\n";
    map << "  \"segments\": [\n";
    for (std::size_t i = 0; i < segments.size(); ++i) {
        const auto& s = segments[i];
        map << "    {\"midiNote\": " << s.midiNote
            << ", \"note\": \"" << jsonEscape(noteName(s.midiNote)) << "\""
            << ", \"startFrame\": " << s.startFrame
            << ", \"lengthFrames\": " << s.lengthFrames << "}";
        map << (i + 1 == segments.size() ? "\n" : ",\n");
    }
    map << "  ]\n";
    map << "}\n";

    MasterExportResult result;
    result.wavPath = outWav;
    result.mapPath = outMapJson;
    result.segments = std::move(segments);
    result.totalFrames = master.frames();
    return result;
}

void buildEpBankFromTakes(const SessionSpec& spec,
                          const std::vector<KeyTake>& takes,
                          const std::filesystem::path& outBin) {
    validateSession(spec, takes);
    std::vector<PreparedNote> prepared;
    prepared.reserve(takes.size());
    for (const auto& t : takes) {
        const auto raw = readWav(t.rawWav);
        if (raw.sampleRate != spec.sampleRate)
            throw std::runtime_error("RAW sample rate is not 48kHz: " + t.rawWav.string());
        prepared.push_back({t.midiNote, sliceToStereo(raw, t.trimStartFrame, t.trimEndFrame)});
    }
    buildBankPrepared(spec, prepared, outBin);
}

void buildEpBankFromMaster(const SessionSpec& spec,
                           const AudioData& masterStereo,
                           const std::vector<MasterSegment>& segments,
                           const std::filesystem::path& outBin) {
    validateMidiRange(spec.startNote, spec.keyCount);
    if (masterStereo.sampleRate != spec.sampleRate)
        throw std::runtime_error("Tuned master WAV must be 48kHz");
    if (masterStereo.channels < 1 || masterStereo.channels > 2)
        throw std::runtime_error("Tuned master WAV must be mono or stereo");
    if (static_cast<int>(segments.size()) != spec.keyCount)
        throw std::runtime_error("epmap key count does not match session");

    std::vector<PreparedNote> prepared;
    prepared.reserve(segments.size());
    for (int i = 0; i < spec.keyCount; ++i) {
        const auto& seg = segments[static_cast<std::size_t>(i)];
        if (seg.midiNote != spec.startNote + i)
            throw std::runtime_error("epmap note sequence does not match session");
        if (seg.lengthFrames == 0 || seg.startFrame + seg.lengthFrames > masterStereo.frames())
            throw std::runtime_error("Tuned master WAV is shorter than epmap segment data");
        prepared.push_back({seg.midiNote,
                            sliceToStereo(masterStereo, seg.startFrame, seg.startFrame + seg.lengthFrames)});
    }
    buildBankPrepared(spec, prepared, outBin);
}

std::vector<MasterSegment> parseEpMapJson(const std::filesystem::path& mapPath,
                                          int& sampleRateOut,
                                          int& startNoteOut,
                                          int& keyCountOut) {
    std::ifstream in(mapPath);
    if (!in) throw std::runtime_error("Cannot open epmap JSON: " + mapPath.string());
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    auto scalar = [&](const char* name) -> int {
        const std::regex re(std::string("\\\"") + name + "\\\"\\s*:\\s*([0-9]+)");
        std::smatch m;
        if (!std::regex_search(text, m, re))
            throw std::runtime_error(std::string("Missing epmap field: ") + name);
        return std::stoi(m[1].str());
    };

    sampleRateOut = scalar("sampleRate");
    startNoteOut = scalar("startNote");
    keyCountOut = scalar("keyCount");

    std::vector<MasterSegment> out;
    const std::regex segRe(R"(\{\s*"midiNote"\s*:\s*([0-9]+)\s*,\s*"note"\s*:\s*"[^"]*"\s*,\s*"startFrame"\s*:\s*([0-9]+)\s*,\s*"lengthFrames"\s*:\s*([0-9]+)\s*\})");
    for (std::sregex_iterator it(text.begin(), text.end(), segRe), end; it != end; ++it) {
        MasterSegment s;
        s.midiNote = std::stoi((*it)[1].str());
        s.startFrame = static_cast<std::uint64_t>(std::stoull((*it)[2].str()));
        s.lengthFrames = static_cast<std::uint64_t>(std::stoull((*it)[3].str()));
        out.push_back(s);
    }
    if (static_cast<int>(out.size()) != keyCountOut)
        throw std::runtime_error("epmap segment count does not match keyCount");
    return out;
}

} // namespace lis
