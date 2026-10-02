#if WITH_DEV_AUTOMATION_TESTS
#include "HAL/IConsoleManager.h"
#include "EngineUtils.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Buildings/Building/HouseBuilding/Nexus/SFNexusBuilding.h"
#include "Game/Defence/SFDefenceWaveController.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "Game/GameState/SFGameState.h"
#include "InGame/HUD/Tutorial/SFStageIntroWidget.h"
#include "InGame/HUD/Tutorial/SFTutorialSubsystem.h"
#include "Units/Unit/UnitBase.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "TimerManager.h"

// Read-only telemetry for a real one-player PIE session. No combat or resource bonuses.
static FAutoConsoleCommandWithWorldAndArgs SFDefenceSoloPreview(
    TEXT("SF.Defence.Solo"), TEXT("status/monitor: record solo telemetry; tutorial_end: jump to the normal tutorial End node for a post-tutorial playtest."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
    {
        if (!World || World->WorldType != EWorldType::PIE || Args.IsEmpty()) return;
        if (Args[0] == TEXT("monitor"))
        {
            FTimerHandle Timer;
            World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(World, [World]
            {
                IConsoleManager::Get().ProcessUserConsoleInput(TEXT("SF.Defence.Solo status"), *GLog, World);
            }), 10.f, true, 0.1f);
            return;
        }
        if (Args[0] == TEXT("tutorial_end"))
        {
            UDataTable* Intro = LoadObject<UDataTable>(nullptr, TEXT("/Game/Game/IN_UI/DT/DT_SFStageIntro.DT_SFStageIntro"));
            const FSFStageIntroDataRow* Row = Intro ? Intro->FindRow<FSFStageIntroDataRow>(TEXT("Stage03"), TEXT("SoloPreview")) : nullptr;
            TArray<UUserWidget*> Widgets;
            UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, USFStageIntroWidget::StaticClass(), false);
            if (!Row || Widgets.IsEmpty()) return;
            if (auto* Tutorial = USFTutorialSubsystem::Get(World)) { Tutorial->CancelGuide(); Tutorial->CancelObjectiveWait(); }
            for (int32 Index = 0; Index < Row->Nodes.Num(); ++Index)
                if (Row->Nodes[Index].NodeType == ESFStageIntroNodeType::End)
                {
                    CastChecked<USFStageIntroWidget>(Widgets[0])->StartStageIntroAt(Intro, TEXT("Stage03"), Index);
                    UE_LOG(LogTemp, Display, TEXT("SOLO_QA tutorial ended via real End node; no starter units or resources granted"));
                    return;
                }
        }
        if (Args[0] != TEXT("status")) return;
        ASFGameState* State = World->GetGameState<ASFGameState>();
        if (!State) return;
        ASFDefenceWaveController* Wave = nullptr;
        for (TActorIterator<ASFDefenceWaveController> It(World); It; ++It) { Wave = *It; break; }
        int32 Players = 0, Friendly = 0, Enemy = 0;
        for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
            if (It->IsValid() && It->Get()->IsLocalPlayerController()) ++Players;
        for (TActorIterator<AUnitBase> It(World); It; ++It)
            if (It->GetCombatState() != EUnitCombatState::Dead)
            {
                if (It->GetTeamIndex() == 0) ++Friendly;
                else if (It->GetTeamIndex() == 1) ++Enemy;
            }
        FString Nexus;
        for (TActorIterator<ASFNexusBuilding> It(World); It; ++It)
            if (It->GetTeamIndex() == 0)
            {
                const FSFSelectableInfo Info = ISFSelectable::Execute_GetSelectableInfo(*It);
                Nexus = FString::Printf(TEXT("%.0f/%.0f"), Info.CurrentHealth, Info.MaximumHealth);
            }
        const FString Line = FString::Printf(TEXT("SOLO_QA time=%.1f players=%d wave=%d phase=%d planned_left=%d friendly=%d enemy=%d nexus=%s mineral=%d ether=%d result=%d\n"),
            World->GetTimeSeconds(), Players, Wave ? Wave->GetWaveNumber() : -1, Wave ? int32(Wave->GetPhase()) : -1,
            Wave ? Wave->GetRemainingSpawnCount() : -1, Friendly, Enemy, *Nexus, State->GetMineral(0), State->GetEther(0), int32(State->GetStageResult()));
        UE_LOG(LogTemp, Display, TEXT("%s"), *Line);
        FFileHelper::SaveStringToFile(Line, *(FPaths::ProjectSavedDir() / TEXT("DefenceSoloPlaytest.log")),
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
    }));
#endif
