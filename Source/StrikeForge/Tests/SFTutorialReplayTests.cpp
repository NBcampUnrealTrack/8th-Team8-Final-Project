#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/GameState/SFGameState.h"
#include "InGame/HUD/Tutorial/SFTutorialReplay.h"
#include "InGame/HUD/Tutorial/SFTutorialSubsystem.h"
#include "Tests/SFCampaignTestAccess.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFTutorialReplayDataTest, "StrikeForge.Tutorial.Replay.AuthoredResults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFTutorialReplayDataTest::RunTest(const FString& Parameters)
{
    const UDataTable* Table = LoadObject<UDataTable>(nullptr,
        TEXT("/Game/Game/IN_UI/DT/DT_SFStageIntro.DT_SFStageIntro"));
    if (!TestNotNull(TEXT("Tutorial table"), Table)) return false;
    for (int32 Stage = 1; Stage <= 7; ++Stage)
    {
        const FName Name(*FString::Printf(TEXT("Stage%02d"), Stage));
        for (const ESFRace Race : {ESFRace::Beast, ESFRace::Human})
        {
            const FSFStageIntroDataRow* Row = Table->FindRow<FSFStageIntroDataRow>(
                SFStageStory::ResolveRowName(Table, Name, Race), TEXT("ReplayTest"));
            if (!TestNotNull(*Name.ToString(), Row)) continue;
            const FSFTutorialReplayPlan Plan = FSFTutorialReplayPlan::FromRow(*Row);
            TestEqual(TEXT("Only reachable construction instructions produce buildings"), Plan.Buildings.Num(),
                Stage == 1 ? 3 : Stage == 2 ? 1 : Stage == 3 ? 2 : 0);
            TestEqual(TEXT("Exactly one tutorial unit for each training stage"), Plan.Units.Num(), Stage <= 3 ? 1 : 0);
            TestEqual(TEXT("Nexus upgrade added to the authored starting tier"),
                Plan.GetInitialTier(ESFBuildingType::Nexus, Stage == 3 ? 1 : 0),
                Stage == 3 ? 2 : Stage == 2 ? 1 : 0);
            TestEqual(TEXT("Barracks upgrade is exclusive to Stage02"),
                Plan.GetInitialTier(ESFBuildingType::Barracks, 0), Stage == 2 ? 1 : 0);
            if (Stage == 2)
                TestTrue(TEXT("Replay releases the enemy activation gate"), Plan.CompletedObjectives.HasTagExact(
                    FGameplayTag::RequestGameplayTag(TEXT("Tutorial.Stage02.Completed"))));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFTutorialReplayEntryTest, "StrikeForge.Tutorial.Replay.EntrySnapshot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFTutorialReplayEntryTest::RunTest(const FString& Parameters)
{
    UClass* Class = LoadClass<USFGameInstance>(nullptr,
        TEXT("/Game/Core/Blueprints/GameInstance/BP_SFGameInstance.BP_SFGameInstance_C"));
    if (!TestNotNull(TEXT("Campaign class"), Class)) return false;
    const FString Slot = TEXT("SFReplayTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    USFGameInstance* Instance = NewObject<USFGameInstance>(GEngine, Class);
    FSFCampaignTestAccess::UseSlot(Instance, Slot);
    Instance->InitializeStandalone();
    UWorld* World = Instance->GetWorld();
    auto Enter = [World]()
    {
        ASFGameState* State = World->SpawnActor<ASFGameState>();
        State->BeginRaceDataLoading();
        State->InitializeStage(TEXT("Stage01"), {});
        return State;
    };
    ASFGameState* First = Enter();
    TestFalse(TEXT("First attempt retains tutorial and story"), First->IsClearedStageReplay());
    ASFGameState* FailedRetry = Enter();
    TestFalse(TEXT("Retry before any victory retains tutorial"), FailedRetry->IsClearedStageReplay());
    TestTrue(TEXT("First victory recorded"), Instance->MarkStageCleared(TEXT("Stage01")));
    TestFalse(TEXT("First victory retains its epilogue after saving"), First->IsClearedStageReplay());
    ASFGameState* Replay = Enter();
    TestTrue(TEXT("Next entry skips tutorial, story and epilogue"), Replay->IsClearedStageReplay());
    Instance->Shutdown();
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    FSFCampaignTestAccess::DeleteSlot(Slot);
    return true;
}
#endif
