#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Game/GameState/SFGameState.h"
#include "UObject/Script.h"
#include "Datas/Stage/SFStageData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFStageBuildingUnlockTest,
    "StrikeForge.Stage.Building.UnlockAfterTutorial",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFStageBuildingUnlockTest::RunTest(const FString& Parameters)
{
    FEditorScriptExecutionGuard ScriptGuard;
    UWorld::InitializationValues Values;
    Values.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("Test world"), World)) return false;

    ASFGameState* GameState = World->SpawnActor<ASFGameState>();
    if (!TestNotNull(TEXT("Game state"), GameState))
    {
        World->DestroyWorld(false);
        return false;
    }

    FSFStageBuildingRule House;
    House.BuildingId = TEXT("House");
    FSFStageBuildingRule Temple;
    Temple.BuildingId = TEXT("Temple");
    Temple.bCanBuild = false;
    Temple.RequiredBuildingIds.Add(TEXT("House"));

    TArray<FSFStageBuildingRule> Rules;
    Rules.Add(House);
    Rules.Add(Temple);

    TestTrue(TEXT("Stage starts loading"), GameState->BeginRaceDataLoading());
    TestTrue(TEXT("Stage rules initialize"), GameState->InitializeStage(TEXT("Stage02"), Rules));
    TestTrue(TEXT("Unrestricted building is initially allowed"),
        GameState->IsBuildingAllowedInCurrentStage(House.BuildingId));
    TestFalse(TEXT("Tutorial stage restriction blocks Temple"),
        GameState->IsBuildingAllowedInCurrentStage(Temple.BuildingId));

    GameState->ReleaseBuildingRestrictionsAfterTutorial();
    TestTrue(TEXT("Temple stage restriction ends with tutorial"),
        GameState->IsBuildingAllowedInCurrentStage(Temple.BuildingId));
    TestFalse(TEXT("Required completed House remains necessary"),
        GameState->AreBuildingRequirementsMet(0, Temple.BuildingId));
    GameState->RecordCompletedBuilding(0, TEXT("AnimalHouse"), House.BuildingId);
    TestTrue(TEXT("A completed House keeps the next building unlocked after it is gone"),
        GameState->AreBuildingRequirementsMet(0, Temple.BuildingId));
    TestFalse(TEXT("One team's completed House does not unlock another team"),
        GameState->AreBuildingRequirementsMet(1, Temple.BuildingId));
    TestFalse(TEXT("Invalid building ID is still rejected"),
        GameState->IsBuildingAllowedInCurrentStage(NAME_None));

    World->DestroyWorld(false);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFStage03TempleProgressionTest,
    "StrikeForge.Stage.Building.Stage03TempleProgression",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFStage03TempleProgressionTest::RunTest(const FString& Parameters)
{
    UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Core/DataTable/DT_SFStage.DT_SFStage"));
    const FSFStageData* Stage = Table ? Table->FindRow<FSFStageData>(TEXT("Stage03"), TEXT("Temple progression")) : nullptr;
    if (!TestNotNull(TEXT("Authored Stage03 row"), Stage)) return false;
    FEditorScriptExecutionGuard ScriptGuard;
    UWorld::InitializationValues Values;
    Values.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &Values);
    ASFGameState* State = World->SpawnActor<ASFGameState>();
    State->BeginRaceDataLoading();
    State->InitializeStage(TEXT("Stage03"), Stage->BuildingRules);
    // Stage03 starts with these buildings; both mines must spawn on usable resources.
    State->RecordCompletedBuilding(0, TEXT("AnimalHouse"), TEXT("House"));
    State->RecordCompletedBuilding(0, TEXT("AnimalMineralMine"), TEXT("MineralMining"));
    State->RecordCompletedBuilding(0, TEXT("AnimalBarrack"), TEXT("Barrack"));
    TestTrue(TEXT("Stage03 permits Temple construction"), State->IsBuildingAllowedInCurrentStage(TEXT("Temple")));
    TestFalse(TEXT("Academy must complete first"), State->AreBuildingRequirementsMet(0, TEXT("Temple")));
    State->RecordCompletedBuilding(0, TEXT("AnimalAcademy"), TEXT("Academy"));
    TestFalse(TEXT("A missing starting Ether mine must not bypass the authored prerequisite"),
        State->AreBuildingRequirementsMet(0, TEXT("Temple")));
    State->RecordCompletedBuilding(0, TEXT("AnimalEtherMine"), TEXT("EtherMining"));
    TestTrue(TEXT("Completing the guided Academy unlocks the next Temple step"),
        State->AreBuildingRequirementsMet(0, TEXT("Temple")));
    TestFalse(TEXT("Another team does not inherit the unlock"), State->AreBuildingRequirementsMet(1, TEXT("Temple")));
    World->DestroyWorld(false);
    return true;
}
#endif
