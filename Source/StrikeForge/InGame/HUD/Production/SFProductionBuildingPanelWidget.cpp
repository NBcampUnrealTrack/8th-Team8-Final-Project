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
#include "SFProductionQueueFeedbackWidget.h"
#include "Components/ButtonSlot.h"

#include "SFProductionUIDataRow.h"
#include "Game/Subsystem/SFDataTableManager.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "Kismet/GameplayStatics.h"

UWidget* USFProductionBuildingPanelWidget::FindTutorialProductionButton(FName UnitRowName) const
{
    if (!HasProductionBuilding() || !IsVisible() || !UnitEntryBox) return nullptr;
    for (UWidget* Child : UnitEntryBox->GetAllChildren())
    {
        const USFProductionUnitEntryWidget* Entry = Cast<USFProductionUnitEntryWidget>(Child);
        if (Entry && Entry->GetProductionRowName() == UnitRowName)
            return Entry->GetTutorialProductionButton();
    }
    return nullptr;
}

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

    QueueButtons = {QueueButton01, QueueButton02, QueueButton03, QueueButton04, QueueButton05};
    for (UButton* Button : QueueButtons)
    {
        if (!Button) continue;
        FButtonStyle Style = Button->GetStyle();
        Style.Disabled = Style.Normal;
        Style.Disabled.TintColor = FSlateColor(FLinearColor(.62f,.62f,.62f,1.f));
        Style.Hovered = Style.Normal;
        Style.Pressed = Style.Normal;
        Style.Pressed.TintColor = FSlateColor(FLinearColor(.8f,.72f,.64f,1.f));
        Button->SetStyle(Style);
        Button->SetToolTipText(NSLOCTEXT("SFProduction", "CancelHint", "클릭하여 생산 취소 · 비용 반환"));
    }
    QueueFeedback.Reset();
    for (UButton* Button : QueueButtons)
    {
        auto* Feedback=CreateWidget<USFProductionQueueFeedbackWidget>(GetOwningPlayer());
        QueueFeedback.Add(Feedback);
        if (!Button || !Feedback) continue;
        Feedback->InitializeContent(Button->GetContent());
        Button->SetContent(Feedback);
        FButtonStyle Style=Button->GetStyle();
        Style.NormalPadding=Style.PressedPadding=FMargin(0);
        Button->SetStyle(Style);
        if (auto* ContentSlot=Cast<UButtonSlot>(Feedback->Slot))
        {
            ContentSlot->SetHorizontalAlignment(HAlign_Fill);
            ContentSlot->SetVerticalAlignment(VAlign_Fill);
        }
    }
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

    if (CurrentProductionBuilding.Get() != InProductionBuilding)
        for (int32 I=0;I<5;++I) { QueueHover[I]=0.f; QueueCancelFlash[I]=0.f; }
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

    for (int32 I=0;I<5;++I) { QueueHover[I]=0.f; QueueCancelFlash[I]=0.f; }
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

bool USFProductionBuildingPanelWidget::RequestProductionBySlot(
    int32 SlotIndex
)
{
    const FName UnitRowName =
        GetUnitRowNameBySlot(SlotIndex);

    return RequestProductionOnBuilding(
        CurrentProductionBuilding.Get(),
        UnitRowName
    );
}

FName USFProductionBuildingPanelWidget::GetUnitRowNameBySlot(
    int32 SlotIndex
) const
{
    if (!CurrentProductableUnitRowNames.IsValidIndex(SlotIndex))
    {
        return NAME_None;
    }

    return CurrentProductableUnitRowNames[SlotIndex];
}

bool USFProductionBuildingPanelWidget::RequestProductionOnBuilding(
    AActor* InProductionBuilding,
    FName UnitRowName
)
{
    const bool bAccepted = TryRequestProduction(InProductionBuilding, UnitRowName);
    if (CurrentProductionBuilding.Get() == InProductionBuilding && UnitEntryBox)
        for (UWidget* Child : UnitEntryBox->GetAllChildren())
            if (auto* Entry = Cast<USFProductionUnitEntryWidget>(Child))
                if (Entry->GetProductionRowName() == UnitRowName) Entry->ShowRequestFeedback(bAccepted);
    return bAccepted;
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
    RequestProductionOnBuilding(CurrentProductionBuilding.Get(), UnitRowName);
}

