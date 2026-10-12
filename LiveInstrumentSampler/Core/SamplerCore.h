#pragma once

#include "WavIO.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace lis {

struct KeyTake {
    int midiNote = 60;
    std::filesystem::path rawWav;
    std::uint64_t trimStartFrame = 0;
    std::uint64_t trimEndFrame = 0;
    bool confirmed = false;
};

struct SessionSpec {
    int startNote = 36;
    int keyCount = 61;
    int sampleRate = 48000;
    double silenceSeconds = 0.5;
    double releaseTailSeconds = 0.5;
};

struct MasterSegment {
    int midiNote = 60;
    std::uint64_t startFrame = 0;
    std::uint64_t lengthFrames = 0;
};

struct MasterExportResult {
    std::filesystem::path wavPath;
    std::filesystem::path mapPath;
    std::vector<MasterSegment> segments;
    std::uint64_t totalFrames = 0;
};

std::string noteName(int midiNote);
void validateSession(const SessionSpec& spec, const std::vector<KeyTake>& takes);
MasterExportResult exportMasterWav(const SessionSpec& spec,
                                   const std::vector<KeyTake>& takes,
                                   const std::filesystem::path& outWav,
                                   const std::filesystem::path& outMapJson);

void buildEpBankFromTakes(const SessionSpec& spec,
                          const std::vector<KeyTake>& takes,
                          const std::filesystem::path& outBin);

void buildEpBankFromMaster(const SessionSpec& spec,
                           const AudioData& masterStereo,
                           const std::vector<MasterSegment>& segments,
                           const std::filesystem::path& outBin);

std::vector<MasterSegment> parseEpMapJson(const std::filesystem::path& mapPath,
                                          int& sampleRateOut,
                                          int& startNoteOut,
                                          int& keyCountOut);

} // namespace lis
