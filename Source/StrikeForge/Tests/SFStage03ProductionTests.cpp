#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ActorComponent/Product/ProductComponent.h"
#include "Buildings/Building/ProductBuilding/SFProductionBuilding.h"
#include "Datas/Building/BuildingDefinition/SFProductionBuildingDefinition.h"
#include "Datas/Unit/UnitStatData.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "Game/GameState/SFGameState.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "InGame/SFInGameHUDWidget.h"
#include "InGame/HUD/Production/SFProductionBuildingPanelWidget.h"
#include "InGame/HUD/Tutorial/SFTutorialSpotlightWidget.h"
#include "InGame/HUD/Tutorial/SFTutorialSubsystem.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Units/Unit/UnitBase.h"
#include "UObject/Script.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFStage03ProductionTest,
    "StrikeForge.Tutorial.Stage03.WarBeastProductionAndHighlight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFStage03ProductionTest::RunTest(const FString& Parameters)
{
    FEditorScriptExecutionGuard ScriptGuard;
    UDataTable* Units = LoadObject<UDataTable>(nullptr, TEXT("/Game/Core/DataTable/DT_Units.DT_Units"));
    UDataTable* Intro = LoadObject<UDataTable>(nullptr, TEXT("/Game/Game/IN_UI/DT/DT_SFStageIntro.DT_SFStageIntro"));
    if (!TestNotNull(TEXT("Units data"), Units) || !TestNotNull(TEXT("Intro data"), Intro)) return false;
    const FUnitStatData* WarBeast = Units->FindRow<FUnitStatData>(TEXT("WarBeast"), TEXT("Stage03Test"));
    const FSFStageIntroDataRow* Stage = Intro->FindRow<FSFStageIntroDataRow>(TEXT("Stage03"), TEXT("Stage03Test"));
    if (!WarBeast || !Stage || !Stage->Nodes.IsValidIndex(11)) return false;
    if (!TestNotNull(TEXT("WarBeast blueprint can load"), WarBeast->UnitClass.LoadSynchronous())) return false;

    USFGameInstance* Instance = NewObject<USFGameInstance>(GEngine);
    FindFProperty<FObjectProperty>(USFGameInstance::StaticClass(), TEXT("UnitsDataTable"))
        ->SetObjectPropertyValue_InContainer(Instance, Units);
    FindFProperty<FObjectProperty>(USFGameInstance::StaticClass(), TEXT("ProductionUIDataTable"))
        ->SetObjectPropertyValue_InContainer(Instance, LoadObject<UDataTable>(nullptr,
            TEXT("/Game/Core/DataTable/DT_SFProductionUI.DT_SFProductionUI")));
    FindFProperty<FObjectProperty>(USFGameInstance::StaticClass(), TEXT("UIDataTable"))
        ->SetObjectPropertyValue_InContainer(Instance, LoadObject<UDataTable>(nullptr,
            TEXT("/Game/Game/IN_UI/DT/DT_SFSelectionUI.DT_SFSelectionUI")));
    Instance->InitializeStandalone();
    UWorld* World = Instance->GetWorld();
    auto Cleanup = [Instance, World]()
    {
        Instance->Shutdown();
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    };
    ASFGameState* State = World->SpawnActor<ASFGameState>();
    World->SetGameState(State);
    State->FindOrAddTeam(0);
    State->AddResource(0, EResourceType::Mineral, 350);
    State->AddResource(0, EResourceType::Ether, 120);
    State->AddMaxPopulation(0, 51);
    USFTutorialSubsystem* Tutorial = USFTutorialSubsystem::Get(World);
    USFTutorialSubsystem::SetStageTutorialActive(World, true);
    USFTutorialSubsystem::SetStageFeatureLocks(World, Stage->LockedFeatures);
    TestTrue(TEXT("Screenshot balance cannot afford WarBeast"),
        !State->CanAffordResource(0, WarBeast->UnitProductionCost));
    TestTrue(TEXT("Actual Stage03 production guide begins"),
        Tutorial->BeginGuide(Intro, TEXT("Stage03"), 12, Stage->Nodes[11].GuideSteps));
    TestEqual(TEXT("Guide preserves the manually configured mineral balance"), State->GetMineral(0), 350);
    TestEqual(TEXT("Guide preserves the manually configured ether balance"), State->GetEther(0), 120);
    // Provide the real production cost explicitly in this fixture, as configured by the designer.
    State->AddResource(0, EResourceType::Ether, WarBeast->UnitProductionCost.Ether - 120);

    // A completed special factory fixture uses the real tier data and normal production component.
    USFProductionBuildingDefinition* Definition = LoadObject<USFProductionBuildingDefinition>(nullptr,
        TEXT("/Game/Core/Primary/Buildings/Definitions/Animal/DA_AnimalSpecial.DA_AnimalSpecial"));
    if (!Definition || !Definition->FindProductionTier(1)) { Cleanup(); return false; }
    ASFProductionBuilding* Building = World->SpawnActor<ASFProductionBuilding>();
    Building->SetBuildingDefinition(Definition);
    Building->SetInitialTier(1);
    Building->SetMyTeamIndex(0);
    FEnumProperty* BuildingState = FindFProperty<FEnumProperty>(ASFBaseBuilding::StaticClass(), TEXT("CurrentBuildingState"));
    BuildingState->GetUnderlyingProperty()->SetIntPropertyValue(
        BuildingState->ContainerPtrToValuePtr<void>(Building), static_cast<int64>(EBuildingState::Completed));
    Building->StoredSpawnableUnitDatas = Definition->FindProductionTier(1)->SpawnableUnits;
    TestTrue(TEXT("Objective permits WarBeast at the special factory"),
        USFTutorialSubsystem::IsUnitProductionAllowed(World, Building, TEXT("WarBeast")));
    TestFalse(TEXT("Objective still blocks the other special unit"),
        USFTutorialSubsystem::IsUnitProductionAllowed(World, Building, TEXT("SpiritShaman")));

    ASFInGamePlayerController* PC = World->SpawnActor<ASFInGamePlayerController>();
    ULocalPlayer* Local = NewObject<ULocalPlayer>(GEngine);
    Instance->AddLocalPlayer(Local, FPlatformUserId::CreateFromInternalId(0));
    PC->SetPlayer(Local);
    World->AddController(PC);
    UClass* PanelClass = LoadClass<USFProductionBuildingPanelWidget>(nullptr,
        TEXT("/Game/Game/IN_UI/HUD/WBP/Production/Production/WBP_SFBuildProductionPanel.WBP_SFBuildProductionPanel_C"));
    USFProductionBuildingPanelWidget* Panel = CreateWidget<USFProductionBuildingPanelWidget>(PC, PanelClass);
    if (!TestNotNull(TEXT("Real production panel"), Panel)) { Cleanup(); return false; }
    Panel->SetOwningPlayer(PC);
    TSharedRef<SWidget> PanelSlate = Panel->TakeWidget();
    Panel->ShowProductionBuilding(Building);
    UWidget* Button = Panel->FindTutorialProductionButton(TEXT("WarBeast"));
    if (!TestNotNull(TEXT("Highlight resolves the actual WarBeast button"), Button))
    {
        Cleanup();
        return false;
    }
    TestTrue(TEXT("WarBeast and SpiritShaman are separate targets"),
        Button != Panel->FindTutorialProductionButton(TEXT("SpiritShaman")));
    USFInGameHUDWidget* HUD = CreateWidget<USFInGameHUDWidget>(PC, USFInGameHUDWidget::StaticClass());
    HUD->SetOwningPlayer(PC);
    FindFProperty<FObjectProperty>(USFInGameHUDWidget::StaticClass(), TEXT("ProductionBuildingPanel"))
        ->SetObjectPropertyValue_InContainer(HUD, Panel);
    FindFProperty<FObjectProperty>(ASFInGamePlayerController::StaticClass(), TEXT("InGameHUDWidget"))
        ->SetObjectPropertyValue_InContainer(PC, HUD);
    UClass* SpotlightClass = LoadClass<USFTutorialSpotlightWidget>(nullptr,
        TEXT("/Game/Game/IN_UI/HUD/WBP/Intro/WBP_TutorialSpotlight.WBP_TutorialSpotlight_C"));
    USFTutorialSpotlightWidget* Spotlight = CreateWidget<USFTutorialSpotlightWidget>(PC, SpotlightClass);
    if (!TestNotNull(TEXT("Tutorial spotlight"), Spotlight)) { Cleanup(); return false; }
    Spotlight->SetOwningPlayer(PC);
    Spotlight->ApplyGuideStep(Stage->Nodes[11].GuideSteps[0], 0, 1);
    TestEqual(TEXT("Stage03 spotlight follows the required production button"), Spotlight->ResolveScreenTargetWidget(), Button);
    Panel->HideProductionBuilding();
    TestNull(TEXT("Hidden panel never leaves a misplaced production highlight"), Spotlight->ResolveScreenTargetWidget());
    Panel->ShowProductionBuilding(Building);
    Button = Panel->FindTutorialProductionButton(TEXT("WarBeast"));
    UButton* ProductionButton = Cast<UButton>(Button);
    if (!TestNotNull(TEXT("Production button is clickable"), ProductionButton)) { Cleanup(); return false; }
    ProductionButton->OnClicked.Broadcast();
    UProductComponent* Product = Building->FindComponentByClass<UProductComponent>();
    TestEqual(TEXT("Real button enqueues one WarBeast"), Product->GetProductLine().Num(), 1);
    TestEqual(TEXT("Production spends its normal ether cost"), State->GetEther(0), 0);
    TestEqual(TEXT("Tutorial production duration remains five seconds"),
        USFTutorialSubsystem::ResolveTutorialRequiredWork(World, WarBeast->UnitProductionDuration), 5.f);
    Product->SetSpeedModifier(TEXT("Automation"), 100.f);
    for (int32 Frame = 0; Frame < 5; ++Frame)
    {
        ++GFrameCounter;
        World->Tick(LEVELTICK_All, 0.2f);
    }
    int32 Produced = 0;
    for (TActorIterator<AUnitBase> It(World); It; ++It)
        if (It->GetUnitRowName() == TEXT("WarBeast")) ++Produced;
    TestEqual(TEXT("Queued WarBeast actually spawns"), Produced, 1);
    TestTrue(TEXT("Successful spawn completes the real tutorial objective"),
        Tutorial->IsObjectiveCompleted(TAG_SF_Tutorial_Unit_Produced_SpecialWarBeast));

    // Guide transitions preserve the resource balances chosen by the designer.
    USFTutorialSubsystem::SetStageTutorialActive(World, false);
    Tutorial->ResetCompletedObjectives();
    Tutorial->BeginGuide(Intro, TEXT("Stage03"), 12, Stage->Nodes[11].GuideSteps);
    TestEqual(TEXT("Outside tutorial, no resource help is granted"), State->GetEther(0), 0);
    Tutorial->CancelGuide();
    Panel->HideProductionBuilding();
    Cleanup();
    return true;
}
#endif
