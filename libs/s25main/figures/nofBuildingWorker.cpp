// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "nofBuildingWorker.h"
#include "EventManager.h"
#include "GamePlayer.h"
#include "Loader.h"
#include "RTTR_Assert.h"
#include "SerializedGameData.h"
#include "SoundManager.h"
#include "Ware.h"
#include "buildings/nobBaseWarehouse.h"
#include "buildings/nobUsual.h"
#include "civ/Content.h"
#include "civ/Research.h"
#include "civ/TechTree.h"
#include "civ/WareDistribution.h"
#include "helpers/MaxEnumValue.h"
#include "helpers/serializeEnums.h"
#include "random/Random.h"
#include "world/GameWorld.h"
#include "nodeObjs/noFlag.h"
#include "gameData/BuildingConsts.h"
#include "gameData/JobConsts.h"
#include "gameData/ShieldConsts.h"
#include <algorithm>

nofBuildingWorker::nofBuildingWorker(const Job job, const MapPoint pos, const unsigned char player, nobUsual* workplace)
    : noFigure(job, pos, player, workplace), state(State::FigureWork), workplace(workplace), was_sounding(false)
{
    RTTR_Assert(dynamic_cast<nobUsual*>(static_cast<GameObject*>(
      workplace))); // Assume we have at least a GameObject and check if it is a valid workplace
}

void nofBuildingWorker::Serialize(SerializedGameData& sgd) const
{
    noFigure::Serialize(sgd);

    sgd.PushEnum<uint8_t>(state);

    if(fs != FigureState::GoHome && fs != FigureState::Wander)
    {
        sgd.PushObject(workplace);
        sgd.PushOptionalEnum<uint16_t>(ware);
        sgd.PushBool(was_sounding);
    }
    sgd.PushUnsignedInt(static_cast<unsigned>(carriedTools_.size()));
    for(const GoodType tool : carriedTools_)
        helpers::pushEnum<uint16_t>(sgd, tool);
    sgd.PushUnsignedInt(static_cast<unsigned>(carryList_.size()));
    for(const GoodType good : carryList_)
        helpers::pushEnum<uint16_t>(sgd, good);
}

void nofBuildingWorker::TakeTools(std::vector<GoodType> tools)
{
    carriedTools_ = std::move(tools);
}

std::vector<GoodType> nofBuildingWorker::ReleaseTools()
{
    std::vector<GoodType> result;
    result.swap(carriedTools_);
    return result;
}

nofBuildingWorker::nofBuildingWorker(SerializedGameData& sgd, const unsigned obj_id)
    : noFigure(sgd, obj_id), state(sgd.Pop<State>())
{
    if(fs != FigureState::GoHome && fs != FigureState::Wander)
    {
        workplace = sgd.PopObject<nobUsual>();
        if(sgd.GetGameDataVersion() < 5)
        {
            const auto iWare = sgd.PopUnsignedChar();
            // GoodType::Nothing is moved because of adding new wares due addons
            // The old GoodType::Nothing is now GoodType::Grapes
            if(iWare == rttr::enum_cast(GoodType::Grapes))
                ware = std::nullopt;
            else
                ware = GoodType(iWare);
        } else
            ware = sgd.PopOptionalEnum<GoodType>();
        was_sounding = sgd.PopBool();
    } else
    {
        workplace = nullptr;
        ware = std::nullopt;
        was_sounding = false;
    }
    if(sgd.GetGameDataVersion() >= 22)
    {
        carriedTools_.resize(sgd.PopUnsignedInt());
        for(GoodType& tool : carriedTools_)
            tool = helpers::popEnum<GoodType>(sgd);
    }
    if(sgd.GetGameDataVersion() >= 24)
    {
        carryList_.resize(sgd.PopUnsignedInt());
        for(GoodType& good : carryList_)
            good = helpers::popEnum<GoodType>(sgd);
    }
}

void nofBuildingWorker::ProducedWare()
{
    carryList_.clear();
    if(!ware || !workplace)
        return;
    const unsigned numProducts = workplace->TakeNumProducts(*ware);
    if(numProducts == 0)
    {
        // The saved work steps of the ware do not suffice yet (output ratio below 1, ware costing several steps)
        ware = std::nullopt;
        return;
    }
    const std::vector<GoodType> byproducts = civ::getByproducts(*world, player, workplace->GetBuildingType(), *ware);
    if(numProducts == 1 && byproducts.empty())
        return;
    // Every product with its byproducts
    for(unsigned i = 0; i < numProducts; ++i)
    {
        carryList_.push_back(*ware);
        carryList_.insert(carryList_.end(), byproducts.begin(), byproducts.end());
    }
}

