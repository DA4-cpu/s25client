// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "mapGenerator/Map.h"
#include <boost/filesystem/path.hpp>

struct MapGenDesc;

namespace rttr::mapGenerator {

/**
 * Writes a preview image (24 bit BMP) of the generated map for tuning the lua data. Panels from top to bottom:
 * zones, temperature, humidity, height, resources. Rows are drawn at half the width of columns like in the game.
 *
 * @returns false if the file could not be written
 */
bool WritePreviewImage(const Map& map, const MapGenDesc& desc, const boost::filesystem::path& filePath);

} // namespace rttr::mapGenerator
