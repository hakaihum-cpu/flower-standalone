#pragma once
#include <cstdint>
#include <string>
#include <array>
#include <vector>

class SampleBank {
public:
    enum Part : uint8_t { SUSTAIN = 0, RELEASE = 1 };

#pragma pack(push,1)
    struct Header {
        char magic[8];
        uint32_t version;
        uint32_t sampleRate;
        uint32_t channels;
        uint32_t bitsPerSample;
        uint32_t entryCount;
        uint32_t reserved;
        uint64_t dataOffset;
    };
    struct EntryDisk {
        uint8_t note;
        uint8_t velocity;
        uint8_t rr;
        uint8_t part;
        uint32_t frames;
        uint64_t offset;
        uint64_t bytes;
    };
#pragma pack(pop)

    struct Entry {
        uint8_t note = 0;
        uint8_t velocity = 0;
        uint8_t rr = 0;
        uint8_t part = 0;
        uint32_t frames = 0;
        uint64_t offset = 0;
        uint64_t bytes = 0;
    };

    SampleBank();
    ~SampleBank();
    bool load(const std::string& path);
    bool loadFd(int fd);
    void unload();
    bool loaded() const { return mapped_ != nullptr; }
    uint32_t sampleRate() const { return sampleRate_; }
    uint32_t bitsPerSample() const { return bits_; }
    std::string status() const;

    const Entry* find(uint8_t note, uint8_t velocity, uint8_t rr, uint8_t part) const;
    float read(const Entry* e, double frame, int channel) const;

private:
    int fd_ = -1;
    uint8_t* mapped_ = nullptr;
    size_t mappedBytes_ = 0;
    uint32_t sampleRate_ = 48000;
    uint32_t channels_ = 2;
    uint32_t bits_ = 24;
    std::vector<Entry> entries_;
    std::array<int32_t, 128*128*4*2> lookup_{};

    static size_t key(int note, int velocity, int rr, int part) {
        return (((note * 128 + velocity) * 4 + rr) * 2 + part);
    }
    float readFrame(const Entry* e, uint32_t frame, int channel) const;
};