void nofBuildingWorker::AbrogateWorkplace()
{
    if(workplace)
    {
        workplace->WorkerLost();
        workplace = nullptr;
    }
}

void nofBuildingWorker::Draw(DrawPoint drawPt)
{
    switch(state)
    {
        case State::FigureWork:
        case State::HunterChasing:
        case State::HunterWalkingToCadaver:
        case State::HunterFindingShootingpoint:
        case State::SkinnerWalkingToCarcass: DrawWalking(drawPt); break;
        case State::Work:
        case State::HunterShooting:
        case State::HunterEviscerating:
        case State::HunterWaitingForAnimalReady:
        case State::SkinnerSkinningCarcass:
        case State::CatapultTargetBuilding:
        case State::CatapultBackoff: DrawWorking(drawPt); break;
        case State::CarryoutWare: DrawWalkingWithWare(drawPt); break;
        case State::WalkingHome:
        case State::EnterBuilding:
            if(ware)
                DrawWalkingWithWare(drawPt);
            else
                DrawWalking(drawPt);
            break;
        default: DrawOtherStates(drawPt); break;
    }
}

void nofBuildingWorker::Walked()
{
    switch(state)
    {
        case State::EnterBuilding:
        {
            // Hab ich noch ne Ware in der Hand?

            if(ware)
            {
                // dann war draußen kein Platz --> ist jetzt evtl Platz?
                state = State::WaitForWareSpace;
                if(HasSpaceForWaresAtFlag())
                    FreePlaceAtFlag();
                // Ab jetzt warten, d.h. nicht mehr arbeiten --> schlecht für die Produktivität
                workplace->StartNotWorking();
            } else
            {
                // Anfangen zu Arbeiten
                TryToWork();
            }
        }
        break;
        case State::CarryoutWare:
        {
            // Alles weitere übernimmt nofBuildingWorker
            WorkingReady();
        }
        break;
        default: WalkedDerived();
    }
}

void nofBuildingWorker::WorkingReady()
{
    // wir arbeiten nicht mehr
    workplace->is_working = false;

    // Trage ich eine Ware?
    if(ware)
    {
        // Ist noch Platz an der Fahne (auch für evtl. Nebenprodukte und weitere Produkte)?
        // Sonst wird die Ware wieder mit hinein genommen und auf Platz gewartet.
        // Mehr als eine volle Fahne wird nacheinander abgelegt
        std::vector<GoodType> waresToPut = GetWaresToCarryOut();
        const unsigned numNow = GetNumWaresToPutNow(waresToPut.size());
        if(HasSpaceForWaresAtFlag())
        {
            for(unsigned i = 0; i < numNow; ++i)
                PutWareAtFlag(waresToPut[i]);
            waresToPut.erase(waresToPut.begin(), waresToPut.begin() + numNow);
            if(waresToPut.empty())
            {
                // Tragen nun keine Ware mehr
                ware = std::nullopt;
                carryList_.clear();
            } else
            {
                carryList_ = std::move(waresToPut);
                ware = carryList_.front();
            }
        }
    }

    // Wieder reingehen
    StartWalking(Direction::NorthWest);
    state = State::EnterBuilding;
}

unsigned nofBuildingWorker::GetNumWaresToPutNow(const std::size_t numWares)
{
    constexpr std::size_t flagCapacity = 8;
    return static_cast<unsigned>(std::max<std::size_t>(std::min(numWares, flagCapacity), 1u));
}

std::vector<GoodType> nofBuildingWorker::GetWaresToCarryOut() const
{
    std::vector<GoodType> result;
    if(!ware)
        return result;
    if(!carryList_.empty())
        return carryList_;
    result.push_back(*ware);
    // Nebenprodukte (Zivilisationsmodus, rttr:AddByproduct), z.B. Knochen beim Jäger
    const std::vector<GoodType> byproducts = civ::getByproducts(*world, player, workplace->GetBuildingType(), *ware);
    result.insert(result.end(), byproducts.begin(), byproducts.end());
    return result;
}

