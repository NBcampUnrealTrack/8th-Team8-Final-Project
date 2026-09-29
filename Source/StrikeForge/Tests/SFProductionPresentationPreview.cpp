#if WITH_DEV_AUTOMATION_TESTS
#include "InGame/HUD/Production/SFProductionBuildingPanelWidget.h"
#include "InGame/HUD/Production/SFProductionUnitEntryWidget.h"
#include "InGame/HUD/Tutorial/SFTutorialSubsystem.h"
#include "ActorComponent/Product/ProductComponent.h"
#include "Buildings/Building/ProductBuilding/SFProductionBuilding.h"
#include "Datas/Building/BuildingDefinition/SFProductionBuildingDefinition.h"
#include "Game/Spawn/SFBuildingSpawnLibrary.h"
#include "Game/GameState/SFGameState.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/VerticalBox.h"
#include "Components/Button.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/BufferArchive.h"
#include "Slate/WidgetRenderer.h"

// Explicit development command only: a disposable PIE fixture, never saved to a map.
static TWeakObjectPtr<ASFProductionBuilding> ProductionFixture;
static TWeakObjectPtr<USFProductionBuildingPanelWidget> ProductionPanel;
static FAutoConsoleCommandWithWorldAndArgs SFProductionQA(
    TEXT("SF.Production.QA"), TEXT("PIE only: setup <tier 1-3>, hover <entry> <queue>, check, capture <name>."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World)
    {
        if (!World || World->WorldType != EWorldType::PIE || Args.IsEmpty()) return;
        if (Args[0]==TEXT("setup"))
        {
            if (ProductionFixture.IsValid()) ProductionFixture->Destroy();
            auto* Definition=LoadObject<USFProductionBuildingDefinition>(nullptr,TEXT("/Game/Core/Primary/Buildings/Definitions/Animal/DA_AnimalBarrack.DA_AnimalBarrack"));
            auto* GS=World->GetGameState<ASFGameState>();
            if (!Definition || !GS) return;
            const int32 Tier=Args.Num()>1 ? FCString::Atoi(*Args[1])-1 : 2;
            TArray<FSoftObjectPath> Paths;
            if (!Definition->GetRuntimeAssetPaths(Tier,Paths)) return;
            for (const auto& Path:Paths) Path.TryLoad();
            if (auto* Tutorial=USFTutorialSubsystem::Get(World)) { Tutorial->CancelObjectiveWait(); Tutorial->CancelGuide(); }
            GS->AddResource(0,EResourceType::Mineral,10000);
            GS->AddResource(0,EResourceType::Ether,10000);
            GS->AddMaxPopulation(0,100);
            for (int32 I=0; I<8 && !ProductionFixture.IsValid();++I)
            {
                ProductionFixture=Cast<ASFProductionBuilding>(USFBuildingSpawnLibrary::SpawnBuildingFromDefinition(World,Definition,0,
                    FTransform(FVector(-1800+I*650,1800,100)),ESFBuildingSpawnMode::ViewPortPlacedMode,Tier));
            }
            if (!ProductionFixture.IsValid()) { UE_LOG(LogTemp,Error,TEXT("PRODUCTION_QA spawn failed")); return; }
            if (!ProductionPanel.IsValid() || ProductionPanel->GetWorld()!=World)
            {
                auto* Class=LoadClass<USFProductionBuildingPanelWidget>(nullptr,TEXT("/Game/Game/IN_UI/HUD/WBP/Production/Production/WBP_SFBuildProductionPanel.WBP_SFBuildProductionPanel_C"));
                ProductionPanel=CreateWidget<USFProductionBuildingPanelWidget>(World->GetFirstPlayerController(),Class);
                ProductionPanel->AddToViewport(200);
                ProductionPanel->SetDesiredSizeInViewport(FVector2D(952,350));
                ProductionPanel->SetPositionInViewport(FVector2D(20,20),false);
            }
            ProductionPanel->ShowProductionBuilding(ProductionFixture.Get());
            auto* Component=ProductionFixture->FindComponentByClass<UProductComponent>();
            if (Component) Component->SetSpeedModifier(TEXT("SFProductionPreview"),0.f);
            const FProductableData Data=IProductable::Execute_GetProductableData(ProductionFixture.Get());
            UE_LOG(LogTemp,Display,TEXT("PRODUCTION_QA tier=%d entries=%d active=%d"),Tier+1,Data.ProductableUnitFNames.Num(),ProductionFixture->IsTierActivated());
        }
        if (!ProductionPanel.IsValid()) return;
        auto* Panel=ProductionPanel.Get();
        if (Args[0]==TEXT("hover"))
        {
            auto* Box=Cast<UVerticalBox>(Panel->GetWidgetFromName(TEXT("UnitEntryBox")));
            for (int32 I=0; Box && I<Box->GetChildrenCount();++I)
                CastChecked<USFProductionUnitEntryWidget>(Box->GetChildAt(I))->bPreviewHover=Args.Num()>1 && I==FCString::Atoi(*Args[1]);
            Panel->PreviewQueueHover=Args.Num()>2 ? FCString::Atoi(*Args[2]) : INDEX_NONE;
        }
        if (Args[0]==TEXT("check") && ProductionFixture.IsValid())
        {
            auto* Component=ProductionFixture->FindComponentByClass<UProductComponent>();
            auto* GS=World->GetGameState<ASFGameState>();
            Component->AllClearProductLine();
            const int32 Before=GS->GetMineral(0);
            bool Passed=Panel->RequestProductionBySlot(0);
            const int32 UnitCost=Before-GS->GetMineral(0);
            for(int32 I=1;I<5;++I) Passed &= Panel->RequestProductionBySlot(0);
            Passed &= Component->GetProductLine().Num()==5;
            Passed &= !Panel->RequestProductionBySlot(0);
            const int32 BeforeCancel=GS->GetMineral(0);
            Panel->QueueButton03->OnClicked.Broadcast();
            Passed &= Component->GetProductLine().Num()==4 && GS->GetMineral(0)==BeforeCancel+UnitCost;
            Panel->QueueButton01->OnClicked.Broadcast();
            Passed &= Component->GetProductLine().Num()==3;
            Panel->QueueButton01->OnClicked.Broadcast();
            Panel->QueueButton01->OnClicked.Broadcast();
            Panel->QueueButton01->OnClicked.Broadcast();
            Passed &= Component->GetProductLine().IsEmpty() && !Panel->QueueButton01->GetIsEnabled();
            Passed &= GS->GetMineral(0)==Before;
            Panel->QueueButton05->OnClicked.Broadcast();
            Passed &= Component->GetProductLine().IsEmpty();
            GS->AddResource(0,EResourceType::Mineral,-Before);
            Passed &= !Panel->RequestProductionBySlot(0);
            GS->AddResource(0,EResourceType::Mineral,Before);
            Panel->HideProductionBuilding();
            Passed &= !Panel->HasProductionBuilding() && !Panel->RequestProductionBySlot(0);
            Panel->ShowProductionBuilding(ProductionFixture.Get());
            auto* Entries=Cast<UVerticalBox>(Panel->GetWidgetFromName(TEXT("UnitEntryBox")));
            for (UWidget* Child:Entries->GetAllChildren())
            {
                auto* Entry=CastChecked<USFProductionUnitEntryWidget>(Child);
                auto* Button=CastChecked<UButton>(Entry->GetWidgetFromName(TEXT("ProductionButton")));
                const int32 Count=Component->GetProductLine().Num();
                Button->OnClicked.Broadcast();
                Passed &= Component->GetProductLine().Num()==Count+1;
            }
            while (Component->GetProductLine().Num()<3)
                if (!Panel->RequestProductionBySlot(0)) break;
            Panel->RefreshProductionProgress(.42f);
            UE_LOG(LogTemp,Display,TEXT("PRODUCTION_QA_CHECK passed=%d tier=%d cost=%d queue=%d"),Passed,ProductionFixture->GetCurrentTier()+1,UnitCost,Component->GetProductLine().Num());
        }
        if (Args[0]==TEXT("capture") && Args.Num()>1)
        {
            auto* Renderer=new FWidgetRenderer(true);
            auto* Target=Renderer->DrawWidget(Panel->TakeWidget(),FVector2D(952,350));
            FBufferArchive Bytes;
            if(Target && FImageUtils::ExportRenderTarget2DAsPNG(Target,Bytes))
                FFileHelper::SaveArrayToFile(Bytes,*(FPaths::ProjectSavedDir()/TEXT("ProductionPolish")/(FPaths::GetCleanFilename(Args[1])+TEXT(".png"))));
            BeginCleanup(Renderer);
            auto* Box=Cast<UVerticalBox>(Panel->GetWidgetFromName(TEXT("UnitEntryBox")));
            for(int32 I=0;Box && I<Box->GetChildrenCount();++I)
                UE_LOG(LogTemp,Display,TEXT("PRODUCTION_QA_GEOMETRY entry=%d size=%s"),I,*Box->GetChildAt(I)->GetCachedGeometry().GetLocalSize().ToString());
        }
    }));
#endif
