// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "civ/ImageFile.h"
#include "libsiedler2/Archiv.h"
#include "libsiedler2/ArchivItem_Bitmap.h"
#include "libsiedler2/ErrorCodes.h"
#include "libsiedler2/libsiedler2.h"
#include "s25util/Log.h"
#include "s25util/StringConversion.h"
#include "s25util/strAlgos.h"
#include <boost/filesystem.hpp>
#include <boost/nowide/fstream.hpp>
#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <vector>

// stb: Only PNG, files are read/written by us (unicode paths on Windows)
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include <stb_image.h>
#include <stb_image_write.h>

namespace bfs = boost::filesystem;

namespace civ {

namespace {
    constexpr unsigned maxImageSize = 2048;

    std::vector<uint8_t> readFile(const bfs::path& path)
    {
        boost::nowide::ifstream file(path, std::ios::binary);
        if(!file)
            throw std::runtime_error("Could not open " + path.string());
        return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    libsiedler2::PixelBufferBGRA loadPng(const bfs::path& path)
    {
        const std::vector<uint8_t> data = readFile(path);
        int width = 0, height = 0, channels = 0;
        stbi_uc* pixels =
          stbi_load_from_memory(data.data(), static_cast<int>(data.size()), &width, &height, &channels, 4);
        if(!pixels)
            throw std::runtime_error("Invalid PNG " + path.string() + ": " + stbi_failure_reason());
        if(width <= 0 || height <= 0 || static_cast<unsigned>(width) > maxImageSize
           || static_cast<unsigned>(height) > maxImageSize)
        {
            stbi_image_free(pixels);
            throw std::runtime_error("Image too large: " + path.string());
        }
        libsiedler2::PixelBufferBGRA buffer(static_cast<uint16_t>(width), static_cast<uint16_t>(height));
        for(int y = 0; y < height; ++y)
        {
            for(int x = 0; x < width; ++x)
            {
                const stbi_uc* px = pixels + (static_cast<size_t>(y) * width + x) * 4u;
                // Fully transparent pixels are stored as transparent black
                if(px[3] != 0)
                    buffer.set(x, y, libsiedler2::ColorBGRA(px[2], px[1], px[0], px[3]));
            }
        }
        stbi_image_free(pixels);
        return buffer;
    }

    libsiedler2::PixelBufferBGRA loadBmp(const bfs::path& path)
    {
        libsiedler2::Archiv archive;
        if(int ec = libsiedler2::Load(path, archive))
            throw std::runtime_error("Invalid BMP " + path.string() + ": " + libsiedler2::getErrorString(ec));
        const auto* bmp = dynamic_cast<const libsiedler2::baseArchivItem_Bitmap*>(archive[0]);
        if(!bmp || bmp->getWidth() == 0 || bmp->getHeight() == 0 || bmp->getWidth() > maxImageSize
           || bmp->getHeight() > maxImageSize)
            throw std::runtime_error("Invalid BMP " + path.string());
        libsiedler2::PixelBufferBGRA buffer(bmp->getWidth(), bmp->getHeight());
        if(int ec = bmp->print(buffer))
            throw std::runtime_error("Invalid BMP " + path.string() + ": " + libsiedler2::getErrorString(ec));
        return buffer;
    }
} // namespace

std::optional<ImageFileInfo> parseImageFileName(const bfs::path& path)
{
    const std::string fileName = s25util::toLower(path.filename().string());
    std::vector<std::string> parts;
    size_t start = 0;
    while(true)
    {
        const size_t pos = fileName.find('.', start);
        parts.push_back(fileName.substr(start, pos - start));
        if(pos == std::string::npos)
            break;
        start = pos + 1;
    }
    if(parts.size() < 2 || (parts.back() != "png" && parts.back() != "bmp") || parts.front().empty())
        return std::nullopt;
    ImageFileInfo info;
    info.path = path;
    info.name = parts.front();
    for(unsigned i = 1; i + 1 < parts.size(); ++i)
    {
        const std::string& part = parts[i];
        int value;
        if(part.size() > 2 && (part.substr(0, 2) == "nx" || part.substr(0, 2) == "ny")
           && s25util::tryFromStringClassic(part.substr(2), value))
        {
            (part[1] == 'x' ? info.nx : info.ny) = value;
        } else if(part != "rle" && part != "player" && part != "shadow")
            return std::nullopt; // Unknown part, e.g. "gebaeude.alt.png": Ignore the file
        // Types of the RttR archive folder names (e.g. from s2_sprite_pipeline.py) are irrelevant
    }
    return info;
}

std::map<std::string, ImageFileInfo> listImageFiles(const bfs::path& folder)
{
    std::map<std::string, ImageFileInfo> result;
    boost::system::error_code ec;
    if(!bfs::is_directory(folder, ec))
        return result;
    std::vector<bfs::path> files;
    for(const auto& entry : bfs::directory_iterator(folder, ec))
    {
        if(bfs::is_regular_file(entry.status()))
            files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    for(const bfs::path& file : files)
    {
        std::optional<ImageFileInfo> info = parseImageFileName(file);
        if(!info)
            continue;
        const auto it = result.find(info->name);
        if(it != result.end())
        {
            LOG.write("Civilization graphics: %1% and %2% have the same name, %2% is ignored\n")
              % it->second.path.filename() % file.filename();
            continue;
        }
        result.emplace(info->name, std::move(*info));
    }
    return result;
}

libsiedler2::PixelBufferBGRA loadImageFile(const bfs::path& path)
{
    const std::string extension = s25util::toLower(path.extension().string());
    if(extension == ".png")
        return loadPng(path);
    if(extension == ".bmp")
        return loadBmp(path);
    throw std::runtime_error("Unsupported image format: " + path.string());
}

void writePngFile(const bfs::path& path, const libsiedler2::PixelBufferBGRA& buffer)
{
    const unsigned width = buffer.getWidth(), height = buffer.getHeight();
    std::vector<uint8_t> rgba(static_cast<size_t>(width) * height * 4u);
    for(unsigned y = 0; y < height; ++y)
    {
        for(unsigned x = 0; x < width; ++x)
        {
            const libsiedler2::ColorBGRA clr = buffer.get(x, y);
            uint8_t* px = &rgba[(static_cast<size_t>(y) * width + x) * 4u];
            px[0] = clr.getRed();
            px[1] = clr.getGreen();
            px[2] = clr.getBlue();
            px[3] = clr.getAlpha();
        }
    }
    std::vector<uint8_t> png;
    const auto append = [](void* context, void* data, int size) {
        auto* out = static_cast<std::vector<uint8_t>*>(context);
        const auto* bytes = static_cast<const uint8_t*>(data);
        out->insert(out->end(), bytes, bytes + size);
    };
    if(width == 0 || height == 0
       || !stbi_write_png_to_func(append, &png, static_cast<int>(width), static_cast<int>(height), 4, rgba.data(),
                                  static_cast<int>(width * 4u)))
        throw std::runtime_error("Could not encode " + path.string());
    boost::nowide::ofstream file(path, std::ios::binary);
    if(!file || !file.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size())))
        throw std::runtime_error("Could not write " + path.string());
}

} // namespace civ
