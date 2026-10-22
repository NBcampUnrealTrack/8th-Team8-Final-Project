#pragma once

#include "CoreMinimal.h"
#include "SFPVEAchievement.generated.h"

/** A stage-clear achievement. Completion is derived from the persisted PvE campaign. */
USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFPVEAchievement
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FName StageRowName;
    UPROPERTY(BlueprintReadOnly) int32 StageOrder = 0;
    UPROPERTY(BlueprintReadOnly) FText Title;
    UPROPERTY(BlueprintReadOnly) FText Description;
    UPROPERTY(BlueprintReadOnly) bool bUnlocked = false;
};
