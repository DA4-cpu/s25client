// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "civ/Content.h"
#include "civ/Identifiers.h"
#include "helpers/EnumRange.h"
#include "helpers/containerUtils.h"
#include "gameData/GameConsts.h"
#include "gameData/ShieldConsts.h"
#include "gameData/WorldDescription.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <set>

namespace civ {

namespace {
    std::shared_ptr<const Content>& activeContentPtr()
    {
        static std::shared_ptr<const Content> content;
        return content;
    }

    bool isUsableBuiltinGood(GoodType good)
    {
        return !isCustomGood(good) && good != GoodType::WaterEmpty && ConvertShields(good) == good;
    }

    constexpr std::array<GoodType, 12> TOOLS = {GoodType::Tongs,    GoodType::Hammer,     GoodType::Axe,
                                                GoodType::Saw,      GoodType::PickAxe,    GoodType::Shovel,
                                                GoodType::Crucible, GoodType::RodAndLine, GoodType::Scythe,
                                                GoodType::Cleaver,  GoodType::Rollingpin, GoodType::Bow};

    std::string joinTags(const TagQuery& query)
    {
        std::string result;
        for(const std::string& tag : query)
        {
            if(!result.empty())
                result += ", ";
            result += tag;
        }
        return "{" + result + "}";
    }

    bool sameProperties(std::vector<std::string> lhs, std::vector<std::string> rhs)
    {
        std::sort(lhs.begin(), lhs.end());
        std::sort(rhs.begin(), rhs.end());
        return lhs == rhs;
    }

