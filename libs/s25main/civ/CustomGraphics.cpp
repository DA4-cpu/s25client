// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "civ/CustomGraphics.h"
#include "LeatherLoader.h"
#include "Loader.h"
#include "WineLoader.h"
#include "civ/Content.h"
#include "civ/Graphics.h"
#include "enum_cast.hpp"
#include "helpers/EnumRange.h"
#include "helpers/containerUtils.h"
#include "ogl/glArchivItem_Bitmap.h"
#include "ogl/glArchivItem_Bitmap_Player.h"
#include "ogl/glArchivItem_Bitmap_Raw.h"
#include "ogl/glArchivItem_Bob.h"
#include "ogl/glSmartBitmap.h"
#include "ogl/glTexturePacker.h"
#include "gameTypes/DirectionToImgDir.h"
#include "gameData/JobConsts.h"
#include "libsiedler2/ArchivItem_Bitmap.h"
#include "libsiedler2/ArchivItem_Bitmap_Player.h"
#include "libsiedler2/ArchivItem_Palette.h"
#include "s25util/Log.h"
#include "s25util/strAlgos.h"
#include <boost/filesystem.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace bfs = boost::filesystem;
using libsiedler2::ArchivItem_BitmapBase;
using libsiedler2::ColorBGRA;
using libsiedler2::PixelBufferBGRA;

namespace civ {

namespace {
    /// Number of images of a walking animation per direction
    constexpr unsigned numWalkFrames = 8;
    /// Index of the walking shadows in the map graphics
    constexpr unsigned walkShadowStart = 900;
    /// Alpha of the shadow pixels of templates
    constexpr uint8_t templateShadowAlpha = 128;

    /// Player bitmap whose player color pixels can be set from outside
    class CustomPlayerBitmap : public glArchivItem_Bitmap_Player
    {
    public:
        void setPlayerColorIdx(uint16_t x, uint16_t y, uint8_t idx) { tex_pdata.set(x, y, idx); }
    };

    /// Pixels and anchor of one or more bitmaps drawn on top of each other
    struct Sprite
    {
        PixelBufferBGRA buffer;
        Position origin;
    };

    void printBitmap(const ArchivItem_BitmapBase& bmp, PixelBufferBGRA& buffer, const Position& pos,
                     const libsiedler2::ArchivItem_Palette* pal5)
    {
        if(bmp.getWidth() == 0 || bmp.getHeight() == 0)
            return;
        PixelBufferBGRA tmp(bmp.getWidth(), bmp.getHeight());
        if(const auto* plBmp = dynamic_cast<const libsiedler2::ArchivItem_Bitmap_Player*>(&bmp))
            plBmp->print(tmp, pal5, 128); // Player colors in the blue of the S2 palette
        else if(const auto* baseBmp = dynamic_cast<const libsiedler2::baseArchivItem_Bitmap*>(&bmp))
            baseBmp->print(tmp, pal5);
        // Copy only the visible pixels so that later bitmaps are drawn over earlier ones
        for(unsigned y = 0; y < tmp.getHeight(); ++y)
        {
            for(unsigned x = 0; x < tmp.getWidth(); ++x)
            {
                const ColorBGRA clr = tmp.get(x, y);
                const int dx = pos.x + static_cast<int>(x), dy = pos.y + static_cast<int>(y);
                if(clr.getAlpha() != 0 && dx >= 0 && dy >= 0 && dx < buffer.getWidth() && dy < buffer.getHeight())
                    buffer.set(dx, dy, clr);
            }
        }
    }

    /// Like glSmartBitmap: The anchor of the result is the maximum of the anchors
    Sprite composeSprite(const std::vector<const ArchivItem_BitmapBase*>& bmps,
                         const libsiedler2::ArchivItem_Palette* pal5)
    {
        Position origin(std::numeric_limits<int>::min(), std::numeric_limits<int>::min());
        Position maxPos = origin;
        bool any = false;
        for(const ArchivItem_BitmapBase* bmp : bmps)
        {
            if(!bmp || bmp->getWidth() == 0)
                continue;
            any = true;
            origin = elMax(origin, Position(bmp->getNx(), bmp->getNy()));
            maxPos = elMax(maxPos, Position(bmp->getWidth() - bmp->getNx(), bmp->getHeight() - bmp->getNy()));
        }
        if(!any)
            return Sprite{PixelBufferBGRA(), Position(0, 0)};
        const Extent size(origin + maxPos);
        Sprite result{PixelBufferBGRA(static_cast<uint16_t>(size.x), static_cast<uint16_t>(size.y)), origin};
        for(const ArchivItem_BitmapBase* bmp : bmps)
        {
            if(bmp && bmp->getWidth() != 0)
                printBitmap(*bmp, result.buffer, origin - Position(bmp->getNx(), bmp->getNy()), pal5);
        }
        return result;
    }

