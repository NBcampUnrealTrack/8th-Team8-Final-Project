#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "InGame/HUD/Tutorial/SFStageIntroWidget.h"
#include "InGame/HUD/Tutorial/SFTutorialSubsystem.h"
#include "Units/Spawner/UnitSpawner.h"
#include "UObject/Script.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFStage02SpawnGateTest,
    "StrikeForge.Tutorial.Stage02.SpawnAfterEnd",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFStage02SpawnGateTest::RunTest(const FString& Parameters)
{
    // This isolated world has no initialized level actors; allow dynamic actor delegates in the editor test.
    FEditorScriptExecutionGuard ScriptGuard;
    const FGameplayTag Completed = FGameplayTag::RequestGameplayTag(TEXT("Tutorial.Stage02.Completed"));
    const UDataTable* StageTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Game/IN_UI/DT/DT_SFStageIntro.DT_SFStageIntro"));
    if (!TestNotNull(TEXT("Stage intro data"), StageTable)) return false;
    const FSFStageIntroDataRow* Stage02 = StageTable->FindRow<FSFStageIntroDataRow>(TEXT("Stage02"), TEXT("SpawnGateTest"));
    if (!TestNotNull(TEXT("Stage02 row"), Stage02) || Stage02->Nodes.IsEmpty()) return false;
    TestTrue(TEXT("Stage02 ends with an explicit End"), Stage02->Nodes.Last().NodeType == ESFStageIntroNodeType::End);
    TestTrue(TEXT("Saved End node publishes the spawner gate"), Stage02->Nodes.Last().ObjectiveTag == Completed);
    UClass* WidgetClass = LoadClass<USFStageIntroWidget>(nullptr,
        TEXT("/Game/Game/IN_UI/HUD/WBP/Intro/WBP_SFStageIntro.WBP_SFStageIntro_C"));
    if (!TestNotNull(TEXT("Tutorial widget class"), WidgetClass)) return false;

    UWorld::InitializationValues Values;
    Values.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("Test world"), World)) return false;
    USFTutorialSubsystem* Tutorial = World->GetSubsystem<USFTutorialSubsystem>();
    if (!TestNotNull(TEXT("Tutorial subsystem"), Tutorial)) { World->DestroyWorld(false); return false; }

    auto MakeSpawner = [World, Completed]()
    {
        AUnitSpawner* Spawner = World->SpawnActor<AUnitSpawner>();
        FindFProperty<FBoolProperty>(AUnitSpawner::StaticClass(), TEXT("bStartActive"))->SetPropertyValue_InContainer(Spawner, false);
        FindFProperty<FBoolProperty>(AUnitSpawner::StaticClass(), TEXT("bStopWhenOwnerBuildingDestroyed"))->SetPropertyValue_InContainer(Spawner, false);
        FStructProperty* Gate = FindFProperty<FStructProperty>(AUnitSpawner::StaticClass(), TEXT("ActivationObjectiveTag"));
        *Gate->ContainerPtrToValuePtr<FGameplayTag>(Spawner) = Completed;
        Spawner->DispatchBeginPlay();
        return Spawner;
    };

    AUnitSpawner* Spawner = MakeSpawner();
    TestFalse(TEXT("Spawner starts inactive"), Spawner->IsSpawnerActivated());
    TestNull(TEXT("Direct SpawnOne cannot bypass the tutorial gate"), Spawner->SpawnOne());
    Spawner->SetSpawningEnabled(true);
    TestFalse(TEXT("Runtime enable does not open the activation gate"), Spawner->IsSpawnerActivated());

    // Completing the last gameplay mission must not unlock enemies before its closing dialogue.
    USFTutorialSubsystem::NotifyObjective(World, TAG_SF_Tutorial_Unit_Produced_AfterBarrackUpgrade, true);
    TestFalse(TEXT("Unit production is not tutorial completion"), Spawner->IsSpawnerActivated());

    UDataTable* Table = NewObject<UDataTable>(World);
    Table->RowStruct = FSFStageIntroDataRow::StaticStruct();
    FSFStageIntroDataRow Row;
    FSFStageIntroNode Dialogue;
    Dialogue.NodeType = ESFStageIntroNodeType::Dialogue;
    Dialogue.NextNodeIndex = 1;
    Dialogue.bSkipTypewriter = true;
    // Even if a dialogue has a tag, it must not publish an End signal.
    Dialogue.ObjectiveTag = Completed;
    Row.Nodes.Add(Dialogue);
    FSFStageIntroNode End;
    End.NodeType = ESFStageIntroNodeType::End;
    End.ObjectiveTag = Completed;
    Row.Nodes.Add(End);
    Table->AddRow(TEXT("Stage02"), Row);
    USFStageIntroWidget* Widget = NewObject<USFStageIntroWidget>(World, WidgetClass);
    TestTrue(TEXT("Closing dialogue starts"), Widget->StartStageIntro(Table, TEXT("Stage02")));
    TestFalse(TEXT("Dialogue does not publish completion"), Tutorial->IsObjectiveCompleted(Completed));
    TestFalse(TEXT("Enemies stay inactive throughout dialogue"), Spawner->IsSpawnerActivated());

    TestTrue(TEXT("Explicit End node starts"), Widget->StartStageIntroAt(Table, TEXT("Stage02"), 1));
    TestTrue(TEXT("End records tutorial completion"), Tutorial->IsObjectiveCompleted(Completed));
    TestTrue(TEXT("End activates the waiting spawner"), Spawner->IsSpawnerActivated());
    TestEqual(TEXT("Activation respects InitialDelay (no immediate spawn)"), Spawner->GetAliveCount(), 0);

    AUnitSpawner* LateSpawner = MakeSpawner();
    TestTrue(TEXT("A spawner loaded after completion also activates"), LateSpawner->IsSpawnerActivated());
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBuildGuideFlowDataTest,
    "StrikeForge.Tutorial.Building.FlowData",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFBuildGuideFlowDataTest::RunTest(const FString& Parameters)
{
    const UDataTable* Table = LoadObject<UDataTable>(nullptr,
        TEXT("/Game/Game/IN_UI/DT/DT_SFStageIntro.DT_SFStageIntro"));
    if (!TestNotNull(TEXT("Stage intro data"), Table)) return false;

    const FGameplayTag OpenTag = FGameplayTag::RequestGameplayTag(TEXT("Tutorial.BuildMode.Open"));
    const FGameplayTag SelectRoot = FGameplayTag::RequestGameplayTag(TEXT("Tutorial.Select.Building"));
    const FGameplayTag BuildRoot = FGameplayTag::RequestGameplayTag(TEXT("Tutorial.Build"));
    struct FExpectedGuide { FName Row; int32 Node; FName Building; int32 Next; };
    const FExpectedGuide Expected[] = {
        {TEXT("Stage01"), 23, TEXT("AnimalMineralMine"), 24},
        {TEXT("Stage01"), 27, TEXT("AnimalHouse"), 28},
        {TEXT("Stage01"), 31, TEXT("AnimalBarrack"), 32},
        {TEXT("Stage02"), 22, TEXT("AnimalEtherMine"), 23},
        {TEXT("Stage03"), 4, TEXT("AnimalRanged"), 6},
        {TEXT("Stage03"), 7, TEXT("AnimalSpecial"), 9},
    };

    for (const FExpectedGuide& ExpectedGuide : Expected)
    {
        const FSFStageIntroDataRow* Row = Table->FindRow<FSFStageIntroDataRow>(ExpectedGuide.Row, TEXT("BuildFlowTest"));
        if (!TestNotNull(*FString::Printf(TEXT("%s row"), *ExpectedGuide.Row.ToString()), Row) ||
            !TestTrue(TEXT("Guide node exists"), Row->Nodes.IsValidIndex(ExpectedGuide.Node)))
        {
            continue;
        }

        const FSFStageIntroNode& Node = Row->Nodes[ExpectedGuide.Node];
        TestTrue(TEXT("Construction uses an interactive guide"), Node.NodeType == ESFStageIntroNodeType::InteractiveGuide);
        TestEqual(TEXT("Construction guide has menu/select/place steps"), Node.GuideSteps.Num(), 3);
        TestEqual(TEXT("Construction guide continues at expected node"), Node.NextNodeIndex, ExpectedGuide.Next);
        if (Node.GuideSteps.Num() != 3) continue;

        const FSFStageIntroGuideStep& Open = Node.GuideSteps[0];
        const FSFStageIntroGuideStep& Select = Node.GuideSteps[1];
        const FSFStageIntroGuideStep& Place = Node.GuideSteps[2];
        TestTrue(TEXT("First step opens build mode"), Open.CompletionTag == OpenTag);
        TestTrue(TEXT("Second step highlights only a building selection"), Select.CompletionTag.MatchesTag(SelectRoot));
        TestEqual(TEXT("Second step requires the correct building"), Select.RequiredBuildingRowName, ExpectedGuide.Building);
        TestTrue(TEXT("Placement waits for actual construction"), Place.CompletionTag.MatchesTag(BuildRoot));
        TestTrue(TEXT("Placement shows a ghost"), Place.bShowPlacementGhost);
        TestTrue(TEXT("Placement is restricted to its marked world location"),
            Place.TargetKind == ESFGuideTargetKind::WorldLocation && Place.bRequireNearTarget);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFStage03StoryDataTest,
    "StrikeForge.Tutorial.Stage03.StoryAndEpilogueData",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFStage03StoryDataTest::RunTest(const FString& Parameters)
{
    const UDataTable* Table = LoadObject<UDataTable>(nullptr,
        TEXT("/Game/Game/IN_UI/DT/DT_SFStageIntro.DT_SFStageIntro"));
    if (!TestNotNull(TEXT("Stage intro data"), Table)) return false;

    const FSFStageIntroDataRow* Stage03 =
        Table->FindRow<FSFStageIntroDataRow>(TEXT("Stage03"), TEXT("Stage03StoryTest"));
    if (!TestNotNull(TEXT("Stage03 row"), Stage03)) return false;
    if (!TestTrue(TEXT("Stage03 story start is valid"),
        Stage03->Nodes.IsValidIndex(Stage03->StartNodeIndex)))
    {
        return false;
    }

    const FSFStageIntroNode& Opening = Stage03->Nodes[Stage03->StartNodeIndex];
    TestTrue(TEXT("Stage03 starts with A-3 story"),
        Opening.NodeType == ESFStageIntroNodeType::Dialogue &&
        Opening.TitleText.ToString().Contains(TEXT("A-3 영웅의 분노")));

    int32 NodeIndex = Stage03->StartNodeIndex;
    TSet<int32> Visited;
    while (Stage03->Nodes.IsValidIndex(NodeIndex) && NodeIndex != 0 && !Visited.Contains(NodeIndex))
    {
        Visited.Add(NodeIndex);
        NodeIndex = Stage03->Nodes[NodeIndex].NextNodeIndex;
    }
    TestEqual(TEXT("Opening story returns to the existing tutorial"), NodeIndex, 0);

    const FSFStageIntroDataRow* Epilogue =
        Table->FindRow<FSFStageIntroDataRow>(TEXT("Stage03Clear"), TEXT("Stage03StoryTest"));
    if (!TestNotNull(TEXT("Stage03Clear row"), Epilogue) || Epilogue->Nodes.IsEmpty())
    {
        return false;
    }

    TestEqual(TEXT("Epilogue begins at its first dialogue"), Epilogue->StartNodeIndex, 0);
    TestTrue(TEXT("Epilogue ends explicitly before the result UI"),
        Epilogue->Nodes.Last().NodeType == ESFStageIntroNodeType::End);

    bool bAgentRevealed = false;
    bool bOfficerDies = false;
    for (const FSFStageIntroNode& Node : Epilogue->Nodes)
    {
        const FString Body = Node.BodyText.ToString();
        bAgentRevealed |= Body.Contains(TEXT("스파이는 요원이었습니다"));
        bOfficerDies |= Body.Contains(TEXT("조용히 눈을 감았다"));
    }
    TestTrue(TEXT("Epilogue identifies the agent as the spy"), bAgentRevealed);
    TestTrue(TEXT("Epilogue contains the officer's final moment"), bOfficerDies);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFTutorialBuildingTimingTest,
    "StrikeForge.Tutorial.Building.StageSpecificTiming",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFTutorialBuildingTimingTest::RunTest(const FString& Parameters)
{
    FEditorScriptExecutionGuard ScriptGuard;

    UWorld::InitializationValues Values;
    Values.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(
        EWorldType::Game,
        false,
        NAME_None,
        nullptr,
        true,
        ERHIFeatureLevel::Num,
        &Values);
    if (!TestNotNull(TEXT("Test world"), World)) return false;

    USFTutorialSubsystem* Tutorial = World->GetSubsystem<USFTutorialSubsystem>();
    if (!TestNotNull(TEXT("Tutorial subsystem"), Tutorial))
    {
        World->DestroyWorld(false);
        return false;
    }

    UDataTable* Table = NewObject<UDataTable>(World);
    Table->RowStruct = FSFStageIntroDataRow::StaticStruct();
    TArray<FSFStageIntroGuideStep> Steps;
    Steps.AddDefaulted();

    constexpr float DefaultRequiredWork = 37.f;
    constexpr float TutorialRequiredWork = 10.f;
    TestEqual(
        TEXT("Normal gameplay keeps the authored duration"),
        USFTutorialSubsystem::ResolveBuildingRequiredWork(World, DefaultRequiredWork),
        DefaultRequiredWork);

    TestTrue(TEXT("Stage02 guide starts"), Tutorial->BeginGuide(Table, TEXT("Stage02"), 0, Steps));
    TestEqual(
        TEXT("Stage02 tutorial construction and upgrades take 10 seconds"),
        USFTutorialSubsystem::ResolveBuildingRequiredWork(World, DefaultRequiredWork),
        TutorialRequiredWork);
    Tutorial->CancelGuide();
    TestEqual(
        TEXT("Stage02 timing returns to normal after the guide"),
        USFTutorialSubsystem::ResolveBuildingRequiredWork(World, DefaultRequiredWork),
        DefaultRequiredWork);

    TestTrue(TEXT("Stage03 guide starts"), Tutorial->BeginGuide(Table, TEXT("Stage03"), 0, Steps));
    TestEqual(
        TEXT("Stage03 tutorial construction and upgrades take 10 seconds"),
        USFTutorialSubsystem::ResolveBuildingRequiredWork(World, DefaultRequiredWork),
        TutorialRequiredWork);
    Tutorial->CancelGuide();

    TestTrue(TEXT("Stage01 guide starts"), Tutorial->BeginGuide(Table, TEXT("Stage01"), 0, Steps));
    TestEqual(
        TEXT("Stage01 keeps its authored duration"),
        USFTutorialSubsystem::ResolveBuildingRequiredWork(World, DefaultRequiredWork),
        DefaultRequiredWork);
    Tutorial->CancelGuide();

    World->DestroyWorld(false);
    return true;
}
#endif
