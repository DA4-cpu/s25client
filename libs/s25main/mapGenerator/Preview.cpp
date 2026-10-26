// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mapGenerator/Preview.h"
#include "RttrForeachPt.h"
#include "helpers/containerUtils.h"
#include "mapGenerator/TextureHelper.h"
#include "gameData/MapGenDesc.h"
#include <boost/nowide/fstream.hpp>
#include <algorithm>
#include <array>
#include <vector>

namespace rttr::mapGenerator {

namespace {
struct Color
{
    uint8_t r, g, b;
};

Color Mix(Color a, Color b, double t)
{
    t = std::clamp(t, 0., 1.);
    const auto mix = [t](uint8_t x, uint8_t y) { return static_cast<uint8_t>(x + (y - x) * t); };
    return Color{mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b)};
}

Color FromRGB(uint32_t rgb)
{
    return Color{static_cast<uint8_t>(rgb >> 16), static_cast<uint8_t>(rgb >> 8), static_cast<uint8_t>(rgb)};
}

/// Default zone colors if none is set in lua
constexpr std::array<uint32_t, 8> defaultZoneColors{0x4060E0, 0x40C060, 0xE0E040, 0xE08030,
                                                    0xC04040, 0x40C0C0, 0xA040C0, 0x808080};

void WriteLE(std::vector<uint8_t>& data, uint32_t value, unsigned numBytes)
{
    for(unsigned i = 0; i < numBytes; ++i)
        data.push_back(static_cast<uint8_t>(value >> (8 * i)));
}
} // namespace