    const ArchivItem_BitmapBase* asBitmap(const ITexture* tex)
    {
        return dynamic_cast<const ArchivItem_BitmapBase*>(tex);
    }

    /// Ware whose carrier overlay (carrier.bob) is used as placeholder for the anchors and templates
    unsigned getCarrierOverlayId(GoodType look)
    {
        if(wineaddon::isWineAddonGoodType(look) || leatheraddon::isLeatherAddonGoodType(look))
            look = GoodType::Boards; // Their carriers are complete sprites
        if(look == GoodType::ShieldJapanese)
            look = GoodType::ShieldRomans;
        return rttr::enum_cast(look);
    }

    std::string withNumber(const std::string& name, unsigned nr) { return name + "_" + std::to_string(nr); }
} // namespace

WorkAnimation getLookWorkAnimation(Job look)
{
    switch(look)
    {
        case Job::Planer: return WorkAnimation{253, 20, 80, DrawPoint(0, 0), 76, 5, 200};
        case Job::Woodcutter: return WorkAnimation{24, 8, 64, DrawPoint(-9, 0), 53, 3, 255};
        default: return WorkAnimation{40, 8, 64, DrawPoint(0, 0), 56, 5, 255}; // Stonemason
    }
}

const char* getDirectionName(Direction dir)
{
    switch(dir)
    {
        case Direction::West: return "West";
        case Direction::NorthWest: return "Nordwest";
        case Direction::NorthEast: return "Nordost";
        case Direction::East: return "Ost";
        case Direction::SouthEast: return "Suedost";
        case Direction::SouthWest: return "Suedwest";
    }
    return "";
}

const char* getNationFolderName(Nation nation)
{
    switch(nation)
    {
        case Nation::Africans: return "Afrikaner";
        case Nation::Japanese: return "Japaner";
        case Nation::Romans: return "Roemer";
        case Nation::Vikings: return "Wikinger";
        case Nation::Babylonians: return "Babylonier";
    }
    return "";
}

std::string getLandscapeName(const std::string& mapGfxPath)
{
    const std::string stem = s25util::toLower(bfs::path(mapGfxPath).stem().string());
    if(stem == "map_0_z")
        return "Gruenland";
    if(stem == "map_1_z")
        return "Oedland";
    if(stem == "map_2_z")
        return "Winter";
    return bfs::path(mapGfxPath).stem().string();
}

std::optional<AnchorRef> getAnchorRef(const ArchivItem_BitmapBase* bmp, const DrawPoint& offset)
{
    if(!bmp || bmp->getWidth() == 0 || bmp->getHeight() == 0)
        return std::nullopt;
    return AnchorRef{Position(bmp->getNx(), bmp->getNy()) - offset, Extent(bmp->getWidth(), bmp->getHeight())};
}

std::optional<AnchorRef> getAnchorRef(const std::vector<const ArchivItem_BitmapBase*>& bmps)
{
    std::optional<AnchorRef> result;
    Position maxPos;
    for(const ArchivItem_BitmapBase* bmp : bmps)
    {
        const std::optional<AnchorRef> ref = getAnchorRef(bmp);
        if(!ref)
            continue;
        const Position end = Position(ref->size) - ref->origin;
        if(!result)
        {
            result = ref;
            maxPos = end;
        } else
        {
            result->origin = elMax(result->origin, ref->origin);
            maxPos = elMax(maxPos, end);
        }
    }
    if(result)
        result->size = Extent(result->origin + maxPos);
    return result;
}

Position calcAnchor(const ImageFileInfo& info, const Extent& size, const std::optional<AnchorRef>& placeholder)
{
    Position result(size.x / 2, size.y > 0 ? size.y - 1 : 0);
    if(placeholder && placeholder->size.x > 0 && placeholder->size.y > 0)
    {
        // Same relative position as in the placeholder
        result.x = static_cast<int>(std::lround(double(placeholder->origin.x) * size.x / placeholder->size.x));
        result.y = static_cast<int>(std::lround(double(placeholder->origin.y) * size.y / placeholder->size.y));
    }
    if(info.nx)
        result.x = *info.nx;
    if(info.ny)
        result.y = *info.ny;
    return result;
}

std::unique_ptr<glArchivItem_Bitmap_Player> createPlayerBitmap(const PixelBufferBGRA& buffer, const Position& anchor,
                                                               const std::array<libsiedler2::ColorRGB, 4>& playerColors)
{
    auto bmp = std::make_unique<CustomPlayerBitmap>();
    bmp->init(buffer.getWidth(), buffer.getHeight(), libsiedler2::TextureFormat::BGRA);
    for(uint16_t y = 0; y < buffer.getHeight(); ++y)
    {
        for(uint16_t x = 0; x < buffer.getWidth(); ++x)
        {
            const ColorBGRA clr = buffer.get(x, y);
            if(clr.getAlpha() == 0)
                continue;
            bmp->setPixel(x, y, clr);
            const libsiedler2::ColorRGB rgb(clr.getRed(), clr.getGreen(), clr.getBlue());
            for(uint8_t i = 0; i < playerColors.size(); ++i)
            {
                if(rgb == playerColors[i])
                {
                    bmp->setPlayerColorIdx(x, y, i);
                    break;
                }
            }
        }
    }
    bmp->setNx(static_cast<int16_t>(anchor.x));
    bmp->setNy(static_cast<int16_t>(anchor.y));
    return bmp;
}

std::unique_ptr<glArchivItem_Bitmap> createBitmap(const PixelBufferBGRA& buffer, const Position& anchor)
{
    auto bmp = std::make_unique<glArchivItem_Bitmap_Raw>();
    if(int ec = bmp->create(buffer.getWidth(), buffer.getHeight(), buffer.getPixelPtr(), buffer.getWidth(),
                            buffer.getHeight(), libsiedler2::TextureFormat::BGRA, nullptr))
        throw std::runtime_error("Could not create bitmap: " + std::to_string(ec));
    bmp->setNx(static_cast<int16_t>(anchor.x));
    bmp->setNy(static_cast<int16_t>(anchor.y));
    return bmp;
}

/// Image folder of an identifier (or of a nation of a building) and its folder for the templates
struct CustomGraphics::Folder
{
    bfs::path path;
    std::map<std::string, ImageFileInfo> files;
    bfs::path templatePath;
    /// Names (lower case) of the existing templates
    std::set<std::string> templateNames;
    /// Images not found here are searched there (e.g. common images of all nations)
    const Folder* parent = nullptr;

