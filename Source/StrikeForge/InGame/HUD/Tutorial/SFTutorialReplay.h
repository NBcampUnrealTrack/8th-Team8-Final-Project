#pragma once

#include "CoreMinimal.h"
#include "Datas/Building/SFBuildingType.h"
#include "InGame/HUD/Tutorial/SFStageIntroData.h"

/** Gameplay results of the authored tutorial path, without opening any widgets. */
struct STRIKEFORGE_API FSFTutorialReplayPlan
{
    TArray<FSFStageIntroGuideStep> Buildings;
    TMap<ESFBuildingType, int32> Upgrades;
    TArray<FGameplayTag> Units;
    FGameplayTagContainer CompletedObjectives;

    static FSFTutorialReplayPlan FromRow(const FSFStageIntroDataRow& Row);
    int32 GetInitialTier(ESFBuildingType Type, int32 AuthoredTier) const;
    bool SpawnBuildings(UWorld* World, int32 TeamIndex) const;
    bool SpawnUnits(UWorld* World, int32 TeamIndex) const;
    void Complete(UWorld* World) const;
};
