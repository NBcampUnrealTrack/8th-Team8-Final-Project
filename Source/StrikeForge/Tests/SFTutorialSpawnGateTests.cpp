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
#endif
