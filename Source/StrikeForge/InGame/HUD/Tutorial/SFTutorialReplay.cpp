#include "SFTutorialReplay.h"

#include "Buildings/Building/SFBaseBuilding.h"
#include "Datas/Building/SFBuildingCatalog.h"
#include "Datas/Building/BuildingDefinition/SFProductionBuildingDefinition.h"
#include "EngineUtils.h"
#include "Game/GameState/SFGameState.h"
#include "Game/Spawn/SFBuildingSpawnLibrary.h"
#include "Game/Subsystem/SFBuildingGridSubsystem.h"
#include "InGame/HUD/Tutorial/SFTutorialSubsystem.h"
#include "Logging/SFLog.h"
#include "Units/Unit/SFUnitSpawnLibrary.h"

FSFTutorialReplayPlan FSFTutorialReplayPlan::FromRow(const FSFStageIntroDataRow& Row)
{
    FSFTutorialReplayPlan Plan;
    TSet<int32> Visited;
    int32 Index = Row.StartNodeIndex;
    while (Row.Nodes.IsValidIndex(Index) && !Visited.Contains(Index))
    {
        Visited.Add(Index);
        const FSFStageIntroNode& Node = Row.Nodes[Index];
        const auto Record = [&Plan](FGameplayTag Tag)
        {
            if (!Tag.IsValid()) return;
            Plan.CompletedObjectives.AddTag(Tag);
            if (Tag.ToString() == TEXT("Tutorial.TierUp.Nexus"))
                ++Plan.Upgrades.FindOrAdd(ESFBuildingType::Nexus);
            else if (Tag.ToString() == TEXT("Tutorial.TierUp.Barrack"))
                ++Plan.Upgrades.FindOrAdd(ESFBuildingType::Barracks);
            else if (Tag.MatchesTag(TAG_SF_Tutorial_Unit_Produced))
                Plan.Units.Add(Tag);
        };

        if (Node.NodeType == ESFStageIntroNodeType::InteractiveGuide)
        {
            // ObjectiveTag on an interactive node is legacy metadata, not another action.
            for (const FSFStageIntroGuideStep& Step : Node.GuideSteps)
            {
                if (Step.Completion != ESFGuideCompletion::ObjectiveTag) continue;
                Record(Step.CompletionTag);
                if (Step.bShowPlacementGhost && !Step.GhostBuildingDefinition.IsNull() &&
                    Step.CompletionTag.ToString().StartsWith(TEXT("Tutorial.Build.")))
                {
                    Plan.Buildings.Add(Step);
                }
            }
        }
        else if (Node.NodeType == ESFStageIntroNodeType::WaitForObjective ||
                 Node.NodeType == ESFStageIntroNodeType::End)
        {
            Record(Node.ObjectiveTag);
        }

        if (Node.NodeType == ESFStageIntroNodeType::End) break;
        // The affirmative branch is the full tutorial when a skip choice is authored.
        Index = Node.NodeType == ESFStageIntroNodeType::Choice ? Node.YesNodeIndex : Node.NextNodeIndex;
    }
    return Plan;
}

int32 FSFTutorialReplayPlan::GetInitialTier(ESFBuildingType Type, int32 AuthoredTier) const
{
    return AuthoredTier + Upgrades.FindRef(Type);
}

bool FSFTutorialReplayPlan::SpawnBuildings(UWorld* World, int32 TeamIndex) const
{
    ASFGameState* State = World->GetGameState<ASFGameState>();
    USFBuildingCatalog* Catalog = State ? State->GetTeamBuildingCatalog(TeamIndex) : nullptr;
    USFBuildingGridSubsystem* Grid = World->GetSubsystem<USFBuildingGridSubsystem>();
    if (!Catalog || !Grid) return Buildings.IsEmpty();

    for (const FSFStageIntroGuideStep& Step : Buildings)
    {
        const USFBuildingDefinition* Ghost = Step.GhostBuildingDefinition.LoadSynchronous();
        USFBuildingDefinition* Definition = Ghost ? Catalog->FindLoadedDefinitionByType(Ghost->BuildingType) : nullptr;
        if (!Definition) return false;
        // Use the same orientation and grid snapping as completed starting buildings.
        const FRotator Rotation = USFBuildingSpawnLibrary::GetConstructionRotation();
        FBuildingGridInfo GridInfo;
        FVector Location;
        if (!Grid->TrySnapWorldCenterToGrid(Step.WorldLocation,
            USFBuildingGridSubsystem::GetRotatedGridSize(Definition->GridSize, Rotation), GridInfo, Location)) return false;

        // Repeated placement reminders and a pre-placed equivalent must not duplicate it.
        bool bAlreadyBuilt = false;
        for (TActorIterator<ASFBaseBuilding> It(World); It; ++It)
        {
            if (It->GetTeamIndex() == TeamIndex && It->GetBuildingDefinition() == Definition &&
                It->GetActorLocation().Equals(Location, 1.f))
            {
                bAlreadyBuilt = true;
                break;
            }
        }
        if (bAlreadyBuilt) continue;
        ASFBaseBuilding* Building = USFBuildingSpawnLibrary::SpawnBuildingFromDefinition(World, Definition,
            TeamIndex, FTransform(Rotation, Location), ESFBuildingSpawnMode::ViewPortPlacedMode,
            GetInitialTier(Definition->BuildingType, Step.GhostTierIndex));
        if (!Building || !Building->IsTierActivated()) return false;
    }
    return true;
}

bool FSFTutorialReplayPlan::SpawnUnits(UWorld* World, int32 TeamIndex) const
{
    for (const FGameplayTag Tag : Units)
    {
        const ESFBuildingType Type = Tag == TAG_SF_Tutorial_Unit_Produced_SpecialWarBeast
            ? ESFBuildingType::SpecialProduction : ESFBuildingType::Barracks;
        ASFBaseBuilding* Producer = nullptr;
        FName UnitName = USFTutorialSubsystem::GetProductionGuideUnit(Tag);
        for (TActorIterator<ASFBaseBuilding> It(World); It; ++It)
        {
            const USFProductionBuildingDefinition* Definition = Cast<USFProductionBuildingDefinition>(It->GetBuildingDefinition());
            if (It->GetTeamIndex() != TeamIndex || !Definition || Definition->BuildingType != Type) continue;
            const int32 Tier = Tag == TAG_SF_Tutorial_Unit_Produced_Tier2 ? 1 : 0;
            const FSFProductionBuildingTierData* Data = Definition->FindProductionTier(Tier);
            if (UnitName.IsNone() && Data && !Data->SpawnableUnits.IsEmpty()) UnitName = Data->SpawnableUnits[0];
            Producer = *It;
            break;
        }
        if (!Producer || UnitName.IsNone() || !USFUnitSpawnLibrary::SpawnUnitFromRow(Producer, UnitName))
        {
            SF_LOG(Error, "[TutorialReplay] Failed to create tutorial unit for %s", *Tag.ToString());
            return false;
        }
    }
    return true;
}

void FSFTutorialReplayPlan::Complete(UWorld* World) const
{
    USFTutorialSubsystem::SetStageTutorialActive(World, false);
    USFTutorialSubsystem::SetStageFeatureLocks(World, FGameplayTagContainer());
    if (ASFGameState* State = World->GetGameState<ASFGameState>()) State->ReleaseBuildingRestrictionsAfterTutorial();
    for (const FGameplayTag Tag : CompletedObjectives)
        USFTutorialSubsystem::NotifyObjective(World, Tag, true);
}