bool nofBuildingWorker::HasSpaceForWaresAtFlag() const
{
    // Civilization mode: places of the flag stay free for wares passing by (construction material), see
    // rttr:SetFlagWorkLimit
    const unsigned keptFree = 8u - civ::getActiveContent().GetFlagWorkLimit();
    return workplace->GetFlag()->GetNumFreeSpaces() >= GetNumWaresToPutNow(GetWaresToCarryOut().size()) + keptFree;
}

void nofBuildingWorker::PutWareAtFlag(const GoodType good)
{
    noFlag* flag = workplace->GetFlag();
    // Ware erzeugen
    auto real_ware = std::make_unique<Ware>(good, nullptr, flag);
    real_ware->WaitAtFlag(flag);
    // Inventur entsprechend erhöhen, dabei Schilder unterscheiden!
    GoodType ware_type = ConvertShields(real_ware->type);
    world->GetPlayer(player).IncreaseInventoryWare(ware_type, 1, WareSource::Production);
    world->GetPlayer(player).CountProducedBy(workplace->GetBuildingType(), ware_type);
    // Abnehmer für Ware finden
    real_ware->SetGoal(world->GetPlayer(player).FindClientForWare(*real_ware));
    // Ware soll ihren weiteren Weg berechnen
    real_ware->RecalcRoute();
    // Ware ablegen
    flag->AddWare(std::move(real_ware));
    // Warenstatistik erhöhen
    world->GetPlayer(this->player).IncreaseMerchandiseStatistic(ware_type);
}

void nofBuildingWorker::TryToWork()
{
    // Tool reform (civilization mode): Without the tools the building may not work at all, with them the tools wear
    // out with every work step and set its speed
    if(!workplace->IsProductionStopped() && AreWaresAvailable() && workplace->BeginWorkStep())
    {
        // Civilization mode: an order of its product is processed when the step starts (several products: when chosen)
        civ::startedWorkStep(*world, *workplace);
        state = State::Waiting1;
        // Work outside the building keeps its duration (it fits to animations and objects like falling trees), so
        // the waiting time is changed for both
        const unsigned outsideWork = GetOutsideWorkLength();
        const unsigned scaledLength = workplace->ScaleWorkDuration(
          civ::getActiveContent().GetWorkTimes(workplace->GetBuildingType()).wait + outsideWork);
        current_ev = GetEvMgr().AddEvent(this,
                                         (GetGOT() == GO_Type::NofCatapultman) ?
                                           CATAPULT_WAIT1_LENGTH :
                                           std::max(scaledLength, outsideWork + 1u) - outsideWork,
                                         1);
        workplace->StopNotWorking();
    } else
    {
        state = State::WaitingForWaresOrProductionStopped;
        // Nun arbeite ich nich mehr
        workplace->StartNotWorking();
    }
}

bool nofBuildingWorker::AreWaresAvailable() const
{
    return workplace->WaresAvailable();
}

void nofBuildingWorker::GotWareOrProductionAllowed()
{
    // Falls man auf Waren wartet, kann man dann anfangen zu arbeiten
    if(state == State::WaitingForWaresOrProductionStopped)
    {
        // anfangen zu arbeiten
        TryToWork();
    }
}

void nofBuildingWorker::GoalReached()
{
    // Tür zumachen (ist immer bis zu Erstbesetzung offen)
    workplace->CloseDoor();
    // Gebäude Bescheid sagen, dass ich da bin
    workplace->WorkerArrived();

    WorkplaceReached();

    // ggf. anfangen zu arbeiten
    TryToWork();
}

