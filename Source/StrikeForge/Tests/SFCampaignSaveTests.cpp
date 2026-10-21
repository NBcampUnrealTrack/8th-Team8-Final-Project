#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "Game/GameMode/SFGameMode.h"
#include "Game/GameState/SFGameState.h"
#include "Game/Multiplayer/SFPVPBattle.h"
#include "Game/Save/SFCampaignSaveGame.h"
#include "GameFramework/WorldSettings.h"
#include "Components/InputComponent.h"
#include "Tests/SFCampaignTestAccess.h"
#include "UObject/Script.h"

namespace
{
    // New instances exercise Init/load and Shutdown across actual serialized files.
    struct FSFCampaignFixture
    {
        FString Slot = TEXT("SFCampaignTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
        USFGameInstance* GI = nullptr;
        UClass* Class = LoadClass<USFGameInstance>(nullptr,
            TEXT("/Game/Core/Blueprints/GameInstance/BP_SFGameInstance.BP_SFGameInstance_C"));
        ~FSFCampaignFixture() { Stop(); FSFCampaignTestAccess::DeleteSlot(Slot); }
        USFGameInstance* Start()
        {
            Stop();
            if (!Class) return nullptr;
            GI = NewObject<USFGameInstance>(GEngine, Class);
            FSFCampaignTestAccess::UseSlot(GI, Slot);
            GI->InitializeStandalone();
            return GI;
        }
        void Stop()
        {
            if (!GI) return;
            UWorld* World = GI->GetWorld();
            GI->Shutdown();
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
            GI = nullptr;
        }
    };

