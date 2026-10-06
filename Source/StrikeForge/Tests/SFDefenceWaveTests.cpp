#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Defence/SFDefenceWaveController.h"
#include "Game/GameState/SFGameState.h"
#include "InGame/HUD/Tutorial/SFTutorialSubsystem.h"
#include "Units/Spawner/UnitSpawner.h"
#include "Units/Unit/UnitBase.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFDefenceWaveFlowTest,
    "StrikeForge.Defence.WaveFlow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFDefenceWaveFlowTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Values;
    Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("World"), World)) return false;
    ASFGameState* State = World->SpawnActor<ASFGameState>();
    World->SetGameState(State);
    FEnumProperty* StateProperty = FindFProperty<FEnumProperty>(ASFGameState::StaticClass(), TEXT("StageState"));
    StateProperty->GetUnderlyingProperty()->SetIntPropertyValue(
        StateProperty->ContainerPtrToValuePtr<void>(State), static_cast<int64>(ESFStageState::Playing));
    ASFDefenceWaveController* Controller = World->SpawnActor<ASFDefenceWaveController>();
    Controller->SpawnCountMultiplier = 0.4f / 3.f;
    for (int32 Index = 0; Index < 3; ++Index)
    {
        AUnitSpawner* Spawner = World->SpawnActor<AUnitSpawner>(FVector(Index * 1000.f, 0.f, 0.f), FRotator::ZeroRotator);
        FindFProperty<FBoolProperty>(AUnitSpawner::StaticClass(), TEXT("bStopWhenOwnerBuildingDestroyed"))
            ->SetPropertyValue_InContainer(Spawner, false);
        Spawner->bWaveControlled = true;
        Spawner->DispatchBeginPlay();
        Controller->Spawners.Add(Spawner);
    }
    for (int32 Index = 0; Index < 6; ++Index)
    {
        FSFDefenceUnitEntry Entry;
        Entry.RowName = FName(*FString::Printf(TEXT("NormalTier%d"), Index));
        Entry.UnitClass = AUnitBase::StaticClass();
        Entry.SpawnWeight = Index % 3 == 0 ? 5 : Index % 3 == 1 ? 3 : 1;
        Controller->UnitPool.Add(Entry);
    }
    Controller->DispatchBeginPlay();
    USFTutorialSubsystem::SetStageTutorialActive(World, true);
    Controller->Tick(180.f);
    TestTrue(TEXT("Tutorial holds the wave flow"), Controller->GetPhase() == ESFDefencePhase::WaitingForTutorial);
    USFTutorialSubsystem::SetStageTutorialActive(World, false);
    Controller->Tick(1.f);
    TestTrue(TEXT("Tutorial end enters preparation"), Controller->GetPhase() == ESFDefencePhase::Preparation);
    TestEqual(TEXT("Only initial preparation is one minute"), Controller->GetRemainingSeconds(), 60.f);
    TestNull(TEXT("Direct BP spawn blocked during preparation"), Controller->Spawners[0]->SpawnOne());
    Controller->Tick(59.f);
    TestTrue(TEXT("Preparation lasts its full duration"), Controller->GetPhase() == ESFDefencePhase::Preparation);
    Controller->Tick(1.f);
    TestEqual(TEXT("Solo first wave is exactly one third of the previous budget"), Controller->GetRemainingSpawnCount(), 14);
    TestEqual(TEXT("Wave time is five minutes"), Controller->GetRemainingSeconds(), 300.f);

    // Spawn all scheduled batches early. Live enemies must not delay or disappear at the transition.
    for (int32 Index = 0; Index < 5; ++Index) Controller->Tick(4.f);
    TestTrue(TEXT("Scheduled exhaustion starts preparation early"), Controller->GetPhase() == ESFDefencePhase::Preparation);
    TestEqual(TEXT("Next preparation resets to three minutes"), Controller->GetRemainingSeconds(), 180.f);
    TMap<FName, int32> TypeCounts;
    int32 DirectionIndex = 0;
    for (AUnitSpawner* Spawner : Controller->Spawners)
    {
        TestEqual(TEXT("Previous wave enemies survive preparation"), Spawner->GetAliveCount(), Controller->PlannedPerDirection[DirectionIndex++]);
        for (TActorIterator<AUnitBase> It(World); It; ++It)
            if (It->GetOwner() == Spawner) ++TypeCounts.FindOrAdd(It->GetUnitRowName());
        TestNull(TEXT("Preparation blocks controlled spawns too"),
            Spawner->SpawnWaveUnit(TEXT("NormalTier0"), AUnitBase::StaticClass()));
        TestNull(TEXT("Direct BP calls cannot bypass wave control"), Spawner->SpawnOne());
    }
    TestEqual(TEXT("The solo wave still contains all six normal types overall"), TypeCounts.Num(), 6);
    const int32 ExpectedByTier[] = {4, 2, 1};
    for (int32 Index = 0; Index < Controller->UnitPool.Num(); ++Index)
        TestEqual(TEXT("Global solo composition follows the tier weights"),
            TypeCounts.FindRef(Controller->UnitPool[Index].RowName), ExpectedByTier[Index % 3]);
    Controller->Tick(180.f);
    TestEqual(TEXT("Second solo wave is exactly one third of the previous budget"), Controller->GetRemainingSpawnCount(), 22);
    TArray<int32> TierCounts;
    TierCounts.Init(0, 3);
    for (int32 EntryIndex : Controller->SpawnOrder) ++TierCounts[EntryIndex % 3];
    TestEqual(TEXT("Second wave global tier 1 quota"), TierCounts[0], 12);
    TestEqual(TEXT("Second wave global tier 2 quota"), TierCounts[1], 8);
    TestEqual(TEXT("Second wave global tier 3 quota"), TierCounts[2], 2);
    Controller->Tick(4.f);
    TestEqual(TEXT("Second wave spawns from all three directions"), Controller->GetRemainingSpawnCount(), 19);
    Controller->Tick(300.f);
    TestTrue(TEXT("Timeout starts preparation even with unspent budget"), Controller->GetPhase() == ESFDefencePhase::Preparation);
    TestEqual(TEXT("Timeout preserves leftover enemies"), Controller->Spawners[0]->GetAliveCount(), 6);
    const int32 LaterBudgets[] = {29, 36, 43};
    for (int32 Wave = 3; Wave <= 5; ++Wave)
    {
        TestEqual(TEXT("Every later preparation is three minutes"), Controller->GetRemainingSeconds(), 180.f);
        Controller->Tick(179.f);
        TestTrue(TEXT("Later preparation remains active before its deadline"), Controller->GetPhase() == ESFDefencePhase::Preparation);
        Controller->Tick(1.f);
        TestEqual(TEXT("Difficulty increases each wave"), Controller->GetRemainingSpawnCount(), LaterBudgets[Wave - 3]);
        TestEqual(TEXT("Expected wave number"), Controller->GetWaveNumber(), Wave);
        if (Wave < 5) Controller->Tick(300.f);
    }
    for (int32 Spawn = 0; Spawn < 15; ++Spawn) Controller->Tick(4.f);
    TestEqual(TEXT("Final wave has exhausted its spawn budget"), Controller->GetRemainingSpawnCount(), 0);
    TestTrue(TEXT("Final wave keeps fighting live enemies after the last spawn"), Controller->GetPhase() == ESFDefencePhase::Wave);
    TestTrue(TEXT("Final wave cannot win just by spawning all enemies"), State->IsStagePlaying());
    Controller->Tick(Controller->GetRemainingSeconds());
    TestTrue(TEXT("Surviving the fifth wave finishes the stage"), State->GetStageResult() == ESFStageResult::Victory);
    TestTrue(TEXT("No sixth preparation or wave is started"), Controller->GetPhase() == ESFDefencePhase::Stopped);
    Controller->Tick(360.f);
    TestEqual(TEXT("Victory never restarts a sixth wave"), Controller->GetWaveNumber(), 5);

    // Also verify early final victory after clearing all surviving enemies, including earlier waves.
    StateProperty->GetUnderlyingProperty()->SetIntPropertyValue(
        StateProperty->ContainerPtrToValuePtr<void>(State), static_cast<int64>(ESFStageState::Playing));
    FEnumProperty* ResultProperty = FindFProperty<FEnumProperty>(ASFGameState::StaticClass(), TEXT("StageResult"));
    ResultProperty->GetUnderlyingProperty()->SetIntPropertyValue(
        ResultProperty->ContainerPtrToValuePtr<void>(State), static_cast<int64>(ESFStageResult::Undetermined));
    Controller->Phase = ESFDefencePhase::Preparation;
    Controller->WaveNumber = 4;
    Controller->BeginWave();
    for (int32 Spawn = 0; Spawn < 15; ++Spawn) Controller->Tick(4.f);
    FEnumProperty* CombatStateProperty = FindFProperty<FEnumProperty>(AUnitBase::StaticClass(), TEXT("CombatState"));
    for (TActorIterator<AUnitBase> It(World); It; ++It)
        CombatStateProperty->GetUnderlyingProperty()->SetIntPropertyValue(
            CombatStateProperty->ContainerPtrToValuePtr<void>(*It), static_cast<int64>(EUnitCombatState::Dead));
    TestTrue(TEXT("Corpse actors still exist during the death animation"), Controller->Spawners[0]->GetAliveCount() > 0);
    Controller->Tick(1.f);
    TestTrue(TEXT("Clearing every live enemy completes the final wave early"), Controller->GetPhase() == ESFDefencePhase::Stopped);
    TestEqual(TEXT("Early final clear records victory"), State->GetStageResult(), ESFStageResult::Victory);
    Controller->StopWaves();
    Controller->Tick(300.f);
    TestTrue(TEXT("Stopped waves never restart"), Controller->GetPhase() == ESFDefencePhase::Stopped);
    // Stage06 retains the full 300 seconds even if every scheduled unit is gone.
    StateProperty->GetUnderlyingProperty()->SetIntPropertyValue(
        StateProperty->ContainerPtrToValuePtr<void>(State), static_cast<int64>(ESFStageState::Playing));
    ResultProperty->GetUnderlyingProperty()->SetIntPropertyValue(
        ResultProperty->ContainerPtrToValuePtr<void>(State), static_cast<int64>(ESFStageResult::Undetermined));
    Controller->bRequireFullWaveDuration = true;
    Controller->PreparationDuration = 60.f;
    Controller->WaveNumber = 0;
    Controller->BeginPreparation();
    for (int32 Wave = 1; Wave <= 5; ++Wave)
    {
        TestEqual(TEXT("Stage06 always prepares for one minute"), Controller->GetRemainingSeconds(), 60.f);
        Controller->Tick(60.f);
        for (int32 Spawn = 0; Spawn < 15; ++Spawn) Controller->Tick(4.f);
        for (TActorIterator<AUnitBase> It(World); It; ++It)
            CombatStateProperty->GetUnderlyingProperty()->SetIntPropertyValue(
                CombatStateProperty->ContainerPtrToValuePtr<void>(*It), static_cast<int64>(EUnitCombatState::Dead));
        Controller->Tick(1.f);
        TestEqual(TEXT("Stage06 cannot complete early"), Controller->GetPhase(), ESFDefencePhase::Wave);
        TestTrue(TEXT("Stage06 still playing before deadline"), State->IsStagePlaying());
        Controller->Tick(Controller->GetRemainingSeconds());
    }
    TestEqual(TEXT("Stage06 wins after all five full waves"), State->GetStageResult(), ESFStageResult::Victory);
    TestEqual(TEXT("Stage06 stops at wave five"), Controller->GetWaveNumber(), 5);
    World->DestroyWorld(false);
    return true;
}
#endif
