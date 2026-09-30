#pragma once

namespace TwilightPoseData
{
constexpr int atlasWidth = 512;
constexpr int atlasHeight = 512;
constexpr int atlasDecodedSize = atlasWidth * atlasHeight;

static const char* const atlasChunks[] =
{
#include "TwilightAtlas_00.inc"
#include "TwilightAtlas_01.inc"
#include "TwilightAtlas_02.inc"
#include "TwilightAtlas_03.inc"
#include "TwilightAtlas_04.inc"
#include "TwilightAtlas_05.inc"
#include "TwilightAtlas_06a.inc"
#include "TwilightAtlas_06b.inc"
#include "TwilightAtlas_06c.inc"
#include "TwilightAtlas_07a.inc"
#include "TwilightAtlas_07b.inc"
};

constexpr int atlasChunkCount =
    static_cast<int> (sizeof (atlasChunks) / sizeof (atlasChunks[0]));

constexpr int backgroundWidth = 236;
constexpr int backgroundHeight = 168;
constexpr int backgroundDecodedSize =
    backgroundWidth * backgroundHeight;

static const char* const backgroundChunks[] =
{
#include "TwilightBackground_00a.inc"
#include "TwilightBackground_00b.inc"
#include "TwilightBackground_00c.inc"
#include "TwilightBackground_01a.inc"
#include "TwilightBackground_01b.inc"
};

constexpr int backgroundChunkCount =
    static_cast<int> (sizeof (backgroundChunks)
                      / sizeof (backgroundChunks[0]));
}
