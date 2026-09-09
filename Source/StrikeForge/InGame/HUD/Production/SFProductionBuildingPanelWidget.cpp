#include "SFProductionBuildingPanelWidget.h"

#include "SFProductionUnitEntryWidget.h"

#include "Buildings/Building/ProductBuilding/SFProductionBuilding.h"
#include "Buildings/SFRootBase.h"
#include "Components/VerticalBox.h"
#include "Datas/Unit/UnitStatData.h"
#include "GameFramework/Actor.h"
#include "Logging/SFLog.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "Game/GameState/SFGameState.h"
#include "GameFramework/PlayerController.h"
#include "Components/VerticalBoxSlot.h"
#include "ActorComponent/Product/ProductComponent.h"
#include "ActorComponent/Product/ProductInfo.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

#include "SFProductionUIDataRow.h"
#include "Game/Subsystem/SFDataTableManager.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "Kismet/GameplayStatics.h"

void USFProductionBuildingPanelWidget::NativeConstruct()
{
    Super::NativeConstruct();

    QueueSlots =
    {
        QueueIcon01,
        QueueIcon02,
        QueueIcon03,
        QueueIcon04,
        QueueIcon05
    };

    QueueButton01->OnClicked.AddUniqueDynamic(
        this,
        &USFProductionBuildingPanelWidget::HandleQueueButton01Clicked
    );

    QueueButton02->OnClicked.AddUniqueDynamic(
        this,
        &USFProductionBuildingPanelWidget::HandleQueueButton02Clicked
    );

    QueueButton03->OnClicked.AddUniqueDynamic(
        this,
        &USFProductionBuildingPanelWidget::HandleQueueButton03Clicked
    );

    QueueButton04->OnClicked.AddUniqueDynamic(
        this,
        &USFProductionBuildingPanelWidget::HandleQueueButton04Clicked   
    );

    QueueButton05->OnClicked.AddUniqueDynamic(
        this,
        &USFProductionBuildingPanelWidget::HandleQueueButton05Clicked
    );

    HideProductionBuilding();
}

void USFProductionBuildingPanelWidget::ShowProductionBuilding(
    AActor* InProductionBuilding
)
{
    if (!IsValid(InProductionBuilding))
    {
        HideProductionBuilding();
        return;
    }

    if (
        !InProductionBuilding->GetClass()->ImplementsInterface(
            UProductable::StaticClass()
        )
        )
    {
        SF_LOG(
            Warning,
            "[SFProductionBuildingPanelWidget] "
            "Selected actor is not Productable."
        );

        HideProductionBuilding();
        return;
    }

    CurrentProductionBuilding =
        InProductionBuilding;

    BindProductionBuilding(
        Cast<ASFProductionBuilding>(
            InProductionBuilding
        )
    );

    const FProductableData ProductableData =
        IProductable::Execute_GetProductableData(InProductionBuilding);

    CurrentProductableUnitRowNames =
        ProductableData.ProductableUnitFNames;

    BindProductComponent(
        ProductableData.ProductComponent.Get()
    );

    RefreshProductionQueue(
        ProductableData.CurrentProductingFNames
    );

    RefreshProductionUnitEntries(
        ProductableData
    );

    SetVisibility(
        ESlateVisibility::Visible
    );

    if (IsValid(CurrentProductComponent))
    {
        RefreshProductionProgress(
            CurrentProductComponent->GetCurrentProductRatio()
        );
    }

    SF_LOG(
        Log,
        "[SFProductionBuildingPanelWidget] "
        "Production building shown: %s",
        *InProductionBuilding->GetName()
    );
}

void USFProductionBuildingPanelWidget::HideProductionBuilding()
{
    UnbindProductionBuilding();
    UnbindProductComponent();

    CurrentProductionBuilding.Reset();
    CurrentProductableUnitRowNames.Reset();

    if (IsValid(UnitEntryBox))
    {
        UnitEntryBox->ClearChildren();
    }

    RefreshProductionQueue(
        TArray<FProductInfo>()
    );

    SetVisibility(
        ESlateVisibility::Collapsed
    );

    RefreshProductionProgress(0.0f);
}

