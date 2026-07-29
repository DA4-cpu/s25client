// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "BuildingPlanner.h"
#include "AIPlayerJH.h"
#include "GlobalGameSettings.h"
#include "addons/const_addons.h"
#include "buildings/nobBaseWarehouse.h"
#include "buildings/nobMilitary.h"
#include "gameTypes/BuildingType.h"
#include "gameTypes/GoodTypes.h"
#include "gameData/BuildingProperties.h"
#include <algorithm>
#include <cmath>

namespace AIJH {
BuildingPlanner::BuildingPlanner(const AIPlayerJH& aijh) : buildingsWanted(), expansionRequired(false)
{
    RefreshBuildingNums(aijh);
    InitBuildingsWanted(aijh);
    UpdateBuildingsWanted(aijh);
}

void BuildingPlanner::Update(unsigned gf, AIPlayerJH& aijh)
{
    RefreshBuildingNums(aijh);
    expansionRequired = CalcIsExpansionRequired(aijh, gf > 500 && gf % 500 == 0);
}

void BuildingPlanner::RefreshBuildingNums(const AIPlayerJH& aijh)
{
    buildingNums = aijh.player.GetBuildingRegister().GetBuildingNums();
}

bool BuildingPlanner::CalcIsExpansionRequired(AIPlayerJH& aijh, bool recalc) const
{
    if(!expansionRequired && !recalc)
        return false;
    if(GetNumMilitaryBldSites() > 0 || GetNumMilitaryBlds() > 4)
        return false;
    bool hasWood = GetNumBuildings(BuildingType::Woodcutter) - GetNumBuildingSites(BuildingType::Woodcutter) > 0;
    bool hasBoards = GetNumBuildings(BuildingType::Sawmill) - GetNumBuildingSites(BuildingType::Sawmill) > 0;
    bool hasStone = GetNumBuildings(BuildingType::Quarry) - GetNumBuildings(BuildingType::Quarry) > 0;
    if(expansionRequired)
    {
      if(hasWood && hasBoards && hasStone)
            return false;
    } else
    {
        // Check if we could build the missing building around any military building or storehouse. If not, expand.
        const BuildingRegister& buildingRegister = aijh.player.GetBuildingRegister();
        std::vector<noBuilding*> blds(buildingRegister.GetMilitaryBuildings().begin(),
                                      buildingRegister.GetMilitaryBuildings().end());
        blds.insert(blds.end(), buildingRegister.GetStorehouses().begin(), buildingRegister.GetStorehouses().end());
        for(const noBuilding* bld : blds)
        {
            if(!hasWood && 1 > GetNumBuildingSites(BuildingType::Woodcutter))
                hasWood = aijh.FindPositionForBuildingAround(BuildingType::Woodcutter, bld->GetPos()).isValid();
            if(!hasBoards && 1 > GetNumBuildingSites(BuildingType::Sawmill))
                hasBoards = aijh.FindPositionForBuildingAround(BuildingType::Sawmill, bld->GetPos()).isValid();
            if(!hasStone && 1 > GetNumBuildingSites(BuildingType::Quarry))
                hasStone = aijh.FindPositionForBuildingAround(BuildingType::Quarry, bld->GetPos()).isValid();
        }
        return !(hasWood && hasBoards && hasStone);
    }
    return expansionRequired;
}

unsigned BuildingPlanner::GetNumBuildings(BuildingType type) const
{
    return buildingNums.buildings[type] + buildingNums.buildingSites[type];
}

unsigned BuildingPlanner::GetNumBuildingSites(BuildingType type) const
{
    return buildingNums.buildingSites[type];
}

unsigned BuildingPlanner::GetNumMilitaryBlds() const
{
    unsigned result = 0;
    for(BuildingType bld : BuildingProperties::militaryBldTypes)
        result += GetNumBuildings(bld);
    return result;
}

unsigned BuildingPlanner::GetNumMilitaryBldSites() const
{
    unsigned result = 0;
    for(BuildingType bld : BuildingProperties::militaryBldTypes)
        result += GetNumBuildingSites(bld);
    return result;
}

void BuildingPlanner::InitBuildingsWanted(const AIPlayerJH& aijh)
{
    std::fill(buildingsWanted.begin(), buildingsWanted.end(), 0u);
    buildingsWanted[BuildingType::Forester] = 
        (std::min(aijh.player.GetInventory().goods[GoodType::Axe] + aijh.player.GetInventory().people[Job::Woodcutter], 6u)/2);
    buildingsWanted[BuildingType::Sawmill] =
      (std::min(aijh.player.GetInventory().goods[GoodType::Saw] + aijh.player.GetInventory().people[Job::Carpenter], 4u));
    buildingsWanted[BuildingType::Woodcutter] = 
        (std::min(aijh.player.GetInventory().goods[GoodType::Axe] + aijh.player.GetInventory().people[Job::Woodcutter], 6u));
    buildingsWanted[BuildingType::GraniteMine] = 0;
    buildingsWanted[BuildingType::CoalMine] = 0;
    buildingsWanted[BuildingType::IronMine] = 0;
    buildingsWanted[BuildingType::GoldMine] = 0;
    buildingsWanted[BuildingType::Catapult] = 5;
    buildingsWanted[BuildingType::Fishery] = 1;
    buildingsWanted[BuildingType::Quarry] = 1;
    buildingsWanted[BuildingType::Hunter] = 0;
    buildingsWanted[BuildingType::Farm] =
      aijh.player.GetInventory().goods[GoodType::Scythe] + aijh.player.GetInventory().people[Job::Farmer];

    unsigned numAIRelevantSeaIds = aijh.GetNumAIRelevantSeaIds();
    if(numAIRelevantSeaIds > 0)
    {
        buildingsWanted[BuildingType::HarborBuilding] = 99;
        buildingsWanted[BuildingType::Shipyard] = numAIRelevantSeaIds == 1 ? 1 : 99;
    }
}

void BuildingPlanner::UpdateBuildingsWanted(const AIPlayerJH& aijh)
{
    // no military buildings -> usually start only
    const unsigned numMilitaryBlds = aijh.player.GetBuildingRegister().GetMilitaryBuildings().size();

    if(numMilitaryBlds < 1 && aijh.player.GetBuildingRegister().GetStorehouses().size() < 2)
    {
        buildingsWanted[BuildingType::Forester] = 3;
        // probably only has 1 saw+carpenter but if that is the case the ai will try to produce 1 additional saw very
        // quickly
        buildingsWanted[BuildingType::Sawmill] = 4;
        buildingsWanted[BuildingType::Woodcutter] = 12;
        buildingsWanted[BuildingType::Quarry] = 2;
        buildingsWanted[BuildingType::GraniteMine] = 0;
        buildingsWanted[BuildingType::CoalMine] = 3;
        buildingsWanted[BuildingType::IronMine] = 1;
        buildingsWanted[BuildingType::GoldMine] = 1;
        buildingsWanted[BuildingType::Catapult] = 0;
        buildingsWanted[BuildingType::Fishery] = 2;
        buildingsWanted[BuildingType::Hunter] = 1;
        buildingsWanted[BuildingType::Farm] = 0;
        buildingsWanted[BuildingType::Charburner] = 0;
    } else
    {
        // at least some expansion happened -> more buildings wanted
        // building wanted usually limited by profession workers+tool for profession with some arbitrary limit. Some
        // buildings which are linked to others in a chain / profession-tool-rivalry have additional limits.
        const Inventory& inventory = aijh.player.GetInventory();

        // foresters
        unsigned max_available_forester = inventory[Job::Forester] + inventory[GoodType::Shovel];
        max_available_forester = std::max(0, int(max_available_forester));


        buildingsWanted[BuildingType::Forester] =
          (unsigned)((GetNumBuildings(BuildingType::Storehouse)/2) + GetNumBuildings(BuildingType::HarborBuilding) + 3);


            // If we are low on wood, we need more foresters
        if(8 > aijh.AmountInStorage(GoodType::Wood) && 1 > GetNumBuildingSites(BuildingType::Forester))
        {
            buildingsWanted[BuildingType::Forester]++;
        }

        buildingsWanted[BuildingType::Forester] =
          std::max(GetNumBuildings(BuildingType::Woodcutter) / 2 - 1, buildingsWanted[BuildingType::Forester]);

        buildingsWanted[BuildingType::Forester] =
          std::min(max_available_forester, buildingsWanted[BuildingType::Forester]);

        if(3 > GetNumBuildings(BuildingType::Forester))
            buildingsWanted[BuildingType::Forester] = 3;


        // woodcutters
        unsigned max_available_woodcutter = inventory.goods[GoodType::Axe] + inventory.people[Job::Woodcutter];
        buildingsWanted[BuildingType::Woodcutter] = std::min(buildingsWanted[BuildingType::Sawmill] * 2 + buildingsWanted[BuildingType::Charburner] * 2
                     + buildingsWanted[BuildingType::Vineyard] * 2 + 3,
                   max_available_woodcutter);
        //std::cout << "buildingsWanted[BuildingType::Woodcutter]: " << buildingsWanted[BuildingType::Woodcutter] << std::endl;



        ////on maps with many trees the ai will build woodcutters all over the place which means the foresters are not
        /// really required
        // TODO: get number of trees in own territory. use it relatively to total size of own territory to adapt number
        // of foresters (less) and woodcutters (more)

        // fishery & hunter
        buildingsWanted[BuildingType::Fishery] =
          std::min(inventory.goods[GoodType::RodAndLine] + inventory.people[Job::Fisher], numMilitaryBlds + 1u);
        buildingsWanted[BuildingType::Hunter] =
          std::min(inventory.goods[GoodType::Bow] + inventory.people[Job::Hunter], 4u);

        // quarry: low ware games start at 2 otherwise build as many as we have stonemasons, higher ware games up to 6
        // quarries
        if(inventory.goods[GoodType::PickAxe] + inventory.people[Job::Miner] < 7
           && inventory.people[Job::Stonemason] > 0 && inventory.people[Job::Miner] < 3)
        {
            buildingsWanted[BuildingType::Quarry] =
              std::max(std::min(inventory.people[Job::Stonemason], numMilitaryBlds), 2u);
        } else
        {
            //>6miners = build up to 6 depending on resources, else max out at miners/2
            if(inventory.people[Job::Miner] > 6)
                buildingsWanted[BuildingType::Quarry] =
                  std::min(inventory.goods[GoodType::PickAxe] + inventory.people[Job::Stonemason], 6u);
            else
                buildingsWanted[BuildingType::Quarry] = inventory.people[Job::Miner] / 2;

            if(buildingsWanted[BuildingType::Quarry] > numMilitaryBlds)
                buildingsWanted[BuildingType::Quarry] = numMilitaryBlds;
        }


        // sawmills limited by woodcutters and carpenter+saws reduced by char burners minimum of 3
        int resourcelimit = inventory.people[Job::Carpenter] + inventory.goods[GoodType::Saw];
        buildingsWanted[BuildingType::Sawmill] =
          (GetNumBuildings(BuildingType::Storehouse) + GetNumBuildings(BuildingType::HarborBuilding) + 4);
        unsigned max_available_carpenters = inventory.goods[GoodType::Saw] + inventory.people[Job::Carpenter];

        if((15 > aijh.AmountInStorage(GoodType::Boards)) && 1 > GetNumBuildingSites(BuildingType::Sawmill))
        {
            if((15 < aijh.AmountInStorage(GoodType::Wood)))
                buildingsWanted[BuildingType::Sawmill]++;
        }
        buildingsWanted[BuildingType::Sawmill] =
          std::min(max_available_carpenters, buildingsWanted[BuildingType::Sawmill]);


        // iron smelters limited by iron mines or crucibles
        int available_mines = static_cast<int>(GetNumBuildings(BuildingType::IronMine))
                                  - static_cast<int>(GetNumBuildingSites(BuildingType::IronMine)) - 1;
         available_mines = std::max(0, available_mines);
         buildingsWanted[BuildingType::Ironsmelter] =
              std::min<unsigned>(inventory.goods[GoodType::Crucible] + inventory.people[Job::IronFounder],
                                 static_cast<unsigned>(available_mines));
         if(2 > GetNumBuildings(BuildingType::Ironsmelter) && 1 > GetNumBuildingSites(BuildingType::Ironsmelter))
            buildingsWanted[BuildingType::Ironsmelter] = 2;
        if(1 > GetNumBuildings(BuildingType::Ironsmelter))
            buildingsWanted[BuildingType::Ironsmelter] = 1;


        available_mines = static_cast<int>(GetNumBuildings(BuildingType::GoldMine))
                              - static_cast<int>(GetNumBuildingSites(BuildingType::GoldMine)) - 1;
        available_mines = std::max(0, available_mines);
        buildingsWanted[BuildingType::Mint] =
          std::min<unsigned>(inventory.goods[GoodType::Crucible] + inventory.people[Job::Minter],
                             static_cast<unsigned>(available_mines));
        if(1 > GetNumBuildings(BuildingType::Mint))
            buildingsWanted[BuildingType::Mint] = 1;

        // armory count = smelter -metalworks if there is more than 1 smelter or 1 if there is just 1.
        buildingsWanted[BuildingType::Armory] =
          (GetNumBuildings(BuildingType::Ironsmelter) > 0) ?
            std::max(0, static_cast<int>(GetNumBuildings(BuildingType::Ironsmelter))
                          - static_cast<int>(GetNumBuildings(BuildingType::Metalworks))):
            GetNumBuildings(BuildingType::Ironsmelter);
        if(aijh.ggs.isEnabled(AddonId::HALF_COST_MIL_EQUIP))
            buildingsWanted[BuildingType::Armory] *= 2;
        // brewery count = 1+(armory/5) if there is at least 1 armory or armory /6 for exhaustible mines
        if(GetNumBuildings(BuildingType::Armory) > 0 && GetNumBuildings(BuildingType::Farm) > 0)
        {
            if(aijh.ggs.isEnabled(AddonId::INEXHAUSTIBLE_MINES))
                buildingsWanted[BuildingType::Brewery] = 1 + GetNumBuildings(BuildingType::Armory) / 5;
            else
                buildingsWanted[BuildingType::Brewery] = 1 + GetNumBuildings(BuildingType::Armory) / 6;
        } else
            buildingsWanted[BuildingType::Brewery] = 0;
        // metalworks is 1 if there is at least 1 smelter, 2 if mines are inexhaustible and we have at least 4 iron
        // smelters
        buildingsWanted[BuildingType::Metalworks] =
          ((GetNumBuildings(BuildingType::Ironsmelter) > 0) ? 1 : 0);

        // max processing
        unsigned foodusers = GetNumBuildings(BuildingType::Charburner) + GetNumBuildings(BuildingType::Mill)
                             + GetNumBuildings(BuildingType::Brewery) + GetNumBuildings(BuildingType::PigFarm)
                             + GetNumBuildings(BuildingType::DonkeyBreeder);
        unsigned freeFarms = std::max(int(GetNumBuildings(BuildingType::Farm) - (foodusers - GetNumBuildings(BuildingType::Mill))), 0);

        if(GetNumBuildings(BuildingType::Farm) >= foodusers - GetNumBuildings(BuildingType::Mill))

            buildingsWanted[BuildingType::Mill] =
              std::min(freeFarms,
                       GetNumBuildings(BuildingType::Bakery) + 1);

        else buildingsWanted[BuildingType::Mill] = GetNumBuildings(BuildingType::Mill);

        resourcelimit = inventory.people[Job::Baker] + inventory.goods[GoodType::Rollingpin] + 1;
        buildingsWanted[BuildingType::Bakery] = std::min<unsigned>(GetNumBuildings(BuildingType::Mill), resourcelimit);

        buildingsWanted[BuildingType::PigFarm] = (GetNumBuildings(BuildingType::Farm) < 8) ?
                                                   GetNumBuildings(BuildingType::Farm) / 4 :
                                                   (GetNumBuildings(BuildingType::Farm) - 2) / 4;
        if(buildingsWanted[BuildingType::PigFarm] > GetNumBuildings(BuildingType::Slaughterhouse) + 1)
            buildingsWanted[BuildingType::PigFarm] = GetNumBuildings(BuildingType::Slaughterhouse) + 1;
        buildingsWanted[BuildingType::Slaughterhouse] =
          std::min(inventory.goods[GoodType::Cleaver] + inventory[Job::Butcher],
                   GetNumBuildings(BuildingType::PigFarm));

        buildingsWanted[BuildingType::Well] =
          buildingsWanted[BuildingType::Bakery] + buildingsWanted[BuildingType::PigFarm]
          + buildingsWanted[BuildingType::DonkeyBreeder] + buildingsWanted[BuildingType::Brewery];


        buildingsWanted[BuildingType::Farm] = foodusers + GetNumBuildings(BuildingType::Storehouse) + 2;

        if((8 > aijh.AmountInStorage(GoodType::Grain) && 2 > GetNumBuildingSites(BuildingType::Farm))
           && GetNumBuildings(BuildingType::Sawmill) > 2)
            buildingsWanted[BuildingType::Farm]++;

        unsigned max_available_farmers = inventory.goods[GoodType::Scythe] + inventory[Job::Farmer];
        buildingsWanted[BuildingType::Farm] =
          std::min(max_available_farmers,
                   buildingsWanted[BuildingType::Farm]);

        if(inventory.goods[GoodType::PickAxe] + inventory.people[Job::Miner] < 3)
        {
            // almost out of new pickaxes and miners - emergency program: get coal,iron,smelter&metalworks
            buildingsWanted[BuildingType::CoalMine] = 1;
            buildingsWanted[BuildingType::IronMine] = 1;
            buildingsWanted[BuildingType::GoldMine] = 0;
            buildingsWanted[BuildingType::Ironsmelter] = 1;
            buildingsWanted[BuildingType::Metalworks] = 1;
            buildingsWanted[BuildingType::Armory] = 0;
            buildingsWanted[BuildingType::GraniteMine] = 0;
            buildingsWanted[BuildingType::Mint] = 0;
        } else // more than 2 miners
        {
            // coal mine count now depends on iron & gold not linked to food or material supply - might have to add a
            // material check if this makes problems
            if(GetNumBuildings(BuildingType::IronMine) > 0)
                buildingsWanted[BuildingType::CoalMine] =
                  GetNumBuildings(BuildingType::IronMine) * 2 + GetNumBuildings(BuildingType::GoldMine) + 1;
            else
                buildingsWanted[BuildingType::CoalMine] = std::max(GetNumBuildings(BuildingType::GoldMine), 2u);
            // more mines planned than food available? -> limit mines
            if(buildingsWanted[BuildingType::CoalMine] > 2
               && buildingsWanted[BuildingType::CoalMine] * 2
                    > GetNumBuildings(BuildingType::Farm) + GetNumBuildings(BuildingType::Fishery) + 1)

                buildingsWanted[BuildingType::CoalMine] =
                  (GetNumBuildings(BuildingType::Farm) + GetNumBuildings(BuildingType::Fishery)) / 2 + 2;
            if(GetNumBuildings(BuildingType::Farm) > 7) // quite the empire just scale mines with farms
            {
                if(aijh.ggs.isEnabled(
                     AddonId::INEXHAUSTIBLE_MINES)) // inexhaustible mines? -> more farms required for each mine
                    buildingsWanted[BuildingType::IronMine] = std::min(GetNumBuildings(BuildingType::Ironsmelter) + 1,
                                                                       GetNumBuildings(BuildingType::Farm) * 2 / 5);
                else
                    buildingsWanted[BuildingType::IronMine] =
                      std::min(GetNumBuildings(BuildingType::Farm) / 2, GetNumBuildings(BuildingType::Ironsmelter) + 2);
                buildingsWanted[BuildingType::GoldMine] =
                  (GetNumBuildings(BuildingType::Mint) > 0) ?
                    GetNumBuildings(BuildingType::Ironsmelter) > 6 && GetNumBuildings(BuildingType::Mint) > 1 ?
                    GetNumBuildings(BuildingType::Ironsmelter) > 10 ? 4 : 3 :
                    2 :
                    1;
                //buildingsWanted[BuildingType::GoldMine]++;
                buildingsWanted[BuildingType::DonkeyBreeder] = 1;
                if(aijh.ggs.isEnabled(AddonId::CHARBURNER))
                {
                    resourcelimit = inventory.people[Job::CharBurner] + inventory.goods[GoodType::Shovel];
                    if (aijh.AmountInStorage(GoodType::IronOre) * 2 + aijh.AmountInStorage(GoodType::Gold) > 15)
                    {
                        if (aijh.AmountInStorage(GoodType::IronOre) * 2 + aijh.AmountInStorage(GoodType::Gold)
                           > aijh.AmountInStorage(GoodType::Coal))
                        {
                            int ironGoldSurplus =
                              (aijh.AmountInStorage(GoodType::IronOre) * 2 + aijh.AmountInStorage(GoodType::Gold)
                               - aijh.AmountInStorage(GoodType::Coal));
                            unsigned charburnerPerMissingCoal = 8;
                            buildingsWanted[BuildingType::Charburner] = std::max(
                              int(ironGoldSurplus / charburnerPerMissingCoal - GetNumBuildingSites(BuildingType::CoalMine)), 0);
                            buildingsWanted[BuildingType::Charburner] = std::min(
                              int(buildingsWanted[BuildingType::Charburner]), resourcelimit);
                        }
                    }
                }
            } else
            {
                // probably still limited in food supply go up to 4 coal 1 gold 2 iron (gold+coal->coin,
                // iron+coal->tool, iron+coal+coal->weapon)
                unsigned numFoodProducers =
                  GetNumBuildings(BuildingType::Bakery) + GetNumBuildings(BuildingType::Slaughterhouse)
                  + GetNumBuildings(BuildingType::Hunter) + GetNumBuildings(BuildingType::Fishery);
                buildingsWanted[BuildingType::IronMine] =
                  (inventory.people[Job::Miner] + inventory.goods[GoodType::PickAxe]
                     > GetNumBuildings(BuildingType::CoalMine) + GetNumBuildings(BuildingType::GoldMine) + 1
                   && numFoodProducers > 4) ?
                    2 :
                    1;
                //buildingsWanted[BuildingType::IronMine]++;
                buildingsWanted[BuildingType::GoldMine] = (inventory.people[Job::Miner] > 2) ? 1 : 0;
                //buildingsWanted[BuildingType::GoldMine]++;
                resourcelimit = inventory.people[Job::CharBurner] + inventory.goods[GoodType::Shovel];
                if(aijh.ggs.isEnabled(AddonId::CHARBURNER) && GetNumBuildings(BuildingType::Farm) > 2)
                {
                    resourcelimit = inventory.people[Job::CharBurner] + inventory.goods[GoodType::Shovel] + 1;
                    if(aijh.AmountInStorage(GoodType::IronOre) * 2 + aijh.AmountInStorage(GoodType::Gold) > 16)
                    {
                        if(aijh.AmountInStorage(GoodType::IronOre) * 2 + aijh.AmountInStorage(GoodType::Gold)
                           > aijh.AmountInStorage(GoodType::Coal))
                        {
                            int ironGoldSurplus =
                              (aijh.AmountInStorage(GoodType::IronOre) * 2 + aijh.AmountInStorage(GoodType::Gold)
                               - aijh.AmountInStorage(GoodType::Coal));
                            unsigned charburnerPerMissingCoal = 8;
                            buildingsWanted[BuildingType::Charburner] =
                              std::max(int(ironGoldSurplus / charburnerPerMissingCoal - GetNumBuildingSites(BuildingType::CoalMine)), 0);
                        }
                    }
                }
            }
            if(GetNumBuildings(BuildingType::Quarry) + 1 < buildingsWanted[BuildingType::Quarry]
               && aijh.AmountInStorage(GoodType::Stones) < 100) // no quarry and low stones -> try granitemines.
            {
                if(inventory.people[Job::Miner] > 6)
                {
                    buildingsWanted[BuildingType::GraniteMine] = std::min<int>(
                      numMilitaryBlds / 15 + 1, // limit granite mines to military / 15 + 1
                      std::max<int>(buildingsWanted[BuildingType::Quarry] - GetNumBuildings(BuildingType::Quarry), 1));
                } else
                    buildingsWanted[BuildingType::GraniteMine] = 1;
            } else
                buildingsWanted[BuildingType::GraniteMine] = 0;
        }

        // Only build catapults if we have more than 5 military blds and 50 stones. Then reserve 4 stones per catapult
        // but do not build more than 4 additions catapults Note: This is a max amount. A catapult is only placed if
        // reasonable
        buildingsWanted[BuildingType::Catapult] =
          numMilitaryBlds <= 5 || inventory.goods[GoodType::Stones] < 50 ?
            0 :
            std::min((inventory.goods[GoodType::Stones] - 50) / 3, GetNumBuildings(BuildingType::Catapult) + 8);
        buildingsWanted[BuildingType::Catapult] = 0;
    }

    if(aijh.ggs.GetMaxMilitaryRank() == 0)
    {
        buildingsWanted[BuildingType::GoldMine] = 0; // max rank is 0 = private / recruit ==> gold is useless!
    }
}

int BuildingPlanner::GetNumAdditionalBuildingsWanted(BuildingType type) const
{
    return static_cast<int>(buildingsWanted[type]) - static_cast<int>(GetNumBuildings(type));
}

bool BuildingPlanner::WantMoreMilitaryBlds(const AIPlayerJH& aijh) const
{
    if(GetNumMilitaryBldSites() >= GetNumMilitaryBlds() + 3)
        return false;
    if(expansionRequired)
        return true;
    if(GetNumBuildings(BuildingType::Sawmill) - GetNumBuildingSites(BuildingType::Sawmill) > 0)
        return true;
    if(aijh.player.GetInventory().goods[GoodType::Boards] > 30 && GetNumBuildingSites(BuildingType::Sawmill) > 0)
        return true;
    return (GetNumMilitaryBlds() + GetNumMilitaryBldSites() > 0);
}

} // namespace AIJH
