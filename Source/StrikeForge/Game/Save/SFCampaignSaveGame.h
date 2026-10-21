#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SFCampaignSaveGame.generated.h"

/** Persistent PvE campaign progress, independent of the current battle. */
UCLASS()
class STRIKEFORGE_API USFCampaignSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    static constexpr int32 CurrentVersion = 1;

    UPROPERTY(SaveGame)
    int32 SaveVersion = CurrentVersion;

    UPROPERTY(SaveGame)
    int32 HighestClearedStageOrder = 0;

    UPROPERTY(SaveGame)
    bool bUnreadStoryUnlock = false;
};