bool USFProductionBuildingPanelWidget::HasProductionBuilding() const
{
    return CurrentProductionBuilding.IsValid();
}

AActor*
USFProductionBuildingPanelWidget::GetProductionBuilding() const
{
    return CurrentProductionBuilding.Get();
}

bool USFProductionBuildingPanelWidget::RequestProductionBySlot(int32 SlotIndex)
{
    if (!CurrentProductionBuilding.IsValid())
    {
        return false;
    }

    if (!CurrentProductableUnitRowNames.IsValidIndex(SlotIndex))
    {
        return false;
    }

    const FName UnitRowName =
        CurrentProductableUnitRowNames[SlotIndex];

    if (UnitRowName.IsNone())
    {
        return false;
    }

    HandleProductionUnitClicked(UnitRowName);
    return true;
}

void USFProductionBuildingPanelWidget::RefreshProductionUnitEntries(
    const FProductableData& ProductableData
)
{
    if (
        !IsValid(UnitEntryBox) ||
        !IsValid(ProductionUnitEntryWidgetClass)
        )
    {
        return;
    }

    UnitEntryBox->ClearChildren();

    for (int32 Index = 0;
        Index < ProductableData.ProductableUnitFNames.Num();
        ++Index)
    {
        const FName& UnitRowName =
            ProductableData.ProductableUnitFNames[Index];

        USFProductionUnitEntryWidget* EntryWidget =
            CreateWidget<USFProductionUnitEntryWidget>(
                GetOwningPlayer(),
                ProductionUnitEntryWidgetClass
            );

        if (!IsValid(EntryWidget))
        {
            continue;
        }

        EntryWidget->InitializeProductionUnit(
            UnitRowName
        );

        EntryWidget->OnProductionUnitClicked.AddUniqueDynamic(
            this,
            &USFProductionBuildingPanelWidget::HandleProductionUnitClicked
        );

        UVerticalBoxSlot* EntrySlot =
            UnitEntryBox->AddChildToVerticalBox(
                EntryWidget
            );

        if (IsValid(EntrySlot))
        {
            const bool bIsLastEntry =
                Index ==
                ProductableData.ProductableUnitFNames.Num() - 1;

            EntrySlot->SetPadding(
                FMargin(
                    0.0f,
                    0.0f,
                    0.0f,
                    bIsLastEntry ? 0.0f : 10.0f
                )
            );

            EntrySlot->SetHorizontalAlignment(
                HAlign_Fill
            );

            EntrySlot->SetVerticalAlignment(
                VAlign_Center
            );
        }

        SF_LOG(
            Log,
            "[SFProductionBuildingPanelWidget] "
            "Production Entry Created: %s",
            *UnitRowName.ToString()
        );
    }
}

void USFProductionBuildingPanelWidget::HandleProductionUnitClicked(
    FName UnitRowName
)
{
    AActor* ProductionBuilding =
        CurrentProductionBuilding.Get();

    if (!IsValid(ProductionBuilding))
    {
        return;
    }

    if (
        !ProductionBuilding->GetClass()->ImplementsInterface(
            UProductable::StaticClass()
        )
        )
    {
        return;
    }

    ASFRootBase* RootBuilding =
        Cast<ASFRootBase>(ProductionBuilding);

    UGameInstance* GameInstance =
        GetGameInstance();

    USFDataTableManager* DataTableManager =
        IsValid(GameInstance)
        ? GameInstance->GetSubsystem<USFDataTableManager>()
        : nullptr;

    const FUnitStatData* UnitStatsRow =
        IsValid(DataTableManager)
        ? DataTableManager->GetDataRow<FUnitStatData>(
            EDataTableType::Unit,
            UnitRowName,
            TEXT("HandleProductionUnitClicked")
        )
        : nullptr;

    ASFGameState* GameState =
        GetWorld()
        ? GetWorld()->GetGameState<ASFGameState>()
        : nullptr;

    if (
        IsValid(RootBuilding) &&
        UnitStatsRow &&
        IsValid(GameState)
        )
    {
        const int32 TeamIndex =
            RootBuilding->GetTeamIndex();

        const bool bCanAfford =
            GameState->CanAffordResource(
                TeamIndex,
                UnitStatsRow->UnitProductionCost
            );

        if (!bCanAfford)
        {
            OnProductionResourceWarningRequested.Broadcast();
            return;
        }
    }

    SF_LOG(
        Log,
        "[SFProductionBuildingPanelWidget] "
        "Production Requested: %s",
        *UnitRowName.ToString()
    );

    if (IsValid(UnitProductionClickSound))
    {
        UGameplayStatics::PlaySound2D(
            this,
            UnitProductionClickSound
        );
    }

    IProductable::Execute_ProductUnits(
        ProductionBuilding,
        UnitRowName
    );
}

