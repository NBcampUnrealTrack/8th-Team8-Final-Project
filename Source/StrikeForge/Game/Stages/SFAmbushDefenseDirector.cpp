#include "SFAmbushDefenseDirector.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/SceneComponent.h"
#include "Game/GameMode/SFGameMode.h"
#include "Game/GameState/SFGameState.h"
#include "Kismet/GameplayStatics.h"
#include "Units/Spawner/UnitSpawner.h"
#include "UI/LoadingWidget.h"
#include "Game/Defence/SFDefenceWaveController.h"
#include "EngineUtils.h"

ASFAmbushDefenseDirector::ASFAmbushDefenseDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = .1f;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    LoadingWidgetClass = TSoftClassPtr<ULoadingWidget>(FSoftObjectPath(TEXT("/Game/Core/UI/WBP_Loading.WBP_Loading_C")));
}

void ASFAmbushDefenseDirector::EnsureLoadingScreen()
{
    if (bLoadingScreenHandled) return;
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC || !PC->IsLocalController()) return;
    TArray<UUserWidget*> Existing;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Existing, ULoadingWidget::StaticClass(), true);
    for (UUserWidget* Widget : Existing)
    {
        if (IsValid(Widget) && Widget->IsInViewport())
        {
            bLoadingScreenHandled = true;
            return;
        }
    }
    UClass* LoadingClass = LoadingWidgetClass.LoadSynchronous();
    if (!ensure(LoadingClass)) return;
    LoadingScreen = CreateWidget<ULoadingWidget>(PC, LoadingClass);
    if (LoadingScreen)
    {
        // The standard screen starts the stage after assets, fade-out and camera
        // boundaries are ready. Do not bypass that lifecycle with StartStage().
        LoadingScreen->AddToViewport(1000);
        LoadingScreen->SetStageTitle(NSLOCTEXT("Ambush", "StageTitle", "끊어진 퇴로"));
        bLoadingScreenHandled = true;
    }
}

void ASFAmbushDefenseDirector::StopSpawners()
{
    for (AUnitSpawner* Spawner : Spawners) if (IsValid(Spawner)) Spawner->DeactivateSpawner();
}

void ASFAmbushDefenseDirector::SetBridgeBroken(bool bBroken)
{
    for (AActor* Actor : IntactBridge) if (IsValid(Actor))
    {
        Actor->SetActorHiddenInGame(bBroken); Actor->SetActorEnableCollision(!bBroken);
    }
    for (AActor* Actor : BrokenBridge) if (IsValid(Actor))
    {
        Actor->SetActorHiddenInGame(!bBroken); Actor->SetActorEnableCollision(bBroken);
    }
}

void ASFAmbushDefenseDirector::BeginPlay()
{
    Super::BeginPlay();
    PreparationSeconds = FMath::Max(0.f, PreparationSeconds);
    HoldSeconds = FMath::Max(1.f, HoldSeconds);
    StopSpawners(); SetBridgeBroken(false);
    EnsureLoadingScreen();
}

void ASFAmbushDefenseDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    ASFGameState* State = GetWorld()->GetGameState<ASFGameState>();
    if (!State) return;
    if (State->IsStageFinished())
    {
        StopSpawners();
        SetActorTickEnabled(false); return;
    }
    if (!State->IsStagePlaying())
    {
        EnsureLoadingScreen();
        return;
    }
    // A configured wave controller owns Stage06 timing and victory. Keep this
    // map's loading screen and bridge presentation without its old 4-minute win.
    for (TActorIterator<ASFDefenceWaveController> It(GetWorld()); It; ++It)
    {
        const bool bDefending = It->GetPhase() == ESFDefencePhase::Wave;
        const ESFAmbushPhase Next = bDefending ? ESFAmbushPhase::Defending : ESFAmbushPhase::Secured;
        if (Phase != Next) SetBridgeBroken(bDefending);
        Phase = Next;
        RemainingSeconds = It->GetRemainingSeconds();
        return;
    }
    if (Phase == ESFAmbushPhase::Waiting)
    {
        Phase = ESFAmbushPhase::Secured; RemainingSeconds = PreparationSeconds;

    }
    else
    {
        RemainingSeconds = FMath::Max(0.f, RemainingSeconds - DeltaSeconds);
        if (RemainingSeconds <= 0.f)
        {
            if (Phase == ESFAmbushPhase::Secured)
            {
                Phase = ESFAmbushPhase::Defending; RemainingSeconds = HoldSeconds;
                SetBridgeBroken(true);
                for (AUnitSpawner* Spawner : Spawners) if (IsValid(Spawner)) Spawner->ActivateSpawner();
            }
            else if (Phase == ESFAmbushPhase::Defending)
            {
                Phase = ESFAmbushPhase::Escaped; RemainingSeconds = 2.f;
                StopSpawners(); SetBridgeBroken(false);
            }
            else if (Phase == ESFAmbushPhase::Escaped)
            {
                if (ASFGameMode* Mode = GetWorld()->GetAuthGameMode<ASFGameMode>()) Mode->ProcessVictory();
            }
        }
    }
}

void ASFAmbushDefenseDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    StopSpawners();
    if (LoadingScreen) LoadingScreen->RemoveFromParent();
    Super::EndPlay(Reason);
}
