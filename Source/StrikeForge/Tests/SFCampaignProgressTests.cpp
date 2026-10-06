#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "UI/LobbyWidget.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCampaignProgressTest, "StrikeForge.Campaign.ProgressAndRaceChoice",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFCampaignProgressTest::RunTest(const FString& Parameters)
{
    UClass* Class = LoadClass<USFGameInstance>(nullptr, TEXT("/Game/Core/Blueprints/GameInstance/BP_SFGameInstance.BP_SFGameInstance_C"));
    if (!TestNotNull(TEXT("Campaign game instance"), Class)) return false;
    USFGameInstance* GI = NewObject<USFGameInstance>(GEngine, Class);
    GI->InitializeStandalone();
    UWorld* World = GI->GetWorld();
    TestEqual(TEXT("Stage05 starts locked"), GI->GetStageProgressState(TEXT("Stage05")), ESFStageProgressState::Locked);
    for (int32 Order = 1; Order <= 4; ++Order)
        TestTrue(TEXT("Sequential clear recorded"), GI->MarkStageCleared(FName(*FString::Printf(TEXT("Stage%02d"), Order))));
    TestEqual(TEXT("Stage04 clear unlocks Stage05"), GI->GetStageProgressState(TEXT("Stage05")), ESFStageProgressState::Unlocked);
    TestTrue(TEXT("Stage05 selects successfully"), GI->SelectStage(TEXT("Stage05")));
    TestEqual(TEXT("Stage05 uses Etherian"), GI->GetSelectedRace(), ESFRace::Human);
    TestTrue(TEXT("Stage05 clear unlocks Stage06"), GI->MarkStageCleared(TEXT("Stage05")));
    TestTrue(TEXT("Stage06 selectable"), GI->SelectStage(TEXT("Stage06")));
    TestTrue(TEXT("Stage06 clear unlocks Stage07"), GI->MarkStageCleared(TEXT("Stage06")));
    TestTrue(TEXT("Stage07 selectable"), GI->SelectStage(TEXT("Stage07")));
    TestEqual(TEXT("Stage07 requires an explicit choice"), GI->GetSelectedRace(), ESFRace::None);
    TestFalse(TEXT("Travel blocked until race chosen"), GI->MoveToStage());
    TestTrue(TEXT("Beast choice supported"), GI->SelectRace(ESFRace::Beast));
    TestTrue(TEXT("Reopening selection succeeds"), GI->SelectStage(TEXT("Stage07")));
    TestEqual(TEXT("Reopening clears prior choice"), GI->GetSelectedRace(), ESFRace::None);
    TestTrue(TEXT("Etherian choice supported"), GI->SelectRace(ESFRace::Human));
    UClass* LobbyClass = LoadClass<ULobbyWidget>(nullptr, TEXT("/Game/Core/UI/WBP_Lobby.WBP_Lobby_C"));
    if (TestNotNull(TEXT("Lobby blueprint"), LobbyClass))
    {
        FArrayProperty* Rows = FindFProperty<FArrayProperty>(ULobbyWidget::StaticClass(), TEXT("StageRowNames"));
        FScriptArrayHelper Values(Rows, Rows->ContainerPtrToValuePtr<void>(LobbyClass->GetDefaultObject()));
        TestEqual(TEXT("Seven lobby stages"), Values.Num(), 7);
        if (Values.Num() == 7) TestEqual(TEXT("Saved fifth button uses the canonical row"), *reinterpret_cast<FName*>(Values.GetRawPtr(4)), FName(TEXT("Stage05")));
    }
    GI->Shutdown();
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