void USFProductionBuildingPanelWidget::BindProductComponent(
    UProductComponent* InProductComponent
)
{
    UnbindProductComponent();

    if (!IsValid(InProductComponent))
    {
        return;
    }

    CurrentProductComponent =
        InProductComponent;

    CurrentProductComponent->OnProductLineChanged.AddUniqueDynamic(
        this,
        &USFProductionBuildingPanelWidget::HandleProductLineChanged
    );

    CurrentProductComponent->OnProductRatioChanged.AddUniqueDynamic(
        this,
        &USFProductionBuildingPanelWidget::HandleProductRatioChanged
    );

}

void USFProductionBuildingPanelWidget::UnbindProductComponent()
{
    if (!IsValid(CurrentProductComponent))
    {
        CurrentProductComponent = nullptr;
        return;
    }

    CurrentProductComponent->OnProductLineChanged.RemoveDynamic(
        this,
        &USFProductionBuildingPanelWidget::HandleProductLineChanged
    );

    CurrentProductComponent->OnProductRatioChanged.RemoveDynamic(
        this,
        &USFProductionBuildingPanelWidget::HandleProductRatioChanged
    );

    CurrentProductComponent = nullptr;
}

void USFProductionBuildingPanelWidget::BindProductionBuilding(
    ASFProductionBuilding* InProductionBuilding
)
{
    UnbindProductionBuilding();

    if (!IsValid(InProductionBuilding))
    {
        return;
    }

    BoundProductionBuilding =
        InProductionBuilding;

    BoundProductionBuilding->OnSpawnableUnitsChanged
        .AddUniqueDynamic(
            this,
            &USFProductionBuildingPanelWidget::HandleSpawnableUnitsChanged
        );
}

void USFProductionBuildingPanelWidget::UnbindProductionBuilding()
{
    if (!BoundProductionBuilding.IsValid())
    {
        BoundProductionBuilding = nullptr;
        return;
    }

    BoundProductionBuilding->OnSpawnableUnitsChanged
        .RemoveDynamic(
            this,
            &USFProductionBuildingPanelWidget::HandleSpawnableUnitsChanged
        );

    BoundProductionBuilding = nullptr;
}

void USFProductionBuildingPanelWidget::HandleProductLineChanged(
    TArray<FProductInfo> ProductLine
)
{
    RefreshProductionQueue(
        ProductLine
    );

    SF_LOG(
        Log,
        "[SFProductionBuildingPanelWidget] "
        "Queue Changed | Count: %d",
        ProductLine.Num()
    );
}

