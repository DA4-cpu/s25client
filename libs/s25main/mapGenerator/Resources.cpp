// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mapGenerator/Resources.h"
#include "RttrForeachPt.h"
#include "helpers/containerUtils.h"
#include "mapGenerator/Algorithms.h"
#include "mapGenerator/Noise.h"
#include "mapGenerator/TextureHelper.h"
#include "gameData/MapGenDesc.h"
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace rttr::mapGenerator {

TreeCode GetTreeCode(const std::string& species)
{
    using namespace libsiedler2;
    const auto code = [](unsigned type, unsigned index) {
        return TreeCode{static_cast<uint8_t>(type), static_cast<uint8_t>(index)};
    };
    // Tree types of noTree 0-8, see MapLoader::PlaceObjects
    if(species == "Pine")
        return code(OT_TreeOrPalm, OI_TreeOrPalm_Begin);
    if(species == "Birch")
        return code(OT_TreeOrPalm, OI_Tree1_Begin);
    if(species == "Oak")
        return code(OT_TreeOrPalm, OI_Tree2_Begin);
    if(species == "Palm1")
        return code(OT_TreeOrPalm, OI_Palm_Begin);
    if(species == "Palm2")
        return code(OT_Palm, OI_TreeOrPalm_Begin);
    if(species == "Pineapple")
        return code(OT_Palm, OI_Tree1_Begin);
    if(species == "Cypress")
        return code(OT_Palm, OI_Tree2_Begin);
    if(species == "Cherry")
        return code(OT_Palm, OI_Palm_Begin);
    if(species == "Fir")
        return code(OT_Palm + 1u, OI_TreeOrPalm_Begin);
    throw std::invalid_argument("Unknown tree species: " + species);
}

libsiedler2::Animal GetAnimalCode(const std::string& species)
{
    using libsiedler2::Animal;
    if(species == "Rabbit")
        return Animal::Rabbit;
    if(species == "Fox")
        return Animal::Fox;
    if(species == "Stag")
        return Animal::Stag;
    if(species == "Deer")
        return Animal::Deer;
    if(species == "Duck")
        return Animal::Duck;
    if(species == "Sheep")
        return Animal::Sheep;
    throw std::invalid_argument("Unknown animal species: " + species);
}

bool IsWaterAnimal(const std::string& species)
{
    return species == "Duck";
}

double GetResourceAmount(const MapResourceDesc& resource, const MapSettings& settings)
{
    const std::string& slider = resource.amountSlider;
    if(slider.empty())
        return resource.amount;
    if(slider == "forest")
        return settings.trees;
    if(slider == "stonePiles")
        return settings.stonePiles;
    if(slider == "coal")
        return settings.ratioCoal;
    if(slider == "iron")
        return settings.ratioIron;
    if(slider == "gold")
        return settings.ratioGold;
    if(slider == "granite")
        return settings.ratioGranite;
    throw std::invalid_argument("Unknown slider: " + slider);
}

const MapZonePackageDesc* GetZonePackage(const Map& map, const MapGenDesc& desc, const MapPoint& pt)
{
    const unsigned zone = map.zones[pt];
    if(zone >= desc.zones.size())
        return nullptr;
    for(const MapZonePackageDesc& package : desc.zones[zone].packages)
    {
        if(package.climate.matches(map.temperature[pt], map.climate[pt], map.heightPercent[pt]))
            return &package;
    }
    return nullptr;
}

namespace {

enum class ObjectKind
{
    Trees,
    Stones,
    Animals,
    Decoration,
    Fish,
    Groundwater,
    Mine
};

ObjectKind GetObjectKind(const std::string& object)
{
    if(object == "trees")
        return ObjectKind::Trees;
    if(object == "stones")
        return ObjectKind::Stones;
    if(object == "animals")
        return ObjectKind::Animals;
    if(object == "decoration")
        return ObjectKind::Decoration;
    if(object == "fish")
        return ObjectKind::Fish;
    if(object == "groundwater")
        return ObjectKind::Groundwater;
    if(object == "coal" || object == "iron" || object == "gold" || object == "granite")
        return ObjectKind::Mine;
    throw std::invalid_argument("Unknown map resource object: " + object);
}

uint8_t GetMineResourceBase(const std::string& object)
{
    if(object == "coal")
        return libsiedler2::R_Coal;
    if(object == "iron")
        return libsiedler2::R_Iron;
    if(object == "gold")
        return libsiedler2::R_Gold;
    return libsiedler2::R_Granite;
}

/// Decoration objects without function (object type 0xC8): bushes, flowers, mushrooms, small stones
constexpr std::array<uint8_t, 19> decorationIds{0x00, 0x01, 0x05, 0x06, 0x0A, 0x0E, 0x0F, 0x10, 0x11, 0x12,
                                                0x13, 0x14, 0x22, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A};

template<class T>
const std::string* PickWeighted(RandomUtility& rnd, const MapGenWeights& weights, T&& isAllowed)
{
    unsigned total = 0;
    for(const auto& entry : weights)
    {
        if(isAllowed(entry.first))
            total += entry.second;
    }
    if(total == 0)
        return nullptr;
    unsigned value = rnd.RandomValue(1u, total);
    for(const auto& entry : weights)
    {
        if(!isAllowed(entry.first))
            continue;
        if(value <= entry.second)
            return &entry.first;
        value -= entry.second;
    }
    return nullptr; // LCOV_EXCL_LINE
}

/// Data shared by all rules
class ResourcePlacer
{
public:
    ResourcePlacer(Map& map, RandomUtility& rnd, const MapSettings& settings, const MapGenDesc& desc)
        : map_(map), rnd_(rnd), settings_(settings), desc_(desc), globalFactor_(settings.resourceAmount / 100.)
    {
        const MapExtent size = map.size;
        const auto& textures = map.textureMap;

        packages_.Resize(size, nullptr);
        objectFree_.Resize(size, 0);
        mineable_.Resize(size, 0);
        allWater_.Resize(size, 0);

        // Keep the area around HQs and harbors free of objects, otherwise harbors could be inaccessible or HQs
        // could not be placed
        std::vector<MapPoint> reserved;
        RTTR_FOREACH_PT(MapPoint, size)
        {
            if(helpers::contains(map.hqPositions, pt)
               || helpers::contains_if(map.harbors, [pt](const Triangle& tr) { return tr.position == pt; }))
            {
                for(const MapPoint& p : map.z.GetPointsInRadiusWithCenter(pt, 5))
                    reserved.push_back(p);
            }
        }
        NodeMapBase<uint8_t> isReserved;
        isReserved.Resize(size, 0);
        for(const MapPoint& pt : reserved)
            isReserved[pt] = 1;

        const auto isBlocked = [](const TerrainDesc& t) {
            return IsSnowOrLava(t) || (t.kind == TerrainKind::Land && !t.Is(ETerrain::Walkable));
        };
        RTTR_FOREACH_PT(MapPoint, size)
        {
            packages_[pt] = GetZonePackage(map, desc, pt);
            allWater_[pt] = textures.All(pt, IsWater) ? 1 : 0;
            mineable_[pt] = textures.All(pt, IsMinableMountain) ? 1 : 0;
            objectFree_[pt] = (!isReserved[pt] && !allWater_[pt] && !textures.Any(pt, isBlocked)) ? 1 : 0;
        }
        if(!map.hqPositions.empty())
            hqDistance_ = DistancesTo(map.hqPositions, size);
        else
            hqDistance_.Resize(size, std::numeric_limits<unsigned>::max());
    }

    void PlaceAll()
    {
        bool minesDone = false;
        for(const MapResourceDesc& rule : desc_.resources)
        {
            const ObjectKind kind = GetObjectKind(rule.object);
            if(kind == ObjectKind::Mine)
            {
                // All mine resources are placed together, so they share the mountains according to their amounts
                if(!minesDone)
                    PlaceMineResources();
                minesDone = true;
            } else
                PlaceRule(rule, kind);
        }
    }

private:
    Map& map_;
    RandomUtility& rnd_;
    const MapSettings& settings_;
    const MapGenDesc& desc_;
    const double globalFactor_;
    NodeMapBase<const MapZonePackageDesc*> packages_;
    /// Objects (trees, stones, ...) may be placed here
    NodeMapBase<uint8_t> objectFree_;
    /// All triangles are mineable mountain
    NodeMapBase<uint8_t> mineable_;
    /// All triangles are water
    NodeMapBase<uint8_t> allWater_;
    NodeMapBase<unsigned> hqDistance_;

    bool Matches(const MapResourceDesc& rule, const std::vector<unsigned>& zones, const MapPoint& pt) const
    {
        if((map_.terrainFlags[pt] & rule.terrain) == 0)
            return false;
        if(!rule.climate.matches(map_.temperature[pt], map_.climate[pt], map_.heightPercent[pt]))
            return false;
        if(!zones.empty() && !helpers::contains(zones, static_cast<unsigned>(map_.zones[pt])))
            return false;
        if(rule.hqDistance
           && (hqDistance_[pt] < rule.hqDistance->first || hqDistance_[pt] > rule.hqDistance->second))
            return false;
        return true;
    }

    std::vector<unsigned> GetZoneIndices(const MapResourceDesc& rule) const
    {
        std::vector<unsigned> result;
        for(const std::string& zone : rule.zones)
        {
            const int idx = desc_.getZoneIndex(zone);
            if(idx < 0)
                throw std::runtime_error("Map resource '" + rule.name + "': unknown zone '" + zone + "'");
            result.push_back(static_cast<unsigned>(idx));
        }
        return result;
    }

    double GetProbability(const MapResourceDesc& rule, double amount, const MapPoint& pt) const
    {
        return amount / 100. * globalFactor_
               * rule.getWeight(map_.temperature[pt], map_.climate[pt], map_.heightPercent[pt]);
    }

    /// Try to place the object of the rule. Returns false if the node is not suitable
    bool Place(ObjectKind kind, const MapPoint& pt)
    {
        using namespace libsiedler2;
        switch(kind)
        {
            case ObjectKind::Trees:
            {
                if(!objectFree_[pt] || map_.objectTypes[pt] != OT_Empty || !packages_[pt])
                    return false;
                const std::string* species = PickWeighted(rnd_, packages_[pt]->trees, [](const auto&) { return true; });
                if(!species)
                    return false;
                const TreeCode code = GetTreeCode(*species);
                map_.objectTypes[pt] = code.type;
                map_.objectInfos[pt] = static_cast<uint8_t>(code.index + rnd_.RandomValue(0, 7));
                return true;
            }
            case ObjectKind::Stones:
                if(!objectFree_[pt] || map_.objectTypes[pt] != OT_Empty)
                    return false;
                map_.objectTypes[pt] = static_cast<uint8_t>(rnd_.ByChance(50) ? OT_Stone1 : OT_Stone2);
                map_.objectInfos[pt] = static_cast<uint8_t>(rnd_.RandomValue(1, 6));
                return true;
            case ObjectKind::Decoration:
                if(!objectFree_[pt] || map_.objectTypes[pt] != OT_Empty)
                    return false;
                map_.objectTypes[pt] = 0xC8;
                map_.objectInfos[pt] = decorationIds[rnd_.Index(decorationIds.size())];
                return true;
            case ObjectKind::Animals:
            {
                if(map_.animals[pt] != Animal::None || !packages_[pt])
                    return false;
                const bool onWater = allWater_[pt] != 0;
                if(!onWater && !objectFree_[pt])
                    return false;
                const std::string* species = PickWeighted(rnd_, packages_[pt]->animals, [onWater](const auto& name) {
                    return IsWaterAnimal(name) == onWater;
                });
                if(!species)
                    return false;
                map_.animals[pt] = GetAnimalCode(*species);
                return true;
            }
            case ObjectKind::Fish:
                if(map_.resources[pt] != R_None)
                    return false;
                map_.resources[pt] = R_Fish;
                return true;
            case ObjectKind::Groundwater:
                if(map_.resources[pt] != R_None)
                    return false;
                map_.resources[pt] = R_Water;
                return true;
            case ObjectKind::Mine: break;
        }
        return false; // LCOV_EXCL_LINE
    }

    void PlaceRule(const MapResourceDesc& rule, ObjectKind kind)
    {
        const double amount = GetResourceAmount(rule, settings_);
        if(amount <= 0. || globalFactor_ <= 0.)
            return;
        const auto zones = GetZoneIndices(rule);
        const double density = rule.density / 100.;
        NodeMapBase<double> clusterValues;
        if(rule.cluster > 0)
            clusterValues = ToUniform(CreateFractalNoise(rnd_, map_.size, rule.cluster, 2, 0.5));

        RTTR_FOREACH_PT(MapPoint, map_.size)
        {
            if(!Matches(rule, zones, pt))
                continue;
            const double probability = GetProbability(rule, amount, pt);
            if(probability <= 0.)
                continue;
            if(rule.cluster > 0)
            {
                // Inside a cluster (smooth noise below the threshold) a share of `density` of the nodes is used, so
                // on average the share `probability` of all matching nodes gets the object
                if(clusterValues[pt] >= std::min(1., probability / density) || rnd_.RandomDouble(0., 1.) >= density)
                    continue;
            } else if(rnd_.RandomDouble(0., 1.) >= probability)
                continue;
            Place(kind, pt);
        }
    }

    void PlaceMineResources()
    {
        struct MineRule
        {
            const MapResourceDesc* rule;
            std::vector<unsigned> zones;
            double amount;
            uint8_t resourceBase;
            unsigned radius;
        };
        std::vector<MineRule> rules;
        for(const MapResourceDesc& rule : desc_.resources)
        {
            if(GetObjectKind(rule.object) != ObjectKind::Mine)
                continue;
            const double amount = GetResourceAmount(rule, settings_);
            if(amount > 0.)
                rules.push_back(MineRule{&rule, GetZoneIndices(rule), amount, GetMineResourceBase(rule.object),
                                         rule.cluster / 2u});
        }
        if(rules.empty() || globalFactor_ <= 0.)
            return;

        // Visit the mountain nodes in random order. Each node starts a cluster of one resource (chosen according to
        // the amounts) with a probability so that the clusters cover about the share sum(amount) of the mountains.
        // Several passes because later clusters overlap earlier ones.
        std::vector<MapPoint> nodes;
        RTTR_FOREACH_PT(MapPoint, map_.size)
        {
            if(mineable_[pt])
                nodes.push_back(pt);
        }
        for(unsigned i = nodes.size(); i > 1; --i)
            std::swap(nodes[i - 1], nodes[rnd_.Index(i)]);

        std::vector<double> probabilities(rules.size());
        const auto computeProbabilities = [&](const MapPoint& pt) {
            double total = 0.;
            for(unsigned i = 0; i < rules.size(); ++i)
            {
                probabilities[i] =
                  Matches(*rules[i].rule, rules[i].zones, pt) ? GetProbability(*rules[i].rule, rules[i].amount, pt) : 0.;
                total += probabilities[i];
            }
            return total;
        };
        double target = 0.;
        for(const MapPoint& pt : nodes)
            target += std::min(1., computeProbabilities(pt));

        unsigned numPlaced = 0;
        constexpr unsigned maxPasses = 5;
        for(unsigned pass = 0; pass < maxPasses && numPlaced < target; ++pass)
        {
            for(const MapPoint& pt : nodes)
            {
                if(numPlaced >= target)
                    break;
                if(map_.resources[pt] != libsiedler2::R_None)
                    continue;
                const double total = computeProbabilities(pt);
                if(total <= 0.)
                    continue;
                double value = rnd_.RandomDouble(0., total);
                unsigned chosen = 0;
                while(chosen + 1 < rules.size() && value >= probabilities[chosen])
                {
                    value -= probabilities[chosen];
                    ++chosen;
                }
                const MineRule& mineRule = rules[chosen];
                const unsigned radius = mineRule.radius;
                const double density = mineRule.rule->density / 100.;
                // Free nodes the cluster would cover: Clusters at the edge of the mountains cover less, so they are
                // started more often
                std::vector<MapPoint> clusterNodes;
                for(const MapPoint& p : map_.z.GetPointsInRadiusWithCenter(pt, radius))
                {
                    if(map_.resources[p] == libsiedler2::R_None && mineable_[p]
                       && Matches(*mineRule.rule, mineRule.zones, p))
                        clusterNodes.push_back(p);
                }
                const double expectedSize = std::max(1., clusterNodes.size() * density);
                if(rnd_.RandomDouble(0., 1.) >= std::min(1., total) / expectedSize)
                    continue;
                for(const MapPoint& p : clusterNodes)
                {
                    if(p != pt && rnd_.RandomDouble(0., 1.) >= density)
                        continue;
                    // Rich in the middle of the cluster, less at the edges
                    const unsigned distance = map_.z.CalcDistance(p, pt);
                    const unsigned amount = (distance * 2 <= radius) ? 7u : rnd_.RandomValue(1u, 6u);
                    map_.resources[p] = static_cast<uint8_t>(mineRule.resourceBase + amount);
                    ++numPlaced;
                }
            }
        }
    }
};

} // namespace

void PlaceResources(Map& map, RandomUtility& rnd, const MapSettings& settings, const MapGenDesc& desc)
{
    ResourcePlacer placer(map, rnd, settings, desc);
    placer.PlaceAll();
}

} // namespace rttr::mapGenerator
