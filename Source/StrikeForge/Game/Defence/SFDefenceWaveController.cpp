#include "SFDefenceWaveController.h"
#include "Units/Spawner/UnitSpawner.h"
#include "Units/Unit/UnitBase.h"
#include "Game/GameState/SFGameState.h"
#include "Game/GameMode/SFGameMode.h"
#include "InGame/HUD/Tutorial/SFTutorialSubsystem.h"
#include "InGame/HUD/Defence/SFDefenceTimerWidget.h"
#include "Logging/SFLog.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

ASFDefenceWaveController::ASFDefenceWaveController()
{
    PrimaryActorTick.bCanEverTick = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ASFDefenceWaveController::BeginPlay()
{
    Super::BeginPlay();
    if (Spawners.Num() != 3 || UnitPool.IsEmpty())
    {
        SF_LOG(Error, "[Defence] Configure exactly three spawners and a normal unit pool.");
        StopWaves();
        return;
    }
    for (AUnitSpawner* Spawner : Spawners)
    {
        if (!IsValid(Spawner)) { StopWaves(); return; }
        Spawner->SetWaveControlled(true);
        Spawner->SetSpawningEnabled(false);
    }
    // Load once during tutorial/loading, never synchronously for every spawn.
    for (const FSFDefenceUnitEntry& Entry : UnitPool)
    {
        TSubclassOf<AUnitBase> Class = Entry.UnitClass.LoadSynchronous();
        if (!Class || Entry.RowName.IsNone())
        {
            SF_LOG(Error, "[Defence] Invalid unit pool entry: %s", *Entry.RowName.ToString());
            StopWaves();
            return;
        }
        LoadedClasses.Add(Class);
    }
    SpawnedPerDirection.Init(0, Spawners.Num());
}

void ASFDefenceWaveController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (Phase == ESFDefencePhase::Stopped) return;
    const ASFGameState* State = GetWorld()->GetGameState<ASFGameState>();
    if (!State) return;
    if (State->IsStageFinished()) { StopWaves(); return; }
    if (!State->IsStagePlaying()) return;
    const USFTutorialSubsystem* Tutorial = USFTutorialSubsystem::Get(this);
    if (Tutorial && Tutorial->IsStageTutorialActive()) return;

    if (Phase == ESFDefencePhase::WaitingForTutorial)
    {
        BeginPreparation();
        return;
    }
    RemainingSeconds = FMath::Max(0.f, RemainingSeconds - DeltaSeconds);
    if (RemainingSeconds <= 0.f)
    {
        if (Phase == ESFDefencePhase::Preparation) BeginWave();
        else CompleteWave();
        return;
    }
    if (Phase == ESFDefencePhase::Wave)
    {
        SpawnCountdown -= DeltaSeconds;
        if (SpawnCountdown <= 0.f)
        {
            SpawnBatch();
            SpawnCountdown = FMath::Max(0.1f, SpawnInterval);
        }
        if (GetRemainingSpawnCount() == 0)
        {
            const bool bCleared = !HasLivingEnemies();
            if ((bEndWaveOnEnemyClear && bCleared) ||
                (!bRequireFullWaveDuration && (WaveNumber < FMath::Max(1, TotalWaves) || bCleared)))
                CompleteWave();
        }
    }
}

void ASFDefenceWaveController::BeginPreparation()
{
    Phase = ESFDefencePhase::Preparation;
    RemainingSeconds = FMath::Max(1.f, WaveNumber == 0 ? InitialPreparationDuration : PreparationDuration);
    for (AUnitSpawner* Spawner : Spawners)
        if (IsValid(Spawner)) Spawner->SetSpawningEnabled(false);
    if (!TimerWidget)
    {
        if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
        {
            TimerWidget = CreateWidget<USFDefenceTimerWidget>(PC, USFDefenceTimerWidget::StaticClass());
            if (TimerWidget)
            {
                TimerWidget->Controller = this;
                TimerWidget->AddToViewport(50);
            }
        }
    }
    SF_LOG(Log, "[Defence] Preparing wave %d for %.0f seconds.", WaveNumber + 1, RemainingSeconds);
}

void ASFDefenceWaveController::BeginWave()
{
    if (WaveNumber >= FMath::Max(1, TotalWaves)) { CompleteWave(); return; }
    ++WaveNumber;
    Phase = ESFDefencePhase::Wave;
    RemainingSeconds = FMath::Max(1.f, WaveDuration);
    const int32 OriginalBudget = UnitPool.Num() * FMath::Max(1,
        FirstWaveCopiesPerType + (WaveNumber - 1) * AdditionalCopiesPerWave);
    TotalPlannedUnits = FMath::Max(1, FMath::RoundToInt(OriginalBudget * Spawners.Num() * SpawnCountMultiplier));
    PlannedPerDirection.Init(TotalPlannedUnits / Spawners.Num(), Spawners.Num());
    for (int32 Direction = 0; Direction < Spawners.Num(); ++Direction)
        if ((Direction + WaveNumber - 1) % Spawners.Num() < TotalPlannedUnits % Spawners.Num())
            ++PlannedPerDirection[Direction];
    BuildSpawnOrder();
    SpawnedPerDirection.Init(0, Spawners.Num());
    SpawnCountdown = 0.f;
    for (AUnitSpawner* Spawner : Spawners)
    {
        if (!IsValid(Spawner)) continue;
        Spawner->ActivateSpawner();
        Spawner->SetSpawningEnabled(true);
    }
    SF_LOG(Log, "[Defence] Wave %d: %d total units, %.0f seconds maximum.",
        WaveNumber, TotalPlannedUnits, RemainingSeconds);
}