    bool HasVictoryKey(const ASFInGamePlayerController* PC)
    {
        return PC->InputComponent && PC->InputComponent->KeyBindings.ContainsByPredicate(
            [](const FInputKeyBinding& Binding) { return Binding.Chord.Key == EKeys::K; });
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFPVEAchievementsTest, "StrikeForge.Campaign.Achievements",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFPVEAchievementsTest::RunTest(const FString& Parameters)
{
    FSFCampaignFixture Fixture;
    auto* GI = Fixture.Start();
    if (!TestNotNull(TEXT("Campaign instance"), GI)) return false;
    int32 Notifications = 0;
    GI->OnPVEAchievementUnlocked.AddLambda([&](const FSFPVEAchievement& Entry)
    {
        ++Notifications;
        TestTrue(TEXT("Notification carries a completed achievement"), Entry.bUnlocked);
        TestEqual(TEXT("Exactly the cleared stage is announced"), Entry.StageOrder, Notifications);
    });
    TestEqual(TEXT("One achievement per stage"), GI->GetPVEAchievements().Num(), 7);
    for (const auto& Entry : GI->GetPVEAchievements()) TestFalse(TEXT("Fresh save has no achievements"), Entry.bUnlocked);
    GI->SelectStage(TEXT("Stage01"));
    TestEqual(TEXT("Selection does not unlock an achievement"), Notifications, 0);
    TestFalse(TEXT("Cannot award a locked stage"), GI->MarkStageCleared(TEXT("Stage07")));
    for (int32 Stage = 1; Stage <= 7; ++Stage)
    {
        const FName Row(*FString::Printf(TEXT("Stage%02d"), Stage));
        GI->MarkStageCleared(Row);
        GI->MarkStageCleared(Row);
        TestEqual(TEXT("Replay never repeats the celebration"), Notifications, Stage);
        for (const auto& Entry : GI->GetPVEAchievements())
            TestEqual(TEXT("Only completed stages are unlocked"), Entry.bUnlocked, Entry.StageOrder <= Stage);
    }
    GI = Fixture.Start();
    for (const auto& Entry : GI->GetPVEAchievements()) TestTrue(TEXT("Achievements survive restart"), Entry.bUnlocked);
    TestEqual(TEXT("Loading a save never replays notifications"), Notifications, 7);
    // An existing v1 campaign save needs no achievement fields or migration.
    auto* Legacy = Cast<USFCampaignSaveGame>(UGameplayStatics::CreateSaveGameObject(USFCampaignSaveGame::StaticClass()));
    Legacy->HighestClearedStageOrder = 3;
    Fixture.Stop();
    UGameplayStatics::SaveGameToSlot(Legacy, Fixture.Slot, 0);
    GI = Fixture.Start();
    for (const auto& Entry : GI->GetPVEAchievements())
        TestEqual(TEXT("Old campaign completion is reflected in the journal"), Entry.bUnlocked, Entry.StageOrder <= 3);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCampaignSaveTest, "StrikeForge.Campaign.AutoSaveAndRestart",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFCampaignSaveTest::RunTest(const FString& Parameters)
{
    FSFCampaignFixture Fixture;
    USFGameInstance* GI = Fixture.Start();
    if (!TestNotNull(TEXT("Campaign instance"), GI)) return false;
    TestEqual(TEXT("Fresh campaign continues at Stage01"), GI->GetNextCampaignStageRowName(), FName(TEXT("Stage01")));
    TestFalse(TEXT("Merely starting the game does not create a save"), UGameplayStatics::DoesSaveGameExist(Fixture.Slot, 0));
    TestFalse(TEXT("A locked stage cannot advance progress"), GI->MarkStageCleared(TEXT("Stage03")));
    for (int32 Order = 1; Order <= 3; ++Order)
        TestTrue(TEXT("Sequential victory"), GI->MarkStageCleared(FName(*FString::Printf(TEXT("Stage%02d"), Order))));
    const USFCampaignSaveGame* Save = Cast<USFCampaignSaveGame>(UGameplayStatics::LoadGameFromSlot(Fixture.Slot, 0));
    if (TestNotNull(TEXT("Victory saves before exit"), Save))
        TestEqual(TEXT("Disk already contains Stage03"), Save->HighestClearedStageOrder, 3);

    GI = Fixture.Start();
    TestEqual(TEXT("Restart restores highest clear"), GI->GetHighestClearedStageOrder(), 3);
    TestEqual(TEXT("Restart offers Stage04"), GI->GetNextCampaignStageRowName(), FName(TEXT("Stage04")));
    TestEqual(TEXT("Cleared stage remains replayable"), GI->GetStageProgressState(TEXT("Stage02")), ESFStageProgressState::Cleared);
    TestEqual(TEXT("Next stage unlocked"), GI->GetStageProgressState(TEXT("Stage04")), ESFStageProgressState::Unlocked);
    TestEqual(TEXT("Later stages stay locked"), GI->GetStageProgressState(TEXT("Stage05")), ESFStageProgressState::Locked);
    TestTrue(TEXT("Unread story notice restored"), GI->HasUnreadStoryUnlock());
    TestTrue(TEXT("Continue target uses normal stage selection"), GI->SelectStage(GI->GetNextCampaignStageRowName()));
    GI->MarkStoryUnlockRead();
    TestTrue(TEXT("Earlier stage replay succeeds"), GI->MarkStageCleared(TEXT("Stage01")));
    GI = Fixture.Start();
    TestFalse(TEXT("Story notice stays read after restart"), GI->HasUnreadStoryUnlock());
    TestEqual(TEXT("Earlier replay cannot roll back progress"), GI->GetHighestClearedStageOrder(), 3);
    for (int32 Order = 4; Order <= 7; ++Order)
        TestTrue(TEXT("Remaining stage clear"), GI->MarkStageCleared(FName(*FString::Printf(TEXT("Stage%02d"), Order))));
    GI = Fixture.Start();
    TestEqual(TEXT("Final victory persists"), GI->GetHighestClearedStageOrder(), 7);
    TestTrue(TEXT("Completed campaign has no nonexistent next stage"), GI->GetNextCampaignStageRowName().IsNone());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCampaignSaveRecoveryTest, "StrikeForge.Campaign.SaveRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFCampaignSaveRecoveryTest::RunTest(const FString& Parameters)
{
    FSFCampaignFixture Fixture;
    USFGameInstance* GI = Fixture.Start();
    if (!TestNotNull(TEXT("Campaign instance"), GI)) return false;
    GI->MarkStageCleared(TEXT("Stage01"));
    Fixture.Stop();
    USFCampaignSaveGame* Invalid = NewObject<USFCampaignSaveGame>();
    Invalid->HighestClearedStageOrder = 100;
    TestTrue(TEXT("Write invalid primary fixture"), UGameplayStatics::SaveGameToSlot(Invalid, Fixture.Slot, 0));
    GI = Fixture.Start();
    TestEqual(TEXT("Invalid primary recovers backup"), GI->GetHighestClearedStageOrder(), 1);
    Fixture.Stop();
    const USFCampaignSaveGame* Repaired = Cast<USFCampaignSaveGame>(UGameplayStatics::LoadGameFromSlot(Fixture.Slot, 0));
    if (TestNotNull(TEXT("Recovered save repaired on exit"), Repaired))
        TestEqual(TEXT("Repaired primary contains original progress"), Repaired->HighestClearedStageOrder, 1);

    Invalid->SaveVersion = USFCampaignSaveGame::CurrentVersion + 1;
    Invalid->HighestClearedStageOrder = 2;
    UGameplayStatics::SaveGameToSlot(Invalid, Fixture.Slot, 0);
    GI = Fixture.Start();
    GI->MarkStageCleared(TEXT("Stage01"));
    Fixture.Stop();
    const USFCampaignSaveGame* Future = Cast<USFCampaignSaveGame>(UGameplayStatics::LoadGameFromSlot(Fixture.Slot, 0));
    if (TestNotNull(TEXT("Newer save retained"), Future))
        TestEqual(TEXT("Older game cannot overwrite newer format"), Future->SaveVersion, USFCampaignSaveGame::CurrentVersion + 1);

    FSFCampaignTestAccess::DeleteSlot(Fixture.Slot);
    GI = Fixture.Start();
    TestEqual(TEXT("Missing save starts fresh"), GI->GetHighestClearedStageOrder(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCampaignVictoryKeyTest, "StrikeForge.Campaign.PVEVictoryKey",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFCampaignVictoryKeyTest::RunTest(const FString& Parameters)
{
    FEditorScriptExecutionGuard ScriptGuard;
    FSFCampaignFixture Fixture;
    USFGameInstance* GI = Fixture.Start();
    if (!TestNotNull(TEXT("Campaign instance"), GI)) return false;
    UWorld* World = GI->GetWorld();
    World->GetWorldSettings()->DefaultGameMode = ASFGameMode::StaticClass();
    TestTrue(TEXT("PvE game mode created"), World->SetGameMode(FURL()));
    ASFGameMode* Mode = World->GetAuthGameMode<ASFGameMode>();
    if (!TestNotNull(TEXT("PvE mode"), Mode)) return false;
    Mode->PreInitializeComponents();
    ASFGameState* State = World->GetGameState<ASFGameState>();
    if (!TestNotNull(TEXT("PvE state"), State)) return false;
    ASFInGamePlayerController* PC = World->SpawnActor<ASFInGamePlayerController>();
    FSFCampaignTestAccess::SetupInput(PC);
    TestTrue(TEXT("K bound in standalone PvE"), HasVictoryKey(PC));
    FSFCampaignTestAccess::InstantVictory(PC);
    TestEqual(TEXT("K during loading does not clear"), GI->GetHighestClearedStageOrder(), 0);
    State->BeginRaceDataLoading();
    State->InitializeStage(TEXT("Stage01"), {});
    State->BeginInitialBuildingAssetsLoading();
    State->MarkStageReadyToStart();
    State->StartStage();
    // Execute the actual bound key delegate, including the production victory/save path.
    for (FInputKeyBinding& Binding : PC->InputComponent->KeyBindings)
        if (Binding.Chord.Key == EKeys::K && Binding.KeyEvent == IE_Pressed) Binding.KeyDelegate.Execute(EKeys::K);
    TestEqual(TEXT("K finishes with victory"), State->GetStageResult(), ESFStageResult::Victory);
    TestEqual(TEXT("K victory records campaign clear"), GI->GetHighestClearedStageOrder(), 1);
    FSFCampaignTestAccess::InstantVictory(PC);
    TestEqual(TEXT("Repeated K cannot skip another stage"), GI->GetHighestClearedStageOrder(), 1);

    GI = Fixture.Start();
    TestEqual(TEXT("K victory survives restart"), GI->GetHighestClearedStageOrder(), 1);
    World = GI->GetWorld();
    World->GetWorldSettings()->DefaultGameMode = ASFPVPBattleMode::StaticClass();
    TestTrue(TEXT("PvP game mode created"), World->SetGameMode(FURL()));
    ASFPVPBattleController* PVP = World->SpawnActor<ASFPVPBattleController>();
    PVP->SetupInputComponent();
    TestFalse(TEXT("PvP has no K victory binding"), HasVictoryKey(PVP));
    FSFCampaignTestAccess::InstantVictory(PVP);
    TestEqual(TEXT("Even direct invocation in PvP cannot alter progress"), GI->GetHighestClearedStageOrder(), 1);
    return true;
}
#endif
