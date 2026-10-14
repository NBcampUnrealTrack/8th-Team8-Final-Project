#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Units/Spawner/UnitSpawner.h"
#include "Units/Unit/UnitBase.h"

struct FSFSpawnerProgressionTestAccess
{
    static FName Draw(AUnitSpawner* Spawner)
    {
        const auto* Entry = Spawner->GetProgressionEntry();
        if (!Entry) return NAME_None;
        const FName Row = Entry->RowName;
        Spawner->ProgressionSpawnBag.Pop(EAllowShrinking::No);
        return Row;
    }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFSpawnerProgressionTest, "StrikeForge.Spawner.TimedTierMix",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFSpawnerProgressionTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Values;
    Values.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
    auto* Spawner = World->SpawnActor<AUnitSpawner>();
    const int32 Weights[4][3] = {{10,0,0},{7,3,0},{4,5,1},{3,5,2}};
    for (int32 Phase = 0; Phase < 4; ++Phase)
    {
        FSFSpawnPhase Config;
        Config.StartSeconds = Phase * 120.f;
        for (int32 Tier = 0; Tier < 3; ++Tier)
        {
            FSFSpawnPoolEntry Entry;
            Entry.RowName = FName(*FString::FromInt(Tier));
            Entry.UnitClass = AUnitBase::StaticClass();
            Entry.Weight = Weights[Phase][Tier];
            Config.UnitPool.Add(Entry);
        }
        Spawner->SpawnPhases.Add(Config);
    }
    for (int32 Phase = 0; Phase < 4; ++Phase)
    {
        Spawner->ProgressionElapsedSeconds = Phase * 120.f;
        TestEqual(TEXT("Phase switches at active-play boundary"), Spawner->GetProgressionPhaseIndex(), Phase);
        for (int32 Bag = 0; Bag < 20; ++Bag)
        {
            int32 Counts[3] = {};
            for (int32 Draw = 0; Draw < 10; ++Draw)
                ++Counts[FCString::Atoi(*FSFSpawnerProgressionTestAccess::Draw(Spawner).ToString())];
            for (int32 Tier = 0; Tier < 3; ++Tier)
                TestEqual(TEXT("Each ten successful draws preserves configured tier ratio"), Counts[Tier], Weights[Phase][Tier]);
        }
        // Leave an unfinished bag: a phase transition must discard its old composition.
        FSFSpawnerProgressionTestAccess::Draw(Spawner);
        Spawner->ProgressionElapsedSeconds = (Phase + 1) * 120.f - .01f;
        TestEqual(TEXT("Higher tiers stay gated before boundary"), Spawner->GetProgressionPhaseIndex(), Phase);
    }
    World->DestroyWorld(false);
    return true;
}
#endif