void USFProductionBuildingPanelWidget::RefreshProductionQueue(
    const TArray<FProductInfo>& ProductLine
)
{
    UGameInstance* GameInstance =
        GetGameInstance();

    if (!IsValid(GameInstance))
    {
        return;
    }

    USFDataTableManager* DataTableManager =
        GameInstance->GetSubsystem<USFDataTableManager>();

    if (!IsValid(DataTableManager))
    {
        return;
    }

    for (int32 Index = 0;
        Index < QueueSlots.Num();
        ++Index)
    {
        UImage* QueueSlot =
            QueueSlots[Index];

        if (!IsValid(QueueSlot))
        {
            continue;
        }

        if (!ProductLine.IsValidIndex(Index))
        {
            QueueSlot->SetVisibility(
                ESlateVisibility::Hidden
            );

            continue;
        }

        const FName UnitRowName =
            ProductLine[Index].RowName;

        const FSFProductionUIDataRow* UIData =
            DataTableManager->GetDataRow<FSFProductionUIDataRow>(
                EDataTableType::ProductionUI,
                UnitRowName,
                TEXT("RefreshProductionQueue")
            );

        if (!UIData)
        {
            QueueSlot->SetVisibility(
                ESlateVisibility::Hidden
            );

            continue;
        }

        UTexture2D* IconTexture =
            UIData->Icon.LoadSynchronous();

        if (!IsValid(IconTexture))
        {
            QueueSlot->SetVisibility(
                ESlateVisibility::Hidden
            );

            continue;
        }

        QueueSlot->SetBrushFromTexture(
            IconTexture
        );

        QueueSlot->SetVisibility(
            ESlateVisibility::Visible
        );
    }
}

void USFProductionBuildingPanelWidget::HandleProductRatioChanged(
    float ProductRatio
)
{
    RefreshProductionProgress(
        ProductRatio
    );
}

void USFProductionBuildingPanelWidget::RefreshProductionProgress(
    float ProductRatio
)
{
    if (!IsValid(ProductionProgressBar))
    {
        return;
    }

    ProductionProgressBar->SetPercent(
        FMath::Clamp(
            ProductRatio,
            0.0f,
            1.0f
        )
    );
}

void USFProductionBuildingPanelWidget::HandleQueueButton01Clicked()
{
    CancelProductionAtQueueIndex(0);
}

void USFProductionBuildingPanelWidget::HandleQueueButton02Clicked()
{
    CancelProductionAtQueueIndex(1);
}

void USFProductionBuildingPanelWidget::HandleQueueButton03Clicked()
{
    CancelProductionAtQueueIndex(2);
}

void USFProductionBuildingPanelWidget::HandleQueueButton04Clicked()
{
    CancelProductionAtQueueIndex(3);
}

void USFProductionBuildingPanelWidget::HandleQueueButton05Clicked()
{
    CancelProductionAtQueueIndex(4);
}   

void USFProductionBuildingPanelWidget::CancelProductionAtQueueIndex(
    int32 QueueIndex
)
{
    if (!IsValid(CurrentProductComponent))
    {
        return;
    }

    if (!CurrentProductComponent->GetProductLine().IsValidIndex(QueueIndex))
    {
        return;
    }

    CurrentProductComponent->RemoveProductInLine(QueueIndex);
}

void USFProductionBuildingPanelWidget::HandleSpawnableUnitsChanged(
    const TArray<FName>& SpawnableRowNames
)
{
    AActor* ProductionBuilding =
        CurrentProductionBuilding.Get();

    if (!IsValid(ProductionBuilding))
    {
        return;
    }

    const FProductableData ProductableData =
        IProductable::Execute_GetProductableData(
            ProductionBuilding
        );

    FProductableData UpdatedData =
        ProductableData;

    UpdatedData.ProductableUnitFNames =
        SpawnableRowNames;

    CurrentProductableUnitRowNames =
        SpawnableRowNames;

    RefreshProductionUnitEntries(
        UpdatedData
    );
}

bool USFProductionBuildingPanelWidget::
IsPointerOverProductionPanel() const
{
    APlayerController* PlayerController =
        GetOwningPlayer();

    if (!IsValid(PlayerController))
    {
        return false;
    }

    if (!IsVisible())
    {
        return false;
    }

    float MouseX = 0.f;
    float MouseY = 0.f;

    if (!PlayerController->GetMousePosition(MouseX, MouseY))
    {
        return false;
    }

    const FVector2D MousePosition(
        MouseX,
        MouseY
    );

    const bool bOverProductionPanel =
        GetCachedGeometry().IsUnderLocation(
            MousePosition
        );

    if (bOverProductionPanel)
    {
        return true;
    }

    if (IsValid(UnitEntryBox) &&
        UnitEntryBox->IsVisible())
    {
        return UnitEntryBox->GetCachedGeometry()
            .IsUnderLocation(MousePosition);
    }

    return false;
}