bool ASFDefenceWaveController::HasLivingEnemies() const
{
    // Possession replaces a Pawn's Owner with its AI controller. The spawner's
    // tracked units remain authoritative after possession and through death animations.
    for (const AUnitSpawner* Spawner : Spawners)
        if (IsValid(Spawner) && Spawner->HasLivingSpawnedUnits()) return true;
    return false;
}

void ASFDefenceWaveController::CompleteWave()
{
    if (WaveNumber < FMath::Max(1, TotalWaves)) { BeginPreparation(); return; }
    StopWaves();
    // Use the normal victory path so clear progress, epilogue and result UI are preserved.
    if (ASFGameMode* Mode = GetWorld()->GetAuthGameMode<ASFGameMode>()) Mode->ProcessVictory();
    else if (ASFGameState* State = GetWorld()->GetGameState<ASFGameState>()) State->FinishStage(ESFStageResult::Victory);
}

void ASFDefenceWaveController::BuildSpawnOrder()
{
    SpawnOrder.Reset(TotalPlannedUnits);
    int32 TotalWeight = 0;
    for (const FSFDefenceUnitEntry& Entry : UnitPool) TotalWeight += FMath::Max(1, Entry.SpawnWeight);

    // Largest remainders keep the exact wave budget, including odd-sized waves.
    TArray<int32> Quotas, Remainders;
    int32 Allocated = 0;
    for (const FSFDefenceUnitEntry& Entry : UnitPool)
    {
        const int32 WeightedBudget = TotalPlannedUnits * FMath::Max(1, Entry.SpawnWeight);
        Quotas.Add(WeightedBudget / TotalWeight);
        Remainders.Add(WeightedBudget % TotalWeight);
        Allocated += Quotas.Last();
    }
    while (Allocated < TotalPlannedUnits)
    {
        int32 Best = 0;
        for (int32 Index = 1; Index < Remainders.Num(); ++Index)
            if (Remainders[Index] > Remainders[Best]) Best = Index;
        ++Quotas[Best];
        Remainders[Best] = -1;
        ++Allocated;
    }

    // Interleave the quotas so high tiers do not all arrive together at the end.
    TArray<int32> Credits, Remaining = Quotas;
    Credits.Init(0, UnitPool.Num());
    for (int32 Spawn = 0; Spawn < TotalPlannedUnits; ++Spawn)
    {
        int32 Best = INDEX_NONE;
        for (int32 Index = 0; Index < Quotas.Num(); ++Index)
        {
            Credits[Index] += Quotas[Index];
            if (Remaining[Index] > 0 && (Best == INDEX_NONE || Credits[Index] > Credits[Best])) Best = Index;
        }
        SpawnOrder.Add(Best);
        Credits[Best] -= TotalPlannedUnits;
        --Remaining[Best];
    }
}

void ASFDefenceWaveController::SpawnBatch()
{
    for (int32 Direction = 0; Direction < Spawners.Num(); ++Direction)
    {
        AUnitSpawner* Spawner = Spawners[Direction];
        if (!IsValid(Spawner) || Spawner->IsShutDownByOwnerBuilding())
        {
            SpawnedPerDirection[Direction] = PlannedPerDirection[Direction];
            continue;
        }
        if (SpawnedPerDirection[Direction] >= PlannedPerDirection[Direction]) continue;
        const int32 Rank = (Direction + WaveNumber - 1) % Spawners.Num();
        const int32 EntryIndex = SpawnOrder[SpawnedPerDirection[Direction] * Spawners.Num() + Rank];
        if (Spawner->SpawnWaveUnit(UnitPool[EntryIndex].RowName, LoadedClasses[EntryIndex]))
            ++SpawnedPerDirection[Direction];
    }
}

int32 ASFDefenceWaveController::GetRemainingSpawnCount() const
{
    if (Phase != ESFDefencePhase::Wave) return 0;
    int32 Remaining = 0;
    for (int32 Direction = 0; Direction < SpawnedPerDirection.Num(); ++Direction)
        Remaining += FMath::Max(0, PlannedPerDirection[Direction] - SpawnedPerDirection[Direction]);
    return Remaining;
}

void ASFDefenceWaveController::StopWaves()
{
    Phase = ESFDefencePhase::Stopped;
    RemainingSeconds = 0.f;
    for (AUnitSpawner* Spawner : Spawners)
        if (IsValid(Spawner)) Spawner->SetSpawningEnabled(false);
    if (TimerWidget) { TimerWidget->RemoveFromParent(); TimerWidget = nullptr; }
}

void ASFDefenceWaveController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopWaves();
    Super::EndPlay(EndPlayReason);
}
