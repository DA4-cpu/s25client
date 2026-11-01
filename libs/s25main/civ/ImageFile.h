// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "libsiedler2/PixelBufferBGRA.h"
#include <boost/filesystem/path.hpp>
#include <map>
#include <optional>
#include <string>

namespace libsiedler2 {
class ArchivItem_Palette;
}

namespace civ {

/// Image file of the civilization mode graphics: "<Name>[.nx<X>.ny<Y>].png" or ".bmp".
/// nx/ny is the anchor point (origin) like in the file names of RttR archive folders (types like "rle" are ignored).
struct ImageFileInfo
{
    boost::filesystem::path path;
    /// Name without anchor and extension in lower case, e.g. "gebaeude_schatten"
    std::string name;
    std::optional<int> nx, ny;
};

/// Parse a file name. nullopt if it is no png/bmp file (anchor values must be numbers)
std::optional<ImageFileInfo> parseImageFileName(const boost::filesystem::path& path);
/// All images in the folder by their (lower case) name. Empty if the folder does not exist.
/// Several files with the same name: The first one in alphabetical order is used (a warning is logged)
std::map<std::string, ImageFileInfo> listImageFiles(const boost::filesystem::path& folder);

/// Load a PNG (any color type, transparency by alpha) or BMP (8 bit with palette: index 254 transparent, 24/32 bit)
/// Throws std::runtime_error on errors
libsiedler2::PixelBufferBGRA loadImageFile(const boost::filesystem::path& path);
/// Write the buffer as a PNG with alpha. Throws std::runtime_error on errors
void writePngFile(const boost::filesystem::path& path, const libsiedler2::PixelBufferBGRA& buffer);

} // namespace civ
