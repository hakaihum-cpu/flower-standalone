#pragma once
#include <JuceHeader.h>

class FramePack
{
public:
    bool load (const void* data, size_t size);
    int getFrameCount() const noexcept { return count; }
    juce::Image getFrame (int index) const;

private:
    struct Entry { std::uint64_t offset = 0; std::uint32_t size = 0; };
    const std::uint8_t* bytes = nullptr;
    size_t byteCount = 0;
    int count = 0;
    std::vector<Entry> entries;
};