bool WritePreviewImage(const Map& map, const MapGenDesc& desc, const boost::filesystem::path& filePath)
{
    const MapExtent size = map.size;
    // Scale small maps up; columns are twice as wide as rows (like in the game)
    const unsigned scale = std::max(1u, 256u / std::max<unsigned>(size.x, size.y));
    const unsigned scaleX = 2 * scale;
    const unsigned scaleY = scale;
    constexpr unsigned numPanels = 5;
    constexpr unsigned gap = 4;
    const unsigned panelWidth = size.x * scaleX;
    const unsigned panelHeight = size.y * scaleY;
    const unsigned width = panelWidth;
    const unsigned height = numPanels * panelHeight + (numPanels - 1) * gap;

    std::vector<Color> pixels(static_cast<size_t>(width) * height, Color{40, 40, 40});
    const auto& textures = map.textureMap;

    const auto baseColor = [&](const MapPoint& pt) {
        if(textures.All(pt, IsWater))
            return Color{40, 70, 170};
        if(map.riverCourse[pt] || textures.Any(pt, IsWater))
            return Color{70, 110, 200};
        if(textures.Any(pt, IsSnowOrLava))
            return Color{235, 235, 245};
        if(textures.Any(pt, IsMountainOrSnowOrLava))
            return Color{140, 130, 120};
        return Color{100, 160, 70};
    };

    const auto panelColor = [&](unsigned panel, const MapPoint& pt) -> Color {
        const Color base = baseColor(pt);
        const bool isWater = textures.All(pt, IsWater);
        switch(panel)
        {
            case 0: // Zones
            {
                const unsigned zone = map.zones[pt];
                uint32_t rgb = defaultZoneColors[zone % defaultZoneColors.size()];
                if(zone < desc.zones.size() && desc.zones[zone].color != 0)
                    rgb = desc.zones[zone].color;
                const Color zoneColor = FromRGB(rgb);
                if(isWater)
                    return Mix(zoneColor, Color{0, 0, 60}, 0.6);
                if(map.riverCourse[pt] || textures.Any(pt, IsWater))
                    return Mix(zoneColor, Color{0, 0, 120}, 0.4);
                if(textures.Any(pt, IsMountainOrSnowOrLava))
                    return Mix(zoneColor, Color{255, 255, 255}, 0.45);
                return zoneColor;
            }
            case 1: // Temperature: blue (cold) -> red (hot)
            {
                const Color c =
                  Mix(Mix(Color{40, 80, 255}, Color{240, 240, 80}, map.temperature[pt] / 50.), Color{230, 40, 30},
                      (map.temperature[pt] - 50) / 50.);
                return isWater ? Mix(c, Color{0, 0, 0}, 0.5) : c;
            }
            case 2: // Humidity: yellow-brown (dry) -> dark green (wet)
            {
                const Color c = Mix(Color{220, 190, 110}, Color{20, 110, 60}, map.climate[pt] / 100.);
                return isWater ? Mix(c, Color{0, 0, 0}, 0.5) : c;
            }
            case 3: // Height
                if(isWater)
                    return Color{30, 50, 140};
                return Mix(Color{60, 60, 60}, Color{255, 255, 255}, map.heightPercent[pt] / 100.);
            default: // Resources
            {
                const uint8_t objectType = map.objectTypes[pt];
                if(objectType >= 0xC4 && objectType <= 0xC6)
                    return Color{20, 90, 20};
                if(objectType == 0xCC || objectType == 0xCD)
                    return Color{200, 200, 200};
                if(map.animals[pt] != libsiedler2::Animal::None)
                    return Color{230, 40, 40};
                const uint8_t resource = map.resources[pt];
                if(resource > libsiedler2::R_Coal && resource < libsiedler2::R_Iron)
                    return Color{20, 20, 20};
                if(resource > libsiedler2::R_Iron && resource < libsiedler2::R_Gold)
                    return Color{200, 110, 40};
                if(resource > libsiedler2::R_Gold && resource < libsiedler2::R_Granite)
                    return Color{240, 220, 40};
                if(resource > libsiedler2::R_Granite && resource < libsiedler2::R_Granite + 8)
                    return Color{150, 150, 170};
                return base;
            }
        }
    };

    for(unsigned panel = 0; panel < numPanels; ++panel)
    {
        const unsigned offsetY = panel * (panelHeight + gap);
        RTTR_FOREACH_PT(MapPoint, size)
        {
            Color color = panelColor(panel, pt);
            if(helpers::contains(map.hqPositions, pt))
                color = Color{255, 0, 0};
            // Odd rows are shifted by half a column
            const unsigned shift = (pt.y & 1) ? scaleX / 2 : 0;
            for(unsigned dy = 0; dy < scaleY; ++dy)
            {
                for(unsigned dx = 0; dx < scaleX; ++dx)
                {
                    const unsigned x = (pt.x * scaleX + dx + shift) % width;
                    const unsigned y = offsetY + pt.y * scaleY + dy;
                    pixels[static_cast<size_t>(y) * width + x] = color;
                }
            }
        }
        // Mark HQs larger
        for(const MapPoint& hq : map.hqPositions)
        {
            for(int dy = -2 * static_cast<int>(scaleY); dy <= 2 * static_cast<int>(scaleY); ++dy)
            {
                for(int dx = -2 * static_cast<int>(scaleX); dx <= 2 * static_cast<int>(scaleX); ++dx)
                {
                    const int x = static_cast<int>(hq.x * scaleX) + dx;
                    const int y = static_cast<int>(hq.y * scaleY) + dy;
                    if(x < 0 || y < 0 || x >= static_cast<int>(width) || y >= static_cast<int>(panelHeight))
                        continue;
                    pixels[static_cast<size_t>(offsetY + y) * width + x] = Color{255, 0, 0};
                }
            }
        }
    }

    // BMP: bottom-up rows, BGR, rows padded to 4 bytes
    const unsigned rowSize = (width * 3 + 3) & ~3u;
    const unsigned dataSize = rowSize * height;
    std::vector<uint8_t> data;
    data.reserve(54 + dataSize);
    data.push_back('B');
    data.push_back('M');
    WriteLE(data, 54 + dataSize, 4);
    WriteLE(data, 0, 4);
    WriteLE(data, 54, 4);
    WriteLE(data, 40, 4);
    WriteLE(data, width, 4);
    WriteLE(data, height, 4);
    WriteLE(data, 1, 2);
    WriteLE(data, 24, 2);
    WriteLE(data, 0, 4);
    WriteLE(data, dataSize, 4);
    WriteLE(data, 2835, 4);
    WriteLE(data, 2835, 4);
    WriteLE(data, 0, 4);
    WriteLE(data, 0, 4);
    for(unsigned row = 0; row < height; ++row)
    {
        const unsigned y = height - 1 - row;
        for(unsigned x = 0; x < width; ++x)
        {
            const Color& c = pixels[static_cast<size_t>(y) * width + x];
            data.push_back(c.b);
            data.push_back(c.g);
            data.push_back(c.r);
        }
        for(unsigned pad = width * 3; pad < rowSize; ++pad)
            data.push_back(0);
    }

    boost::nowide::ofstream file(filePath, std::ios::binary);
    if(!file)
        return false;
    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(file);
}

} // namespace rttr::mapGenerator