bool nofBuildingWorker::FreePlaceAtFlag()
{
    // Hinaus gehen, um Ware abzulegen, falls wir auf einen freien Platz warten
    // (mit Nebenprodukten müssen genug Plätze für alle frei sein)
    if(state == State::WaitForWareSpace && HasSpaceForWaresAtFlag())
    {
        StartWalking(Direction::SouthEast);
        state = State::CarryoutWare;
        return true;
    } else
        return false;
}
void nofBuildingWorker::LostWork()
{
    switch(state)
    {
        default: break;
        case State::FigureWork:
        {
            // Auf Wegen nach Hause gehen
            GoHome();
        }
        break;
        case State::Waiting1:
        case State::Waiting2:
        case State::Work:
        case State::WaitingForWaresOrProductionStopped:
        case State::WaitForWareSpace:
        case State::HunterShooting:
        case State::HunterEviscerating:
        case State::HunterWaitingForAnimalReady:
        case State::SkinnerSkinningCarcass:
        case State::CatapultTargetBuilding:
        case State::CatapultBackoff:
        {
            // Bisheriges Event abmelden, da die Arbeit unterbrochen wird
            GetEvMgr().RemoveEvent(current_ev);

            // Bescheid sagen, dass Arbeit abgebrochen wurde
            WorkAborted();

            // Rumirren
            StartWandering();
            Wander();

            // Evtl. Sounds löschen
            world->GetSoundMgr().stopSounds(*this);

            state = State::FigureWork;
        }
        break;
        case State::EnterBuilding:
        case State::CarryoutWare:
        case State::WalkToWorkpoint:
        case State::WalkingHome:
        case State::HunterChasing:
        case State::HunterFindingShootingpoint:
        case State::HunterWalkingToCadaver:
        case State::SkinnerWalkingToCarcass:
        {
            // Bescheid sagen, dass Arbeit abgebrochen wurde
            WorkAborted();

            // Rumirren
            // Bei diesen States läuft man schon, darf also nicht noch zusätzlich Wander aufrufen, da man dann ja im
            // Laufen nochmal losläuft!
            StartWandering();

            // Evtl. Sounds löschen
            world->GetSoundMgr().stopSounds(*this);

            state = State::FigureWork;
        }
        break;
    }

    workplace = nullptr;
}

void nofBuildingWorker::ProductionStopped()
{
    // Wenn ich gerade warte und schon ein Arbeitsevent angemeldet habe, muss das wieder abgemeldet werden
    if(state == State::Waiting1)
    {
        GetEvMgr().RemoveEvent(current_ev);
        current_ev = nullptr;
        state = State::WaitingForWaresOrProductionStopped;
        workplace->StartNotWorking();
    }
}

void nofBuildingWorker::WorkAborted() {}

void nofBuildingWorker::WorkplaceReached() {}

/// Zeichnen der Figur in sonstigen Arbeitslagen
void nofBuildingWorker::DrawOtherStates(DrawPoint) {}

const civ::Products* nofBuildingWorker::GetCivProducts() const
{
    if(!workplace || !civ::isEnabled(*world))
        return nullptr;
    return civ::getActiveContent().GetProducts(workplace->GetBuildingType());
}

GoodType nofBuildingWorker::ChooseKnowledgeProduct(const civ::Products& products) const
{
    // Gewichtet nach dem Regler "Wissen" des Besitzers (synchroner Zufall des Spiels, alle Rechner wählen gleich).
    // Sind alle Gewichte der möglichen Produkte 0, sind alle gleich wahrscheinlich. Produkte, die mit den Werkzeugen
    // des Schritts keine Ware ergeben, sind nicht möglich (außer keines ergibt eine)
    std::vector<bool> possible;
    for(const GoodType good : products.productChoices)
        possible.push_back(workplace->GetStepOutputRatio(good) > 0);
    if(std::none_of(possible.begin(), possible.end(), [](const bool value) { return value; }))
        possible.assign(possible.size(), true);
    std::vector<unsigned> weights;
    const civ::PlayerResearch& research = world->GetPlayer(player).GetResearch();
    const auto distribution = research.GetKnowledgeDistribution(world->GetTechTree());
    for(unsigned i = 0; i < products.productChoiceTypes.size(); ++i)
        weights.push_back(possible[i] ? distribution[products.productChoiceTypes[i]] : 0u);
    unsigned totalWeight = 0;
    for(const unsigned weight : weights)
        totalWeight += weight;
    if(totalWeight == 0)
    {
        for(unsigned i = 0; i < weights.size(); ++i)
            weights[i] = possible[i] ? 1u : 0u;
        totalWeight = static_cast<unsigned>(std::count(possible.begin(), possible.end(), true));
    }
    unsigned pick = RANDOM_RAND(totalWeight);
    for(unsigned i = 0; i < weights.size(); ++i)
    {
        if(pick < weights[i])
            return products.productChoices[i];
        pick -= weights[i];
    }
    return products.productChoices.back();
}