    Folder(const bfs::path& path, const bfs::path& templatePath, const Folder* parent = nullptr)
        : path(path), files(listImageFiles(path)), templatePath(templatePath), parent(parent)
    {
        if(!templatePath.empty())
        {
            for(const auto& entry : listImageFiles(templatePath))
                templateNames.insert(entry.first);
        }
    }
};

CustomGraphics::CustomGraphics() = default;
CustomGraphics::~CustomGraphics() = default;

void CustomGraphics::clear(Loader& loader)
{
    // The caches point to our bitmaps -> reset them first
    for(const auto nation : helpers::enumRange<Nation>())
    {
        for(unsigned i = 0; i < NUM_CUSTOM_BUILDINGS; ++i)
        {
            Loader::BuildingSprites& sprites = loader.building_cache[nation][getCustomBuilding(i)];
            sprites.building.reset();
            sprites.skeleton.reset();
            sprites.door.reset();
        }
        for(unsigned i = 0; i < NUM_CUSTOM_JOBS; ++i)
        {
            for(const auto dir : helpers::enumRange<Direction>())
            {
                for(glSmartBitmap& bmp : loader.bob_jobs_cache[nation][getCustomJob(i)][dir])
                    bmp.reset();
            }
        }
    }
    for(auto& carrierSprites : loader.carrier_cache)
    {
        for(unsigned i = 0; i < NUM_CUSTOM_GOODS; ++i)
        {
            for(const auto dir : helpers::enumRange<Direction>())
            {
                for(glSmartBitmap& bmp : carrierSprites[getCustomGood(i)][dir])
                    bmp.reset();
            }
        }
    }
    wares_ = {};
    buildings_ = {};
    mapObjects_.clear();
    sprites_.clear();
    bitmaps_.clear();
    numLoaded_ = numTemplates_ = 0;
}

void CustomGraphics::fillCaches(Loader& loader, glTexturePacker* stp, const Content& content, const Settings& settings)
{
    clear(loader);
    settings_ = settings;
    pal5_ = loader.GetPaletteN("pal5");
    if(!pal5_)
        return;
    for(uint8_t i = 0; i < playerColors_.size(); ++i)
        playerColors_[i] = pal5_->get(128 + i);

    for(unsigned i = 0; i < content.GetNumWares(); ++i)
    {
        const GoodType good = getCustomGood(i);
        const CustomWare& ware = *content.GetWare(good);
        loadWare(loader, stp, good, ware.name, ware.look);
    }
    for(unsigned i = 0; i < content.GetNumBuildings(); ++i)
    {
        const BuildingType bld = getCustomBuilding(i);
        const CustomBuilding& building = *content.GetBuilding(bld);
        const CustomWorker* worker = content.GetWorker(getCustomJob(i));
        loadBuilding(loader, stp, bld, building.name, building.look, worker ? worker->look : Job::Helper);
    }
    mapObjects_.resize(content.GetMapObjects().size());
    for(unsigned i = 0; i < content.GetMapObjects().size(); ++i)
        loadMapObject(loader, stp, i, content.GetMapObjects()[i]);

    if(content.GetNumWares() > 0 || content.GetNumBuildings() > 0 || !content.GetMapObjects().empty())
    {
        LOG.write("Civilization mode: %1% own images loaded from %2%, %3% new templates written to %4%\n") % numLoaded_
          % settings_.folder % numTemplates_ % settings_.templateFolder;
    }
}

const ImageFileInfo* CustomGraphics::findImage(const Folder& folder, const std::string& name) const
{
    const std::string lowerName = s25util::toLower(name);
    std::vector<std::string> candidates;
    if(!settings_.landscape.empty())
        candidates.push_back(lowerName + "_" + s25util::toLower(settings_.landscape));
    candidates.push_back(lowerName);
    for(const Folder* curFolder = &folder; curFolder; curFolder = curFolder->parent)
    {
        for(const std::string& candidate : candidates)
        {
            const auto it = curFolder->files.find(candidate);
            if(it != curFolder->files.end())
                return &it->second;
        }
    }
    return nullptr;
}

glArchivItem_Bitmap* CustomGraphics::loadImage(const Folder& folder, const std::string& name,
                                               const std::optional<AnchorRef>& placeholder)
{
    const ImageFileInfo* info = findImage(folder, name);
    if(!info)
        return nullptr;
    try
    {
        const PixelBufferBGRA buffer = loadImageFile(info->path);
        auto bmp = createBitmap(buffer, calcAnchor(*info, Extent(buffer.getWidth(), buffer.getHeight()), placeholder));
        glArchivItem_Bitmap* result = bmp.get();
        bitmaps_.push_back(std::move(bmp));
        ++numLoaded_;
        return result;
    } catch(const std::exception& e)
    {
        LOG.write("Civilization mode: Could not load %1%: %2%\n") % info->path % e.what();
        return nullptr;
    }
}

glArchivItem_Bitmap_Player* CustomGraphics::loadPlayerImage(const Folder& folder, const std::string& name,
                                                            const std::optional<AnchorRef>& placeholder)
{
    const ImageFileInfo* info = findImage(folder, name);
    if(!info)
        return nullptr;
    try
    {
        const PixelBufferBGRA buffer = loadImageFile(info->path);
        auto bmp = createPlayerBitmap(
          buffer, calcAnchor(*info, Extent(buffer.getWidth(), buffer.getHeight()), placeholder), playerColors_);
        glArchivItem_Bitmap_Player* result = bmp.get();
        bitmaps_.push_back(std::move(bmp));
        ++numLoaded_;
        return result;
    } catch(const std::exception& e)
    {
        LOG.write("Civilization mode: Could not load %1%: %2%\n") % info->path % e.what();
        return nullptr;
    }
}

void CustomGraphics::writeTemplate(const Folder& folder, const std::string& name,
                                   const std::vector<const ArchivItem_BitmapBase*>& bmps)
{
    if(folder.templatePath.empty() || helpers::contains(folder.templateNames, s25util::toLower(name)))
        return;
    const Sprite sprite = composeSprite(bmps, pal5_);
    if(sprite.buffer.getWidth() == 0)
        return;
    try
    {
        bfs::create_directories(folder.templatePath);
        const std::string fileName =
          name + ".nx" + std::to_string(sprite.origin.x) + ".ny" + std::to_string(sprite.origin.y) + ".png";
        writePngFile(folder.templatePath / fileName, sprite.buffer);
        ++numTemplates_;
    } catch(const std::exception& e)
    {
        LOG.write("Civilization mode: Could not write the template %1%: %2%\n") % name % e.what();
    }
}

void CustomGraphics::writeTemplate(const Folder& folder, const std::string& name, const ArchivItem_BitmapBase* bmp,
                                   bool isShadow, const DrawPoint& offset, unsigned tint)
{
    if(!bmp || folder.templatePath.empty() || helpers::contains(folder.templateNames, s25util::toLower(name)))
        return;
    Sprite sprite = composeSprite({bmp}, pal5_);
    if(sprite.buffer.getWidth() == 0)
        return;
    // Drawn at the position + offset -> anchor relative to the position
    sprite.origin -= offset;
    for(ColorBGRA& clr : sprite.buffer)
    {
        if(clr.getAlpha() == 0)
            continue;
        if(isShadow)
            clr = ColorBGRA(0, 0, 0, templateShadowAlpha);
        else if(tint != 0xFFFFFF)
        {
            const auto mul = [](uint8_t value, unsigned factor) {
                return static_cast<uint8_t>((value * (factor & 0xFFu) + 127u) / 255u);
            };
            clr = ColorBGRA(mul(clr.getBlue(), tint), mul(clr.getGreen(), tint >> 8), mul(clr.getRed(), tint >> 16),
                            clr.getAlpha());
        }
    }
    try
    {
        bfs::create_directories(folder.templatePath);
        const std::string fileName =
          name + ".nx" + std::to_string(sprite.origin.x) + ".ny" + std::to_string(sprite.origin.y) + ".png";
        writePngFile(folder.templatePath / fileName, sprite.buffer);
        ++numTemplates_;
    } catch(const std::exception& e)
    {
        LOG.write("Civilization mode: Could not write the template %1%: %2%\n") % name % e.what();
    }
}

void CustomGraphics::loadWare(Loader& loader, glTexturePacker* stp, GoodType good, const std::string& name,
                              GoodType look)
{
    const Folder folder(settings_.folder / name,
                        settings_.templateFolder.empty() ? bfs::path() : settings_.templateFolder / name);
    WareGfx& gfx = wares_[getCustomGoodIdx(good)];

    const ArchivItem_BitmapBase* lookIcon = asBitmap(loader.GetWareTex(look));
    const ArchivItem_BitmapBase* lookFlag = asBitmap(loader.GetWareStackTex(look));
    const ArchivItem_BitmapBase* lookDonkey = asBitmap(loader.GetWareDonkeyTex(look));
    gfx.icon = loadImage(folder, "Symbol", getAnchorRef(lookIcon));
    gfx.flag = loadImage(folder, "Fahne", getAnchorRef(lookFlag));
    gfx.donkey = loadImage(folder, "Esel", getAnchorRef(lookDonkey));
    writeTemplate(folder, "Symbol", lookIcon);
    writeTemplate(folder, "Fahne", lookFlag);
    writeTemplate(folder, "Esel", lookDonkey);

    // Carried ware: Overlay over the body of the carrier, per direction (and optionally per image)
    glArchivItem_Bob* bobCarrier = loader.GetBob("carrier");
    if(!bobCarrier)
        return;
    const unsigned overlayId = getCarrierOverlayId(look);
    // Overlays: [fat][direction][image]
    std::array<helpers::EnumArray<std::array<glArchivItem_Bitmap_Player*, numWalkFrames>, Direction>, 2> overlays{};
    bool complete = true, anyOwn = false;
    for(const bool fat : {false, true})
    {
        const std::string baseName = fat ? "TraegerDick" : "Traeger";
        for(const auto dir : helpers::enumRange<Direction>())
        {
            const libsiedler2::ImgDir imgDir = toImgDir(dir);
            const std::string dirName = baseName + "_" + getDirectionName(dir);
            // Template: One image per direction if the original uses the same image for all steps of the animation
            bool sameForAllFrames = true;
            for(unsigned frame = 1; frame < numWalkFrames; ++frame)
            {
                if(bobCarrier->getOverlayIdx(overlayId, fat, imgDir, frame)
                   != bobCarrier->getOverlayIdx(overlayId, fat, imgDir, 0))
                    sameForAllFrames = false;
            }
            const ArchivItem_BitmapBase* lookOverlay0 = bobCarrier->getOverlay(overlayId, fat, imgDir, 0);
            glArchivItem_Bitmap_Player* perDirection = loadPlayerImage(folder, dirName, getAnchorRef(lookOverlay0));
            if(sameForAllFrames)
                writeTemplate(folder, dirName, lookOverlay0);
            for(unsigned frame = 0; frame < numWalkFrames; ++frame)
            {
                const ArchivItem_BitmapBase* lookOverlay = bobCarrier->getOverlay(overlayId, fat, imgDir, frame);
                const std::string frameName = withNumber(dirName, frame);
                glArchivItem_Bitmap_Player* perFrame = loadPlayerImage(folder, frameName, getAnchorRef(lookOverlay));
                if(!sameForAllFrames)
                    writeTemplate(folder, frameName, lookOverlay);
                overlays[fat][dir][frame] = perFrame ? perFrame : perDirection;
                if(overlays[fat][dir][frame])
                    anyOwn = true;
                else if(!fat)
                    complete = false;
            }
        }
    }
    if(!anyOwn)
        return;
    if(!complete)
    {
        LOG.write("Civilization mode: %1%: Images of the carrier are missing for some directions, the placeholder "
                  "is used\n")
          % name;
        return;
    }
    for(const bool fat : {false, true})
    {
        for(const auto dir : helpers::enumRange<Direction>())
        {
            const libsiedler2::ImgDir imgDir = toImgDir(dir);
            for(unsigned frame = 0; frame < numWalkFrames; ++frame)
            {
                glSmartBitmap& bmp = loader.carrier_cache[fat][good][dir][frame];
                bmp.reset();
                bmp.add(bobCarrier->getBody(fat, imgDir, frame));
                // Fat carriers use the images of the thin ones if they have no own images
                glArchivItem_Bitmap_Player* overlay = overlays[fat][dir][frame];
                bmp.add(
                  static_cast<libsiedler2::ArchivItem_Bitmap_Player*>(overlay ? overlay : overlays[false][dir][frame]));
                bmp.addShadow(loader.GetMapImage(walkShadowStart + static_cast<unsigned>(imgDir) * 8 + frame));
                if(stp)
                    stp->add(bmp);
            }
        }
    }
}

void CustomGraphics::loadBuilding(Loader& loader, glTexturePacker* stp, BuildingType bld, const std::string& name,
                                  BuildingType look, Job lookJob)
{
    const bfs::path templatePath = settings_.templateFolder.empty() ? bfs::path() : settings_.templateFolder / name;
    const Folder folder(settings_.folder / name, templatePath);
    BuildingGfx& gfx = buildings_[getCustomBuildingIdx(bld)];

    // Building, construction site and icon per nation
    static constexpr std::array<const char*, 5> partNames = {"Gebaeude", "Gebaeude_Schatten", "Baustelle",
                                                             "Baustelle_Schatten", "Tuer"};
    const std::string winterSuffix = settings_.isWinter ? "_Winter" : "";
    for(const auto nation : helpers::enumRange<Nation>())
    {
        if(!loader.HasNationGraphics(nation))
            continue;
        const char* nationName = getNationFolderName(nation);
        const Folder nationFolder(folder.path / nationName,
                                  templatePath.empty() ? bfs::path() : templatePath / nationName, &folder);
        // Placeholder images (same order as partNames)
        std::array<glArchivItem_Bitmap*, 5> lookImages{};
        if(look == BuildingType::Charburner)
        {
            const unsigned id = rttr::enum_cast(nation) * 8;
            lookImages = {loader.GetImageN("charburner", id + (settings_.isWinter ? 6 : 1)),
                          loader.GetImageN("charburner", id + 2), loader.GetImageN("charburner", id + 3),
                          loader.GetImageN("charburner", id + 4),
                          loader.GetImageN("charburner", id + (settings_.isWinter ? 7 : 5))};
        } else
        {
            for(unsigned part = 0; part < lookImages.size(); ++part)
                lookImages[part] = dynamic_cast<glArchivItem_Bitmap*>(
                  loader.GetNationImageN(nation, 250 + 5 * rttr::enum_cast(look) + part));
        }
        std::array<glArchivItem_Bitmap*, 5> images{};
        bool anyOwn = false;
        for(unsigned part = 0; part < images.size(); ++part)
        {
            images[part] = loadImage(nationFolder, partNames[part], getAnchorRef(lookImages[part]));
            anyOwn |= images[part] != nullptr;
            writeTemplate(nationFolder, partNames[part] + winterSuffix, lookImages[part], part % 2 == 1);
        }
        glArchivItem_Bitmap* lookIcon = loader.GetNationIcon(nation, look);
        gfx.icon[nation] = loadImage(nationFolder, "Icon", getAnchorRef(lookIcon));
        writeTemplate(nationFolder, "Icon", lookIcon);
        if(!anyOwn)
            continue;
        // Missing parts: Placeholder, but no shadow/door if the image itself is an own one (it would not fit)
        const auto pick = [&](unsigned part, unsigned mainPart) -> glArchivItem_Bitmap* {
            if(images[part])
                return images[part];
            return images[mainPart] ? nullptr : lookImages[part];
        };
        gfx.image[nation] = pick(0, 0);
        Loader::BuildingSprites& sprites = loader.building_cache[nation][bld];
        sprites.building.reset();
        sprites.skeleton.reset();
        sprites.door.reset();
        sprites.building.add(pick(0, 0));
        sprites.building.addShadow(pick(1, 0));
        sprites.skeleton.add(pick(2, 2));
        sprites.skeleton.addShadow(pick(3, 2));
        sprites.door.add(pick(4, 0));
        if(stp)
        {
            stp->add(sprites.building);
            stp->add(sprites.skeleton);
            if(!sprites.door.empty())
                stp->add(sprites.door);
        }
    }

    // Worker: Symbol, walking (complete figure per direction and image) and work animation
    const Job job = getCustomJob(getCustomBuildingIdx(bld));
    const ArchivItem_BitmapBase* lookJobIcon = asBitmap(loader.GetJobTex(lookJob));
    gfx.jobIcon = loadImage(folder, "Arbeiter_Symbol", getAnchorRef(lookJobIcon));
    writeTemplate(folder, "Arbeiter_Symbol", lookJobIcon);

    glArchivItem_Bob* bobJobs = loader.GetBob("jobs");
    if(bobJobs)
    {
        const auto& spriteData = JOB_SPRITE_CONSTS[lookJob];
        helpers::EnumArray<std::array<glArchivItem_Bitmap_Player*, numWalkFrames>, Direction> walking{};
        helpers::EnumArray<bool, Direction> ownDirection{};
        for(const auto dir : helpers::enumRange<Direction>())
        {
            const libsiedler2::ImgDir imgDir = toImgDir(dir);
            unsigned numOwn = 0;
            for(unsigned frame = 0; frame < numWalkFrames; ++frame)
            {
                const std::vector<const ArchivItem_BitmapBase*> lookParts = {
                  bobJobs->getBody(spriteData.isFat(), imgDir, frame),
                  bobJobs->getOverlay(spriteData.getBobId(Nation::Romans), spriteData.isFat(), imgDir, frame)};
                const std::string frameName = withNumber(std::string("Arbeiter_") + getDirectionName(dir), frame);
                walking[dir][frame] = loadPlayerImage(folder, frameName, getAnchorRef(lookParts));
                if(walking[dir][frame])
                    ++numOwn;
                writeTemplate(folder, frameName, lookParts);
            }
            ownDirection[dir] = numOwn == numWalkFrames;
            if(numOwn > 0 && numOwn < numWalkFrames)
                LOG.write("Civilization mode: %1%: Not all %2% images of the worker for the direction %3% exist, "
                          "the placeholder is used\n")
                  % name % numWalkFrames % getDirectionName(dir);
        }
        gfx.hasWalking = helpers::contains(ownDirection, true);
        if(gfx.hasWalking)
        {
            for(const auto nation : helpers::enumRange<Nation>())
            {
                if(!loader.HasNationGraphics(nation))
                    continue;
                for(const auto dir : helpers::enumRange<Direction>())
                {
                    const libsiedler2::ImgDir imgDir = toImgDir(dir);
                    for(unsigned frame = 0; frame < numWalkFrames; ++frame)
                    {
                        glSmartBitmap& bmp = loader.bob_jobs_cache[nation][job][dir][frame];
                        bmp.reset();
                        if(ownDirection[dir])
                            bmp.add(static_cast<libsiedler2::ArchivItem_Bitmap_Player*>(walking[dir][frame]));
                        else
                        {
                            bmp.add(bobJobs->getBody(spriteData.isFat(), imgDir, frame));
                            bmp.add(
                              bobJobs->getOverlay(spriteData.getBobId(nation), spriteData.isFat(), imgDir, frame));
                        }
                        bmp.addShadow(loader.GetMapImage(walkShadowStart + static_cast<unsigned>(imgDir) * 8 + frame));
                        if(stp)
                            stp->add(bmp);
                    }
                }
            }
        }
    }

    // Work animation (collectors): Templates of all images of the placeholder, own images 0, 1, 2, ... as long as
    // they exist
    const WorkAnimation lookWork = getLookWorkAnimation(lookJob);
    const auto getLookFrame = [&loader, &lookWork](unsigned frame) -> const ArchivItem_BitmapBase* {
        return static_cast<const libsiedler2::ArchivItem_Bitmap_Player*>(
          loader.GetPlayerImage("rom_bobs", lookWork.firstImage + frame % lookWork.numImages));
    };
    for(unsigned frame = 0; frame < lookWork.numImages; ++frame)
        writeTemplate(folder, withNumber("Arbeiter_Arbeit", frame), getLookFrame(frame), false, lookWork.offset);
    for(unsigned frame = 0;; ++frame)
    {
        // Anchor relative to the figure (the placeholder is drawn with an offset)
        glArchivItem_Bitmap_Player* image = loadPlayerImage(folder, withNumber("Arbeiter_Arbeit", frame),
                                                            getAnchorRef(getLookFrame(frame), lookWork.offset));
        if(!image)
            break;
        gfx.workFrames.push_back(image);
    }
}

void CustomGraphics::loadMapObject(Loader& loader, glTexturePacker* stp, unsigned objIdx, const CustomMapObject& obj)
{
    const Folder folder(settings_.folder / obj.name,
                        settings_.templateFolder.empty() ? bfs::path() : settings_.templateFolder / obj.name);
    std::vector<glSmartBitmap*>& sprites = mapObjects_[objIdx];
    sprites.assign(obj.sizes, nullptr);
    // The placeholder differs between the landscapes -> templates with the landscape in the name
    const std::string landscapeSuffix =
      (settings_.landscape.empty() || settings_.landscape == "Gruenland") ? "" : "_" + settings_.landscape;
    const unsigned graniteType = obj.lookGranite == 1 ? 1 : 0;
    for(unsigned state = 0; state < obj.sizes; ++state)
    {
        const unsigned graniteState = getPlaceholderGraniteState(state, obj.sizes);
        const ArchivItem_BitmapBase* lookImage = loader.GetMapImage(516 + graniteType * 6 + graniteState);
        const ArchivItem_BitmapBase* lookShadow = loader.GetMapImage(616 + graniteType * 6 + graniteState);
        const std::string imageName = "Groesse" + std::to_string(state);
        glArchivItem_Bitmap* image = loadImage(folder, imageName, getAnchorRef(lookImage));
        glArchivItem_Bitmap* shadow = loadImage(folder, imageName + "_Schatten", getAnchorRef(lookShadow));
        writeTemplate(folder, imageName + landscapeSuffix, lookImage, false, DrawPoint(0, 0), obj.tint);
        writeTemplate(folder, imageName + "_Schatten" + landscapeSuffix, lookShadow, true);
        if(!image)
            continue;
        auto sprite = std::make_unique<glSmartBitmap>();
        sprite->add(image);
        if(shadow)
            sprite->addShadow(shadow);
        if(stp)
            stp->add(*sprite);
        sprites[state] = sprite.get();
        sprites_.push_back(std::move(sprite));
    }
}

glArchivItem_Bitmap* CustomGraphics::GetWareIcon(GoodType good) const
{
    return isCustomGood(good) ? wares_[getCustomGoodIdx(good)].icon : nullptr;
}

glArchivItem_Bitmap* CustomGraphics::GetWareFlag(GoodType good) const
{
    return isCustomGood(good) ? wares_[getCustomGoodIdx(good)].flag : nullptr;
}

glArchivItem_Bitmap* CustomGraphics::GetWareDonkey(GoodType good) const
{
    return isCustomGood(good) ? wares_[getCustomGoodIdx(good)].donkey : nullptr;
}

glArchivItem_Bitmap* CustomGraphics::GetJobIcon(Job job) const
{
    return isCustomJob(job) ? buildings_[getCustomJobIdx(job)].jobIcon : nullptr;
}

glArchivItem_Bitmap* CustomGraphics::GetBuildingIcon(Nation nation, BuildingType bld) const
{
    return isCustomBuilding(bld) ? buildings_[getCustomBuildingIdx(bld)].icon[nation] : nullptr;
}

glArchivItem_Bitmap* CustomGraphics::GetBuildingImage(Nation nation, BuildingType bld) const
{
    return isCustomBuilding(bld) ? buildings_[getCustomBuildingIdx(bld)].image[nation] : nullptr;
}

bool CustomGraphics::HasWalking(Job job) const
{
    return isCustomJob(job) && buildings_[getCustomJobIdx(job)].hasWalking;
}

const std::vector<glArchivItem_Bitmap_Player*>& CustomGraphics::GetWorkFrames(Job job) const
{
    static const std::vector<glArchivItem_Bitmap_Player*> empty;
    return isCustomJob(job) ? buildings_[getCustomJobIdx(job)].workFrames : empty;
}

glSmartBitmap* CustomGraphics::GetMapObject(unsigned objIdx, unsigned state) const
{
    if(objIdx >= mapObjects_.size() || state >= mapObjects_[objIdx].size())
        return nullptr;
    return mapObjects_[objIdx][state];
}

} // namespace civ
