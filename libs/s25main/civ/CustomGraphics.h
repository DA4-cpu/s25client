// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DrawPoint.h"
#include "civ/ImageFile.h"
#include "helpers/EnumArray.h"
#include "gameTypes/BuildingType.h"
#include "gameTypes/Direction.h"
#include "gameTypes/GoodTypes.h"
#include "gameTypes/JobTypes.h"
#include "gameTypes/Nation.h"
#include "libsiedler2/ColorRGB.h"
#include <boost/filesystem/path.hpp>
#include <array>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

class glArchivItem_Bitmap;
class glArchivItem_Bitmap_Player;
class glSmartBitmap;
class glTexturePacker;
class Loader;
namespace libsiedler2 {
class ArchivItem;
class ArchivItem_BitmapBase;
class ArchivItem_Palette;
} // namespace libsiedler2

namespace civ {

class Content;
struct CustomMapObject;
struct CustomOre;

/// Folder with the graphics drawn for the civilization mode: <folder>/<identifier>/<file>
constexpr auto graphicsFolder = "<RTTR_RTTR>/assets/civ";
/// Folder where the placeholder graphics are written as templates (exact names, sizes and anchor points)
constexpr auto templatesFolder = "<RTTR_USERDATA>/civ-vorlagen";

/// Work animation of a collector worker without own graphics: Images of rom_bobs like a built-in job
struct WorkAnimation
{
    unsigned firstImage, numImages;
    /// Steps of GameClient::Interpolate over the work time (image = step % number of images)
    unsigned interpolateSteps;
    /// Drawn at the figure position + offset
    DrawPoint offset;
    /// Sound (sound.lst) played when this image is shown
    unsigned sound, soundImage;
    uint8_t soundVolume;
};
WorkAnimation getLookWorkAnimation(Job look);
/// Animation of a planter (plantation) without own graphics: Sowing like the farmer for the look "Farmer", otherwise
/// planting like the forester
WorkAnimation getLookPlantAnimation(Job look);

/// German names used in the file names
const char* getDirectionName(Direction dir);
const char* getNationFolderName(Nation nation);
/// Landscape name used as file suffix ("Gruenland", "Oedland", "Winter") from the path of the map graphics
std::string getLandscapeName(const std::string& mapGfxPath);

/// Anchor point and size of the placeholder of an image
struct AnchorRef
{
    Position origin;
    Extent size;
};
/// Anchor of a bitmap which is drawn at the position + offset (nullopt for nullptr or an empty bitmap)
std::optional<AnchorRef> getAnchorRef(const libsiedler2::ArchivItem_BitmapBase* bmp,
                                      const DrawPoint& offset = DrawPoint(0, 0));
/// Anchor of bitmaps drawn on top of each other (like glSmartBitmap: the maximum of the anchors)
std::optional<AnchorRef> getAnchorRef(const std::vector<const libsiedler2::ArchivItem_BitmapBase*>& bmps);
/// Anchor point (origin) of an image of the given size: From the file name or relative to the size like the
/// placeholder (same relative position). Without placeholder: Center of the bottom line
Position calcAnchor(const ImageFileInfo& info, const Extent& size, const std::optional<AnchorRef>& placeholder);
/// Bitmap with player colors: Pixels with exactly one of the 4 player colors (S2 palette 128-131, blue) are drawn in
/// the color of the player
std::unique_ptr<glArchivItem_Bitmap_Player>
createPlayerBitmap(const libsiedler2::PixelBufferBGRA& buffer, const Position& anchor,
                   const std::array<libsiedler2::ColorRGB, 4>& playerColors);
std::unique_ptr<glArchivItem_Bitmap> createBitmap(const libsiedler2::PixelBufferBGRA& buffer, const Position& anchor);

/// Graphics of the wares, workshops (building and worker) and map objects defined in lua (civilization mode) drawn by
/// the player. Each image which exists replaces the placeholder graphics of the "look" type.
/// File names: see data/RTTR/assets/civ/LIESMICH.txt
class CustomGraphics
{
public:
    struct Settings
    {
        /// Folder with one folder per identifier
        boost::filesystem::path folder;
        /// Write templates there if not empty
        boost::filesystem::path templateFolder;
        /// Suffix of landscape variants ("Winter", ...)
        std::string landscape;
        bool isWinter = false;
    };

    CustomGraphics();
    ~CustomGraphics();

    /// Load the graphics of the content and fill the caches of the loader for the custom slots.
    /// Must be called after the built-in caches are filled (Loader::fillCaches). stp may be nullptr
    void fillCaches(Loader& loader, glTexturePacker* stp, const Content& content, const Settings& settings);
    /// Remove all graphics and reset the custom slots of the caches of the loader
    void clear(Loader& loader);

