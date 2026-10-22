#include "SFGameInstance.h"
#include "Game/GameMode/SFGameMode.h"
#include "UI/SFAchievementToastWidget.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

TArray<FSFPVEAchievement> USFGameInstance::GetPVEAchievements() const
{
    const FText Titles[] = {
        NSLOCTEXT("SFAchievements", "Stage01", "첫 승리의 깃발"),
        NSLOCTEXT("SFAchievements", "Stage02", "전진하는 전열"),
        NSLOCTEXT("SFAchievements", "Stage03", "흔들리지 않는 수호자"),
        NSLOCTEXT("SFAchievements", "Stage04", "포위를 넘어"),
        NSLOCTEXT("SFAchievements", "Stage05", "영웅의 증명"),
        NSLOCTEXT("SFAchievements", "Stage06", "승리를 향한 진군"),
        NSLOCTEXT("SFAchievements", "Stage07", "에테르의 정복자")
    };
    TArray<FSFPVEAchievement> Result;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Titles); ++Index)
    {
        FSFPVEAchievement& Entry = Result.AddDefaulted_GetRef();
        Entry.StageOrder = Index + 1;
        Entry.StageRowName = FName(*FString::Printf(TEXT("Stage%02d"), Entry.StageOrder));
        Entry.Title = Titles[Index];
        Entry.Description = FText::Format(NSLOCTEXT("SFAchievements", "ClearStage", "PvE 스테이지 {0} 클리어"), FText::AsNumber(Entry.StageOrder));
        // Campaign progression is sequential and already saved on victory. Keeping a
        // single source of truth avoids duplicate rewards and migrates old saves for free.
        Entry.bUnlocked = GetStageProgressState(Entry.StageRowName) == ESFStageProgressState::Cleared;
    }
    return Result;
}

void USFGameInstance::NotifyPVEAchievementUnlocked(FName StageRowName)
{
    const auto Entries = GetPVEAchievements();
    const auto* Entry = Entries.FindByPredicate([StageRowName](const FSFPVEAchievement& Value) { return Value.StageRowName == StageRowName; });
    if (!Entry || !Entry->bUnlocked) return;
    OnPVEAchievementUnlocked.Broadcast(*Entry);
    UWorld* World = GetWorld();
    APlayerController* PC = GetFirstLocalPlayerController();
    if (!World || World->GetNetMode() != NM_Standalone || !World->GetAuthGameMode<ASFGameMode>() || !PC || !PC->IsLocalController()) return;
    if (auto* Toast = CreateWidget<USFAchievementToastWidget>(PC))
    {
        Toast->InitializeAchievement(*Entry);
        Toast->AddToViewport(1000);
    }
}
