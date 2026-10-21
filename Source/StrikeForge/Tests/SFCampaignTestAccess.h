#pragma once

#if WITH_DEV_AUTOMATION_TESTS
#include "Game/GameInstance/SFGameInstance.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "Kismet/GameplayStatics.h"

struct FSFCampaignTestAccess
{
    static void UseSlot(USFGameInstance* GI, const FString& Slot) { GI->CampaignSaveSlotName = Slot; }
    static void DeleteSlot(const FString& Slot)
    {
        UGameplayStatics::DeleteGameInSlot(Slot, 0);
        UGameplayStatics::DeleteGameInSlot(Slot + TEXT("_Backup"), 0);
    }
    static void SetupInput(ASFInGamePlayerController* PC) { PC->SetupInputComponent(); }
    static void InstantVictory(ASFInGamePlayerController* PC) { PC->HandleInstantPVEVictory(); }
};
#endif