    /// Own graphics or nullptr (placeholder)
    glArchivItem_Bitmap* GetWareIcon(GoodType good) const;
    glArchivItem_Bitmap* GetWareFlag(GoodType good) const;
    glArchivItem_Bitmap* GetWareDonkey(GoodType good) const;
    glArchivItem_Bitmap* GetJobIcon(Job job) const;
    glArchivItem_Bitmap* GetBuildingIcon(Nation nation, BuildingType bld) const;
    glArchivItem_Bitmap* GetBuildingImage(Nation nation, BuildingType bld) const;
    /// True if the worker has own walking graphics (Loader::bob_jobs_cache)
    bool HasWalking(Job job) const;
    /// Own work animation (empty: placeholder)
    const std::vector<glArchivItem_Bitmap_Player*>& GetWorkFrames(Job job) const;
    /// Own planting animation of a plantation (empty: placeholder)
    const std::vector<glArchivItem_Bitmap_Player*>& GetPlantFrames(Job job) const;
    /// Own sprite of the map object in the state (size) or nullptr
    glSmartBitmap* GetMapObject(unsigned objIdx, unsigned state) const;
    /// Own sign of the geologist for the ore (index) and amount (0..2) or nullptr
    glArchivItem_Bitmap* GetOreSign(unsigned oreIdx, unsigned amountIdx) const;

    /// Number of images loaded (for tests and the log)
    unsigned GetNumLoaded() const { return numLoaded_; }

private:
    struct Folder;
    struct WareGfx
    {
        glArchivItem_Bitmap *icon = nullptr, *flag = nullptr, *donkey = nullptr;
    };
    struct BuildingGfx
    {
        helpers::EnumArray<glArchivItem_Bitmap*, Nation> icon{}, image{};
        glArchivItem_Bitmap* jobIcon = nullptr;
        bool hasWalking = false;
        std::vector<glArchivItem_Bitmap_Player*> workFrames, plantFrames;
    };

    void loadWare(Loader& loader, glTexturePacker* stp, GoodType good, const std::string& name, GoodType look);
    void loadBuilding(Loader& loader, glTexturePacker* stp, BuildingType bld, const std::string& name,
                      BuildingType look, Job lookJob, bool plants);
    void loadMapObject(Loader& loader, glTexturePacker* stp, unsigned objIdx, const CustomMapObject& obj);
    void loadOre(Loader& loader, unsigned oreIdx, const CustomOre& ore);
    /// Own animation frames <name>_0, _1, ... as long as they exist and templates of the placeholder animation
    std::vector<glArchivItem_Bitmap_Player*> loadAnimation(Loader& loader, const Folder& folder,
                                                           const std::string& name, const WorkAnimation& lookAnim);

    /// Load the image (with the landscape variant) or nullptr if it does not exist or is invalid
    glArchivItem_Bitmap* loadImage(const Folder& folder, const std::string& name,
                                   const std::optional<AnchorRef>& placeholder);
    glArchivItem_Bitmap_Player* loadPlayerImage(const Folder& folder, const std::string& name,
                                                const std::optional<AnchorRef>& placeholder);
    const ImageFileInfo* findImage(const Folder& folder, const std::string& name) const;
    /// Write the placeholder as template if the folder has no image with this name
    void writeTemplate(const Folder& folder, const std::string& name, const libsiedler2::ArchivItem_BitmapBase* bmp,
                       bool isShadow = false, const DrawPoint& offset = DrawPoint(0, 0), unsigned tint = 0xFFFFFF);
    void writeTemplate(const Folder& folder, const std::string& name,
                       const std::vector<const libsiedler2::ArchivItem_BitmapBase*>& bmps);

    Settings settings_;
    const libsiedler2::ArchivItem_Palette* pal5_ = nullptr;
    std::array<libsiedler2::ColorRGB, 4> playerColors_{};
    std::vector<std::unique_ptr<libsiedler2::ArchivItem>> bitmaps_;
    std::vector<std::unique_ptr<glSmartBitmap>> sprites_;
    std::array<WareGfx, NUM_CUSTOM_GOODS> wares_;
    std::array<BuildingGfx, NUM_CUSTOM_BUILDINGS> buildings_;
    /// Map object -> state -> sprite (nullptr: placeholder)
    std::vector<std::vector<glSmartBitmap*>> mapObjects_;
    /// Ore -> amount index -> sign (nullptr: placeholder)
    std::vector<std::array<glArchivItem_Bitmap*, 3>> oreSigns_;
    unsigned numLoaded_ = 0, numTemplates_ = 0;
};

} // namespace civ