    WaresNeeded makeWaresNeeded(const std::vector<GoodType>& goods)
    {
        switch(goods.size())
        {
            case 0: return WaresNeeded();
            case 1: return WaresNeeded(goods[0]);
            case 2: return WaresNeeded(goods[0], goods[1]);
            case 3: return WaresNeeded(goods[0], goods[1], goods[2]);
            default: throw GameDataError("Too many input wares");
        }
    }
} // namespace

GoodType CustomBuilding::GetProduct(std::optional<GoodType> consumedInput) const
{
    if(productByInput.empty())
        return product;
    if(consumedInput)
    {
        for(const auto& entry : productByInput)
        {
            if(entry.first == *consumedInput)
                return entry.second;
        }
    }
    return productByInput.front().second;
}

Content::Content()
{
    for(const auto good : helpers::enumRange<GoodType>())
    {
        if(isUsableBuiltinGood(good))
            tags_[good] = {getIdentifier(good)};
    }
}

Content::Content(const WorldDescription& desc) : Content()
{
    // Wares
    if(desc.wares.size() > NUM_CUSTOM_GOODS)
        throw GameDataError("At most " + std::to_string(NUM_CUSTOM_GOODS) + " new wares can be defined");
    std::set<std::string> wareNames;
    for(unsigned i = 0; i < desc.wares.size(); ++i)
    {
        const WareDesc& wareDesc = desc.wares[i];
        const std::string context = "Ware '" + wareDesc.name + "'";
        if(goodFromIdentifier(wareDesc.name) || wareDesc.name == "WaterEmpty"
           || !wareNames.insert(wareDesc.name).second)
            throw GameDataError(context + " is defined twice (or is the name of a built-in ware)");
        CustomWare ware;
        ware.name = wareDesc.name;
        ware.title = wareDesc.title;
        const auto look = goodFromIdentifier(wareDesc.look);
        if(!look)
            throw GameDataError(context + ": look must be a built-in ware, not '" + wareDesc.look + "'");
        ware.look = *look;
        const auto transport = goodFromIdentifier(wareDesc.transport);
        if(!transport)
            throw GameDataError(context + ": transport must be a built-in ware, not '" + wareDesc.transport + "'");
        ware.transportLike = *transport;
        for(const auto type : helpers::enumRange<KnowledgeType>())
            ware.knowledge[type] = wareDesc.knowledge[static_cast<unsigned>(type)];
        wares_.push_back(ware);
        tags_[getCustomGood(i)] = {ware.name};
        SetTags(getCustomGood(i), wareDesc.kind, wareDesc.properties);
    }
    for(const auto& entry : desc.wareTags)
    {
        const auto good = goodFromIdentifier(entry.first);
        if(!good)
        {
            throw GameDataError("Tags for '" + entry.first
                                + "': not a built-in ware (new wares get their tags in rttr:AddWare)");
        }
        SetTags(*good, entry.second.kind, entry.second.properties);
    }

    // Workshops
    if(desc.workshops.size() > NUM_CUSTOM_BUILDINGS)
        throw GameDataError("At most " + std::to_string(NUM_CUSTOM_BUILDINGS) + " workshops can be defined");
    std::set<std::string> buildingNames;
    for(unsigned i = 0; i < desc.workshops.size(); ++i)
    {
        const WorkshopDesc& wsDesc = desc.workshops[i];
        const std::string context = "Workshop '" + wsDesc.name + "'";
        if(buildingFromIdentifier(wsDesc.name, true) || wsDesc.name == "Nothing9"
           || !buildingNames.insert(wsDesc.name).second)
            throw GameDataError(context + " is defined twice (or is the name of a built-in building)");
        CustomBuilding bld;
        bld.name = wsDesc.name;
        bld.title = wsDesc.title;
        bld.description = wsDesc.description;
        if(wsDesc.size == "small")
            bld.size = BuildingQuality::Hut;
        else if(wsDesc.size == "medium")
            bld.size = BuildingQuality::House;
        else
            bld.size = BuildingQuality::Castle;

        bld.cost = BuildingCost{0, 0};
        for(const auto& costEntry : wsDesc.cost)
        {
            if(costEntry.second > 255)
                throw GameDataError(context + ": costs must be <= 255");
            if(costEntry.first == "Boards")
                bld.cost.boards = static_cast<uint8_t>(costEntry.second);
            else if(costEntry.first == "Stones")
                bld.cost.stones = static_cast<uint8_t>(costEntry.second);
            else
                throw GameDataError(context + ": construction costs can only be Boards and Stones (for now), not '"
                                    + costEntry.first + "'");
        }
        if(bld.cost.boards + bld.cost.stones == 0)
            throw GameDataError(context + ": construction costs are missing");

        const auto look = buildingFromIdentifier(wsDesc.look);
        if(!look)
            throw GameDataError(context + ": look must be a built-in building, not '" + wsDesc.look + "'");
        bld.look = *look;

        // Worker
        CustomWorker worker;
        worker.name = wsDesc.name;
        worker.title = wsDesc.worker.title;
        if(wsDesc.worker.tool == "none")
            worker.tool = GoodType::Nothing;
        else
        {
            const auto tool = goodFromIdentifier(wsDesc.worker.tool);
            if(!tool || !helpers::contains(TOOLS, *tool))
            {
                throw GameDataError(context
                                    + ": the tool of the worker must be a tool (e.g. \"Hammer\") or \"none\", not '"
                                    + wsDesc.worker.tool + "'");
            }
            worker.tool = *tool;
        }
        const auto workerLook = jobFromIdentifier(wsDesc.worker.look);
        if(!workerLook)
            throw GameDataError(context + ": the look of the worker must be a built-in job (e.g. \"Carpenter\"), not '"
                                + wsDesc.worker.look + "'");
        worker.look = *workerLook;

        // Inputs
        std::vector<GoodType> inputWares;
        for(const TagQuery& input : wsDesc.inputs)
        {
            const std::vector<GoodType> matches = FindWares(input);
            if(matches.empty())
                throw GameDataError(context + ": input " + joinTags(input) + " matches no ware");
            if(wsDesc.consumeEach && matches.size() != 1u)
            {
                throw GameDataError(context + ": input " + joinTags(input) + " matches "
                                    + std::to_string(matches.size())
                                    + " wares. With consume = \"each\" every input must match exactly one ware "
                                      "(use consume = \"any\" for alternatives)");
            }
            for(const GoodType good : matches)
            {
                if(!helpers::contains(inputWares, good))
                    inputWares.push_back(good);
            }
        }
        if(inputWares.size() > 3u)
            throw GameDataError(context + ": the inputs match more than 3 different wares");

        // Output
        const std::vector<GoodType> outputMatches = FindWares(wsDesc.output);
        if(outputMatches.empty())
            throw GameDataError(context + ": output " + joinTags(wsDesc.output) + " matches no ware");
        if(wsDesc.inheritProperties)
        {
            if(inputWares.empty())
                throw GameDataError(context + ": inheritProperties requires inputs");
            for(const GoodType input : inputWares)
            {
                std::vector<GoodType> products;
                for(const GoodType candidate : outputMatches)
                {
                    if(sameProperties(GetProperties(candidate), GetProperties(input)))
                        products.push_back(candidate);
                }
                if(products.size() != 1u)
                {
                    throw GameDataError(context + ": for input '" + getIdentifierOf(input) + "' "
                                        + std::to_string(products.size()) + " wares match the output "
                                        + joinTags(wsDesc.output) + " with the same properties (expected exactly 1)");
                }
                bld.productByInput.emplace_back(input, products.front());
            }
        } else if(wsDesc.outputChoice == "knowledge")
        {
            // One product per knowledge type, e.g. carved bones for LWP, IWP and MWP
            for(const GoodType candidate : outputMatches)
            {
                const CustomWare* candidateWare = GetWare(candidate);
                const bool hasKnowledge =
                  candidateWare
                  && std::any_of(candidateWare->knowledge.begin(), candidateWare->knowledge.end(),
                                 [](int points) { return points != 0; });
                if(!hasKnowledge)
                {
                    throw GameDataError(context + ": with outputChoice = \"knowledge\" every ware of the output "
                                        + joinTags(wsDesc.output) + " needs knowledge points, but '"
                                        + getIdentifierOf(candidate) + "' has none");
                }
            }
            bld.productChoices = outputMatches;
            bld.product = outputMatches.front();
        } else
        {
            if(outputMatches.size() != 1u)
            {
                throw GameDataError(context + ": output " + joinTags(wsDesc.output) + " matches "
                                    + std::to_string(outputMatches.size())
                                    + " wares (expected exactly 1, or use outputChoice = \"knowledge\")");
            }
            bld.product = outputMatches.front();
        }

        bld.workDesc.job = getCustomJob(i);
        bld.workDesc.producedWare = bld.GetProduct(std::nullopt);
        bld.workDesc.waresNeeded = makeWaresNeeded(inputWares);
        bld.workDesc.numSpacesPerWare = static_cast<uint8_t>(wsDesc.storage);
        bld.workDesc.useOneWareEach = wsDesc.consumeEach;
        bld.workGFs = static_cast<unsigned>(duration_to_gfs(std::chrono::seconds(wsDesc.workTime)));
        bld.waitGFs = static_cast<unsigned>(duration_to_gfs(std::chrono::seconds(wsDesc.waitTime)));

        buildings_.push_back(std::move(bld));
        workers_.push_back(std::move(worker));
    }
}

std::string Content::getIdentifierOf(GoodType good) const
{
    if(const CustomWare* ware = GetWare(good))
        return ware->name;
    return getIdentifier(good);
}

void Content::SetTags(GoodType good, const std::string& kind, const std::vector<std::string>& properties)
{
    std::vector<std::string>& tags = tags_[good];
    if(!kind.empty() && !helpers::contains(tags, kind))
        tags.push_back(kind);
    for(const std::string& property : properties)
    {
        if(!helpers::contains(tags, property))
            tags.push_back(property);
        if(!helpers::contains(properties_[good], property))
            properties_[good].push_back(property);
    }
}

const CustomWare* Content::GetWare(GoodType good) const
{
    if(!isCustomGood(good))
        return nullptr;
    const unsigned idx = getCustomGoodIdx(good);
    return idx < wares_.size() ? &wares_[idx] : nullptr;
}

const CustomBuilding* Content::GetBuilding(BuildingType bld) const
{
    if(!isCustomBuilding(bld))
        return nullptr;
    const unsigned idx = getCustomBuildingIdx(bld);
    return idx < buildings_.size() ? &buildings_[idx] : nullptr;
}

const CustomWorker* Content::GetWorker(Job job) const
{
    if(!isCustomJob(job))
        return nullptr;
    const unsigned idx = getCustomJobIdx(job);
    return idx < workers_.size() ? &workers_[idx] : nullptr;
}

bool Content::IsUsed(GoodType good) const
{
    return isCustomGood(good) ? GetWare(good) != nullptr : good != GoodType::Nothing;
}

bool Content::IsUsed(BuildingType bld) const
{
    return isCustomBuilding(bld) ? GetBuilding(bld) != nullptr : bld != BuildingType::Nothing9;
}

bool Content::IsUsed(Job job) const
{
    return !isCustomJob(job) || GetWorker(job) != nullptr;
}

bool Content::Matches(GoodType good, const TagQuery& query) const
{
    const std::vector<std::string>& tags = tags_[good];
    if(tags.empty())
        return false;
    return std::all_of(query.begin(), query.end(),
                       [&tags](const std::string& tag) { return helpers::contains(tags, tag); });
}

std::vector<GoodType> Content::FindWares(const TagQuery& query) const
{
    std::vector<GoodType> result;
    for(const auto good : helpers::enumRange<GoodType>())
    {
        if(Matches(good, query))
            result.push_back(good);
    }
    return result;
}

std::optional<GoodType> Content::FindGood(const std::string& identifier) const
{
    if(const auto good = goodFromIdentifier(identifier))
        return good;
    for(unsigned i = 0; i < wares_.size(); ++i)
    {
        if(wares_[i].name == identifier)
            return getCustomGood(i);
    }
    return std::nullopt;
}

std::optional<BuildingType> Content::FindBuilding(const std::string& identifier, bool allowHeadquarters) const
{
    if(const auto bld = buildingFromIdentifier(identifier, allowHeadquarters))
        return bld;
    for(unsigned i = 0; i < buildings_.size(); ++i)
    {
        if(buildings_[i].name == identifier)
            return getCustomBuilding(i);
    }
    return std::nullopt;
}

std::vector<std::string> Content::GetSignature() const
{
    std::vector<std::string> result;
    for(const CustomWare& ware : wares_)
        result.push_back("W:" + ware.name);
    for(const CustomBuilding& bld : buildings_)
        result.push_back("B:" + bld.name);
    return result;
}

bool Content::IsCompatibleTo(const std::vector<std::string>& savedSignature) const
{
    std::vector<std::string> savedWares, savedBuildings;
    for(const std::string& entry : savedSignature)
    {
        if(entry.compare(0, 2, "W:") == 0)
            savedWares.push_back(entry.substr(2));
        else if(entry.compare(0, 2, "B:") == 0)
            savedBuildings.push_back(entry.substr(2));
        else
            return false;
    }
    if(savedWares.size() > wares_.size() || savedBuildings.size() > buildings_.size())
        return false;
    for(unsigned i = 0; i < savedWares.size(); ++i)
    {
        if(savedWares[i] != wares_[i].name)
            return false;
    }
    for(unsigned i = 0; i < savedBuildings.size(); ++i)
    {
        if(savedBuildings[i] != buildings_[i].name)
            return false;
    }
    return true;
}

const Content& getActiveContent()
{
    static const Content emptyContent;
    const auto& content = activeContentPtr();
    return content ? *content : emptyContent;
}

void setActiveContent(std::shared_ptr<const Content> content)
{
    activeContentPtr() = std::move(content);
}

void resetActiveContent(const Content* content)
{
    if(activeContentPtr().get() == content)
        activeContentPtr().reset();
}

GoodType getTransportLike(GoodType good)
{
    if(!isCustomGood(good))
        return good;
    const CustomWare* ware = getActiveContent().GetWare(good);
    return ware ? ware->transportLike : GoodType::Stones;
}

GoodType getLook(GoodType good)
{
    if(!isCustomGood(good))
        return good;
    const CustomWare* ware = getActiveContent().GetWare(good);
    return ware ? ware->look : GoodType::Stones;
}

BuildingType getLook(BuildingType bld)
{
    if(!isCustomBuilding(bld))
        return bld;
    const CustomBuilding* building = getActiveContent().GetBuilding(bld);
    return building ? building->look : BuildingType::Woodcutter;
}

Job getLook(Job job)
{
    if(!isCustomJob(job))
        return job;
    const CustomWorker* worker = getActiveContent().GetWorker(job);
    return worker ? worker->look : Job::Helper;
}

} // namespace civ