bool USFProductionBuildingPanelWidget::TryRequestProduction(
    AActor* ProductionBuilding,
    FName UnitRowName
)
{
    if (!IsValid(ProductionBuilding) || UnitRowName.IsNone())
    {
        return false;
    }

    if (
        !ProductionBuilding->GetClass()->ImplementsInterface(
            UProductable::StaticClass()
        )
        )
    {
        return false;
    }

    const FProductableData ProductableData =
        IProductable::Execute_GetProductableData(
            ProductionBuilding
        );

    if (!ProductableData.ProductableUnitFNames.Contains(UnitRowName))
    {
        return false;
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
            TEXT("TryRequestProduction")
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

        if (
            !GameState->CanAffordResource(
                TeamIndex,
                UnitStatsRow->UnitProductionCost
            )
            )
        {
            OnProductionResourceWarningRequested.Broadcast();
            return false;
        }
    }

    UProductComponent* Component = ProductableData.ProductComponent.Get();
    const int32 PreviousCount = IsValid(Component) ? Component->GetProductLine().Num() : 0;
    IProductable::Execute_ProductUnits(ProductionBuilding, UnitRowName);
    // Full queue, population limit, construction and tutorial restrictions can reject a request.
    const bool bAccepted = IsValid(Component) && Component->GetProductLine().Num() > PreviousCount;
    if (!bAccepted) return false;
    if (IsValid(UnitProductionClickSound))
        UGameplayStatics::PlaySound2D(this, UnitProductionClickSound);

    return true;
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
    OccupiedQueueCount = FMath::Min(5, ProductLine.Num());
    for (int32 I=0;I<QueueButtons.Num();++I)
        if (QueueButtons[I])
        {
            QueueButtons[I]->SetIsEnabled(I < OccupiedQueueCount);
            QueueButtons[I]->SetToolTipText(I < OccupiedQueueCount
                ? NSLOCTEXT("SFProduction", "CancelHint", "클릭하여 생산 취소 · 비용 반환") : FText::GetEmpty());
        }

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
            ESlateVisibility::HitTestInvisible
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
    PresentedProgress = FMath::Clamp(ProductRatio, 0.f, 1.f);
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
    if (QueueIndex < 5) QueueCancelFlash[QueueIndex] = .32f;
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

void USFProductionBuildingPanelWidget::NativeDestruct()
{
    UnbindProductionBuilding();
    UnbindProductComponent();
    for (int32 I=0; I<QueueFeedback.Num(); ++I)
        if (QueueButtons.IsValidIndex(I) && QueueButtons[I] && QueueFeedback[I])
            QueueButtons[I]->SetContent(QueueFeedback[I]->ReleaseContent());
    QueueFeedback.Reset();
    Super::NativeDestruct();
}

void USFProductionBuildingPanelWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    for (int32 I=0; I<QueueButtons.Num(); ++I)
    {
        bool bHover = I < OccupiedQueueCount && QueueButtons[I] && QueueButtons[I]->IsHovered();
#if WITH_DEV_AUTOMATION_TESTS
        bHover |= I < OccupiedQueueCount && PreviewQueueHover == I;
#endif
        QueueHover[I] = FMath::FInterpTo(QueueHover[I], bHover ? 1.f : 0.f, DeltaTime, 18.f);
        QueueCancelFlash[I] = FMath::Max(0.f, QueueCancelFlash[I] - DeltaTime);
        if (QueueFeedback.IsValidIndex(I) && QueueFeedback[I])
            QueueFeedback[I]->SetPresentation(QueueHover[I],QueueCancelFlash[I]/.32f,
                I<OccupiedQueueCount,I==0,PresentedProgress);
    }
}