GoodType nofBuildingWorker::ChooseProduct(const civ::Products& products)
{
    switch(products.productChoice)
    {
        case civ::ProductChoice::Distribution:
        {
            std::vector<GoodType> candidates;
            for(const GoodType good : products.productChoices)
            {
                if(workplace->GetStepOutputRatio(good) > 0)
                    candidates.push_back(good);
            }
            if(candidates.empty())
                candidates = products.productChoices;
            return civ::chooseProduct(*world, player, candidates, GetObjId(),
                                      [this](const GoodType good) { return workplace->GetNumMadeInStep(good); });
        }
        case civ::ProductChoice::Knowledge: return ChooseKnowledgeProduct(products);
        case civ::ProductChoice::None: break;
    }
    const auto consumed = workplace->GetLastConsumedWare();
    return products.GetProduct(consumed ? std::optional<GoodType>(*consumed) : std::nullopt);
}

void nofBuildingWorker::ChooseStepProduct(const bool beforeConsume)
{
    const civ::Products* products = GetCivProducts();
    if(!products)
        return;
    const bool choice = products->productChoice != civ::ProductChoice::None;
    if(beforeConsume)
    {
        // Mehrere Produkte: für jeden Arbeitsschritt neu gewählt, die angesparten Schritte zählen dabei nicht (Nutzer,
        // 2026-11-25). Bestellungen werden dabei abgehakt (civ::chooseProduct)
        workplace->SetChosenProduct(std::nullopt);
        if(products->HasProduct() && choice)
            workplace->SetChosenProduct(ChooseProduct(*products));
        return;
    }
    if(!products->HasProduct() || choice)
        return;
    // Ein Produkt: mit inheritProperties entscheidet die aus dem ersten Eingang verbrauchte Ware
    const GoodType product = ChooseProduct(*products);
    workplace->SetChosenProduct(product);
    // Mehrere Produkte nach den Eingängen: ihre Bestellungen jetzt (ein Produkt: beim Beginn des Schritts)
    if(products->GetAllProducts().size() > 1u)
        civ::processOrders(*world, player, product, workplace->GetNumMadeInStep(product));
}

helpers::OptionalEnum<GoodType> nofBuildingWorker::TakeStepProduct(const helpers::OptionalEnum<GoodType> original)
{
    const civ::Products* products = GetCivProducts();
    if(!products)
        return original;
    if(!products->HasProduct())
        return std::nullopt;
    const auto chosen = workplace->GetChosenProduct();
    workplace->SetChosenProduct(std::nullopt);
    GoodType product = chosen ? *chosen : ChooseProduct(*products);
    // Schilde: Die Produkte nennen das Schild der Römer, hergestellt wird das Schild des eigenen Volkes
    if(ConvertShields(product) == GoodType::ShieldRomans)
        product = SHIELD_TYPES[world->GetPlayer(player).nation];
    return product;
}

bool nofBuildingWorker::HasCarryGraphic(const GoodType good) const
{
    // Werkstätten tragen ihre Waren immer wie Träger (GetCarryID der Werkstatt-Arbeiter); eingebaute Gebäude haben
    // eine Grafik für das Produkt des Originalspiels
    if(!workplace || isCustomBuilding(workplace->GetBuildingType()))
        return true;
    const auto& produced = BUILTIN_BLD_WORK_DESC[static_cast<unsigned>(workplace->GetBuildingType())].producedWare;
    const auto* original = boost::variant2::get_if<GoodType>(&produced);
    return original && *original == good;
}

bool nofBuildingWorker::DrawCarryingOtherWare(const DrawPoint drawPt)
{
    if(!ware || HasCarryGraphic(*ware))
        return false;
    DrawWalkingCarrier(drawPt, *ware, JOB_SPRITE_CONSTS[job_].isFat());
    return true;
}

/// Zeichnet Figur beim Hereinlaufen/nach Hause laufen mit evtl. getragenen Waren
void nofBuildingWorker::DrawWalkingWithWare(DrawPoint drawPt)
{
    if(DrawCarryingOtherWare(drawPt))
        return;
    unsigned short id = GetCarryID();
    // >=CARRY_ID_CARRIER_OFFSET -> carrier.bob else jobs.bob!
    if(id >= CARRY_ID_CARRIER_OFFSET)
    {
        id -= CARRY_ID_CARRIER_OFFSET;
        RTTR_Assert(id <= helpers::MaxEnumValue_v<GoodType>);
        DrawWalkingCarrier(drawPt, GoodType(id), JOB_SPRITE_CONSTS[job_].isFat());
    } else
        DrawWalking(drawPt, LOADER.GetBob("jobs"), id, JOB_SPRITE_CONSTS[job_].isFat());
}
