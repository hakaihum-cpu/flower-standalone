#include "SampleBank.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <algorithm>

SampleBank::SampleBank() { lookup_.fill(-1); }
SampleBank::~SampleBank() { unload(); }

void SampleBank::unload() {
    if (mapped_) { munmap(mapped_, mappedBytes_); mapped_ = nullptr; }
    if (fd_ >= 0) { close(fd_); fd_ = -1; }
    mappedBytes_ = 0;
    entries_.clear();
    lookup_.fill(-1);
}

bool SampleBank::load(const std::string& path) {
    int sourceFd = open(path.c_str(), O_RDONLY);
    if (sourceFd < 0) return false;
    bool ok = loadFd(sourceFd);
    close(sourceFd);
    return ok;
}

bool SampleBank::loadFd(int sourceFd) {
    unload();
    if (sourceFd < 0) return false;
    fd_ = dup(sourceFd);
    if (fd_ < 0) return false;
    struct stat st{};
    if (fstat(fd_, &st) != 0 || st.st_size < (off_t)sizeof(Header)) { unload(); return false; }
    mappedBytes_ = static_cast<size_t>(st.st_size);
    mapped_ = reinterpret_cast<uint8_t*>(mmap(nullptr, mappedBytes_, PROT_READ, MAP_SHARED, fd_, 0));
    if (mapped_ == MAP_FAILED) { mapped_ = nullptr; unload(); return false; }

    const auto* h = reinterpret_cast<const Header*>(mapped_);
    if (std::memcmp(h->magic, "EPBANK1", 7) != 0 || h->version != 1 || h->channels != 2) { unload(); return false; }
    if (h->bitsPerSample != 16 && h->bitsPerSample != 24) { unload(); return false; }
    const uint64_t indexEnd = sizeof(Header) + uint64_t(h->entryCount) * sizeof(EntryDisk);
    if (indexEnd > mappedBytes_ || h->dataOffset < indexEnd || h->dataOffset > mappedBytes_) { unload(); return false; }

    sampleRate_ = h->sampleRate;
    channels_ = h->channels;
    bits_ = h->bitsPerSample;
    entries_.reserve(h->entryCount);
    lookup_.fill(-1);
    const auto* disk = reinterpret_cast<const EntryDisk*>(mapped_ + sizeof(Header));
    for (uint32_t i=0; i<h->entryCount; ++i) {
        const auto& d = disk[i];
        if (d.note > 127 || d.velocity > 127 || d.rr > 3 || d.part > 1) continue;
        if (d.offset + d.bytes > mappedBytes_) continue;
        Entry e{d.note,d.velocity,d.rr,d.part,d.frames,d.offset,d.bytes};
        int idx = static_cast<int>(entries_.size());
        entries_.push_back(e);
        lookup_[key(d.note,d.velocity,d.rr,d.part)] = idx;
    }
    return !entries_.empty();
}

std::string SampleBank::status() const {
    if (!loaded()) return "BANK —";
    std::ostringstream s;
    s << "BANK READY " << entries_.size() << " entries / " << sampleRate_ << " Hz / " << bits_ << " bit";
    return s.str();
}

const SampleBank::Entry* SampleBank::find(uint8_t note, uint8_t velocity, uint8_t rr, uint8_t part) const {
    if (!loaded() || rr < 1 || rr > 3 || part > 1) return nullptr;
    int32_t idx = lookup_[key(note,velocity,rr,part)];
    if (idx < 0 || static_cast<size_t>(idx) >= entries_.size()) return nullptr;
    return &entries_[idx];
}

float SampleBank::readFrame(const Entry* e, uint32_t frame, int channel) const {
    if (!e || frame >= e->frames || channel < 0 || channel >= 2) return 0.f;
    const uint32_t bps = bits_ / 8;
    uint64_t p = e->offset + (uint64_t(frame) * 2 + uint64_t(channel)) * bps;
    if (p + bps > mappedBytes_) return 0.f;
    if (bits_ == 16) {
        int16_t v;
        std::memcpy(&v, mapped_ + p, 2);
        return float(v) / 32768.f;
    }
    const uint8_t* b = mapped_ + p;
    int32_t v = int32_t(b[0]) | (int32_t(b[1]) << 8) | (int32_t(b[2]) << 16);
    if (v & 0x800000) v |= ~0xFFFFFF;
    return float(v) / 8388608.f;
}

float SampleBank::read(const Entry* e, double frame, int channel) const {
    if (!e || frame < 0.0 || frame >= double(e->frames)) return 0.f;
    uint32_t i0 = static_cast<uint32_t>(frame);
    uint32_t i1 = std::min(i0 + 1, e->frames ? e->frames - 1 : 0);
    float frac = float(frame - double(i0));
    float a = readFrame(e, i0, channel);
    float b = readFrame(e, i1, channel);
    return a + (b-a)*frac;
}
