#include "FramePack.h"
#include <cstring>

namespace
{
template <typename T> bool readLE (const std::uint8_t* p, size_t remain, T& out)
{
    if (remain < sizeof (T)) return false;
    out = 0;
    for (size_t i = 0; i < sizeof (T); ++i) out |= (T) p[i] << (8 * i);
    return true;
}
}

bool FramePack::load (const void* data, size_t size)
{
    bytes = static_cast<const std::uint8_t*> (data);
    byteCount = size;
    entries.clear(); count = 0;
    if (bytes == nullptr || size < 8 || std::memcmp (bytes, "CRF1", 4) != 0) return false;
    std::uint32_t c = 0;
    if (! readLE (bytes + 4, size - 4, c) || c == 0 || c > 10000) return false;
    const size_t tableSize = 8 + (size_t) c * 12;
    if (tableSize > size) return false;
    entries.reserve (c);
    const std::uint8_t* p = bytes + 8;
    for (std::uint32_t i = 0; i < c; ++i, p += 12)
    {
        Entry e;
        if (! readLE (p, size - (size_t)(p-bytes), e.offset)) return false;
        if (! readLE (p+8, size - (size_t)(p+8-bytes), e.size)) return false;
        if (e.offset + e.size > size) return false;
        entries.push_back (e);
    }
    count = (int) c;
    return true;
}

juce::Image FramePack::getFrame (int index) const
{
    if (count <= 0 || entries.empty()) return {};
    index = ((index % count) + count) % count;
    const auto& e = entries[(size_t) index];
    return juce::ImageFileFormat::loadFrom (bytes + e.offset, e.size);
}