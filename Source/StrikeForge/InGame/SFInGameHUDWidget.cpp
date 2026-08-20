#include "SFInGameHUDWidget.h"

#include "InGame/Controller/SFInGamePlayerController.h"

#include "SFSelectInfo/SFSelectable.h"
#include "SFSelectInfo/SFSelectionPanelWidget.h"
#include "SFSelectInfo/SFSelectionUIDataRow.h"

#include "HUD/Production/SFProductionBuildingPanelWidget.h"
#include "UI/ResultWidget.h"

#include "Game/Subsystem/SFDataTableManager.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "ActorComponent/HP/HealthComponent.h"

#include "CommonActivatableWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/DecalComponent.h"

#include "Engine/EngineTypes.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"

#include "Logging/SFLog.h"


void USFInGameHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    ASFInGamePlayerController* PlayerController =
        Cast<ASFInGamePlayerController>(
            GetOwningPlayer()
        );

    if (!IsValid(PlayerController))
    {
        SF_LOG(
            Error,
            "[SFInGameHUDWidget] "
            "SFInGamePlayerController is invalid."
        );
        return;
    }

    PlayerController->OnSelectInput.AddUniqueDynamic(
        this,
        &USFInGameHUDWidget::HandleSelectInput
    );
    PlayerController->OnSelectionDragStarted.RemoveDynamic(
        this,
        &USFInGameHUDWidget::HandleSelectionDragStarted
    );
    PlayerController->OnSelectionDragStarted.AddUniqueDynamic(
        this,
        &USFInGameHUDWidget::HandleSelectionDragStarted
    );

    PlayerController->OnSelectionDragCompleted.RemoveDynamic(
        this,
        &USFInGameHUDWidget::HandleSelectionDragCompleted
    );
    PlayerController->OnSelectionDragCompleted.AddUniqueDynamic(
        this,
        &USFInGameHUDWidget::HandleSelectionDragCompleted
    );

    if (IsValid(SelectionDragBox))
    {
        SelectionDragBox->SetVisibility(
            ESlateVisibility::Collapsed
        );
    }
    RefreshBuildModeOpenButtonVisibility();

    if (IsValid(SelectionPanel))
    {
        SelectionPanel->OnTierUpgradeRequested.RemoveDynamic(
                this,
                &USFInGameHUDWidget::HandleTierUpgradeRequested
            );

        SelectionPanel->OnTierUpgradeRequested.AddUniqueDynamic(
                this,
                &USFInGameHUDWidget::HandleTierUpgradeRequested
            );

        SF_LOG(
            Log,
            "[SFInGameHUDWidget] "
            "Tier upgrade delegate binding completed."
        );
    }
}

void USFInGameHUDWidget::NativeDestruct()
{
    HideAllSelectionIndicators();

    if (IsValid(ActiveBuildingModeWidget))
    {
        ActiveBuildingModeWidget
            ->OnDeactivated()
            .RemoveAll(this);
    }

    UnbindSelectedActorDestroyed();
    UnbindSelectedHealthDelegate();

    ASFInGamePlayerController* PlayerController =
        Cast<ASFInGamePlayerController>(
            GetOwningPlayer()
        );

    if (IsValid(PlayerController))
    {
        PlayerController->OnSelectInput.RemoveDynamic(
            this,
            &USFInGameHUDWidget::HandleSelectInput
        );

        PlayerController->OnSelectionDragStarted.RemoveDynamic(
            this,
            &USFInGameHUDWidget::HandleSelectionDragStarted
        );

        PlayerController->OnSelectionDragCompleted.RemoveDynamic(
            this,
            &USFInGameHUDWidget::HandleSelectionDragCompleted
        );
    }

    if (IsValid(SelectionPanel))
    {
        SelectionPanel->OnTierUpgradeRequested.RemoveDynamic(
                this,
                &USFInGameHUDWidget::HandleTierUpgradeRequested
            );
    }

    if (IsValid(ActiveSettingsWidget))
    {
        ActiveSettingsWidget->OnDeactivated().RemoveAll(this);
    }

    Super::NativeDestruct();
}

void USFInGameHUDWidget::HandleSelectInput()
{
    SF_LOG(
        Log,
        "[SFInGameHUDWidget] "
        "Select input received."
    );

    PerformSelectionTrace();
}

void USFInGameHUDWidget::HandleSelectedActorDestroyed(
    AActor* DestroyedActor
)
{
    HideSelectionIndicator(DestroyedActor);

    if (DestroyedActor != SelectedActor.Get())
    {
        return;
    }

    UnbindSelectedHealthDelegate();

    SelectedActor = nullptr;
    SelectedInfo = FSFSelectableInfo();

    if (ASFInGamePlayerController* PlayerController = Cast<ASFInGamePlayerController>(GetOwningPlayer()))
    {
        PlayerController->SetSelectedActor(nullptr);
    }

    RefreshBuildModeOpenButtonVisibility();

    if (IsValid(SelectionPanel))
    {
        SelectionPanel->ClearSelection();
    }

    if (IsValid(ProductionBuildingPanel))
    {
        ProductionBuildingPanel->HideProductionBuilding();
    }
}

void USFInGameHUDWidget::PerformSelectionTrace()
{
    APlayerController* PlayerController =
        GetOwningPlayer();

    if (!IsValid(PlayerController))
    {
        ClearSelection();
        return;
    }

    FHitResult HitResult;

    const ETraceTypeQuery SelectionTraceType =
        UEngineTypes::ConvertToTraceType(
            ECC_Visibility
        );

    const bool bHit =
        PlayerController->GetHitResultUnderCursorByChannel(
            SelectionTraceType,
            false,
            HitResult
        );

    if (!bHit)
    {
        ClearSelection();
        return;
    }

    AActor* HitActor =
        HitResult.GetActor();

    if (!IsValid(HitActor))
    {
        ClearSelection();
        return;
    }

    if (
        !HitActor->GetClass()->ImplementsInterface(
            USFSelectable::StaticClass()
        )
        )
    {
        ClearSelection();
        return;
    }

    SelectActor(HitActor);
}

void USFInGameHUDWidget::ApplySelection(
    AActor* NewSelectedActor,
    const FSFSelectableInfo& NewSelectedInfo
)
{
    if (!IsValid(NewSelectedActor))
    {
        ClearSelection();
        return;
    }

    const bool bIsBuilding =
        NewSelectedInfo.SelectableType ==
        ESFSelectableType::MainBuilding ||
        NewSelectedInfo.SelectableType ==
        ESFSelectableType::ProductionBuilding ||
        NewSelectedInfo.SelectableType ==
        ESFSelectableType::ResourceBuilding;

    if (bIsBuilding)
    {
        CloseBuildingMode();
    }

    if (SelectedActor != NewSelectedActor)
    {
        UnbindSelectedActorDestroyed();
        UnbindSelectedHealthDelegate();

        HideAllSelectionIndicators();

        SelectedActor = NewSelectedActor;

        SelectedActor->OnDestroyed.AddUniqueDynamic(
            this,
            &USFInGameHUDWidget::HandleSelectedActorDestroyed
        );
    }

	ShowSelectionIndicator(NewSelectedActor); 

    SelectedActors.Reset();
    SelectedActors.Add(NewSelectedActor);
    SelectedInfo = NewSelectedInfo;

    if (ASFInGamePlayerController* PlayerController = Cast<ASFInGamePlayerController>(GetOwningPlayer()))
    {
        PlayerController->SetSelectedActor(SelectedActor.Get());
    }

    BindSelectedHealthDelegate(SelectedActor.Get());

    RefreshBuildModeOpenButtonVisibility();
    RefreshSelectionPanels();
}

void USFInGameHUDWidget::ApplySelectionUIData(
    FSFSelectableInfo& InOutSelectableInfo
)
{
    if (InOutSelectableInfo.DataRowName.IsNone())
    {
        return;
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

    const FSFSelectionUIDataRow* UIData =
        DataTableManager->GetDataRow<FSFSelectionUIDataRow>(
            EDataTableType::UI,
            InOutSelectableInfo.DataRowName,
            TEXT("ApplySelectionUIData")
        );

    if (!UIData)
    {
        return;
    }

    InOutSelectableInfo.DisplayName =
        UIData->DisplayName;

    InOutSelectableInfo.PortraitImage =
        UIData->PortraitImage.LoadSynchronous();

    InOutSelectableInfo.SelectionIcon =
        UIData->SelectionIcon.LoadSynchronous();

    InOutSelectableInfo.Description =
        UIData->Description;

    InOutSelectableInfo.bShowHealth =
        UIData->bShowHealth;

    InOutSelectableInfo.bShowAttackPower =
        UIData->bShowAttackPower;

    InOutSelectableInfo.bShowMoveSpeed =
        UIData->bShowMoveSpeed;
}

void USFInGameHUDWidget::RefreshSelectionPanels()
{
    if (IsValid(SelectionPanel))
    {
        SelectionPanel->ShowSelection(
            SelectedInfo
        );
    }

    if (!IsValid(ProductionBuildingPanel))
    {
        return;
    }

    if (
        SelectedInfo.SelectableType ==
        ESFSelectableType::ProductionBuilding
        )
    {
        ProductionBuildingPanel->ShowProductionBuilding(
            SelectedActor.Get()
        );
        return;
    }

    ProductionBuildingPanel->HideProductionBuilding();
}

void USFInGameHUDWidget::ClearSelection()
{

    HideAllSelectionIndicators();

    UnbindSelectedActorDestroyed();
    UnbindSelectedHealthDelegate();

    SelectedActor = nullptr;
    SelectedActors.Reset();
    SelectedInfo = FSFSelectableInfo();

    if (ASFInGamePlayerController* PlayerController = Cast<ASFInGamePlayerController>(GetOwningPlayer()))
    {
        PlayerController->SetSelectedActor(nullptr);
    }

    RefreshBuildModeOpenButtonVisibility();

    if (IsValid(SelectionPanel))
    {
        SelectionPanel->ClearSelection();
    }

    if (IsValid(ProductionBuildingPanel))
    {
        ProductionBuildingPanel->HideProductionBuilding();
    }
}

void USFInGameHUDWidget::UnbindSelectedActorDestroyed()
{
    if (!IsValid(SelectedActor))
    {
        return;
    }

    SelectedActor->OnDestroyed.RemoveDynamic(
        this,
        &USFInGameHUDWidget::HandleSelectedActorDestroyed
    );
}

void USFInGameHUDWidget::OpenBuildingMode()
{
    if (
        !IsValid(BuildingUIStack) ||
        !BuildingModeWidgetClass
        )
    {
        return;
    }

    if (
        IsValid(ActiveBuildingModeWidget) &&
        ActiveBuildingModeWidget->IsActivated()
        )
    {
        return;
    }

    ActiveBuildingModeWidget =
        BuildingUIStack->AddWidget<UCommonActivatableWidget>(
            BuildingModeWidgetClass
        );

    if (!IsValid(ActiveBuildingModeWidget))
    {
        return;
    }

    ActiveBuildingModeWidget->OnDeactivated().RemoveAll(this);

    ActiveBuildingModeWidget->OnDeactivated().AddUObject(
        this,
        &USFInGameHUDWidget::HandleBuildingModeDeactivated
    );

    RefreshBuildModeOpenButtonVisibility();
}

void USFInGameHUDWidget::CloseBuildingMode()
{
    if (!IsValid(ActiveBuildingModeWidget))
    {
        return;
    }

    ActiveBuildingModeWidget->DeactivateWidget();
}

void USFInGameHUDWidget::HandleBuildingModeDeactivated()
{
    if (IsValid(ActiveBuildingModeWidget))
    {
        ActiveBuildingModeWidget->OnDeactivated().RemoveAll(this);
    }

    ActiveBuildingModeWidget = nullptr;

    RefreshBuildModeOpenButtonVisibility();

    ASFInGamePlayerController* PlayerController =
        Cast<ASFInGamePlayerController>(
            GetOwningPlayer()
        );

    if (!IsValid(PlayerController))
    {
        return;
    }

    PlayerController->ClearSpawnedBuildingRef();
    PlayerController->SetSFControlMode(
        ESFControlMode::Gameplay
    );
}

void USFInGameHUDWidget::RefreshBuildModeOpenButtonVisibility()
{
    if (!IsValid(BuildModeOpenButton))
    {
        return;
    }

    const bool bIsBuildingSelected =
        SelectedInfo.SelectableType ==
        ESFSelectableType::MainBuilding ||
        SelectedInfo.SelectableType ==
        ESFSelectableType::ProductionBuilding ||
        SelectedInfo.SelectableType ==
        ESFSelectableType::ResourceBuilding;

    const bool bIsBuildingModeActive =
        IsValid(ActiveBuildingModeWidget) &&
        ActiveBuildingModeWidget->IsActivated();

    BuildModeOpenButton->SetVisibility(
        bIsBuildingSelected ||
        bIsBuildingModeActive
        ? ESlateVisibility::Collapsed
        : ESlateVisibility::Visible
    );
}

void USFInGameHUDWidget::ShowResultWidget(
    bool bIsWin
)
{
    if (!IsValid(ResultWidget))
    {
        if (!ResultWidgetClass)
        {
            SF_LOG(
                Error,
                "[SFInGameHUDWidget] "
                "ResultWidgetClass is not assigned."
            );
            return;
        }

        ResultWidget =
            CreateWidget<UResultWidget>(
                GetOwningPlayer(),
                ResultWidgetClass
            );

        if (!IsValid(ResultWidget))
        {
            SF_LOG(
                Error,
                "[SFInGameHUDWidget] "
                "Failed to create ResultWidget."
            );
            return;
        }

        ResultWidget->AddToViewport(100);
    }

    ResultWidget->SetResultState(
        bIsWin
    );

    SetVisibility(
        ESlateVisibility::Collapsed
    );
}

void USFInGameHUDWidget::SelectActor(AActor* Actor)
{
    if (!IsValid(Actor))
    {
        return;
    }

    if (!Actor->GetClass()->ImplementsInterface(USFSelectable::StaticClass()))
    {
        return;
    }

    FSFSelectableInfo SelectableInfo = ISFSelectable::Execute_GetSelectableInfo(Actor);

    const bool bIsBuilding =
        SelectableInfo.SelectableType == ESFSelectableType::MainBuilding ||
        SelectableInfo.SelectableType == ESFSelectableType::ProductionBuilding ||
        SelectableInfo.SelectableType == ESFSelectableType::ResourceBuilding;

    if (bIsBuilding)
    {
        const FHealthData HealthData = ISFSelectable::Execute_GetHealthData(Actor);
        SelectableInfo.Tier = HealthData.CurrentTier + 1;
    }

    ApplySelectionUIData(SelectableInfo);
    ApplySelection(Actor, SelectableInfo);
}

bool USFInGameHUDWidget::IsPointerOverSelectionAction() const
{
    return
        IsValid(SelectionPanel) &&
        SelectionPanel->IsTierUpButtonHovered();
}

void USFInGameHUDWidget::HandleTierUpgradeRequested()
{
    if (!IsValid(SelectedActor))
    {
        SF_LOG(
            Warning,
            "[SFInGameHUDWidget] "
            "Tier upgrade failed: "
            "SelectedActor is invalid."
        );
        return;
    }

    const bool bCanTierUpgrade =
        SelectedInfo.SelectableType ==
        ESFSelectableType::MainBuilding ||
        SelectedInfo.SelectableType ==
        ESFSelectableType::ProductionBuilding;

    if (!bCanTierUpgrade)
    {
        SF_LOG(
            Warning,
            "[SFInGameHUDWidget] "
            "Tier upgrade failed: "
            "Invalid selectable type."
        );
        return;
    }

    ASFBaseBuilding* SelectedBuilding =
        Cast<ASFBaseBuilding>(
            SelectedActor.Get()
        );

    if (!IsValid(SelectedBuilding))
    {
        SF_LOG(
            Error,
            "[SFInGameHUDWidget] "
            "Tier upgrade failed: "
            "SelectedActor is not SFBaseBuilding."
        );
        return;
    }

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] "
        "Tier upgrade requested | Building: %s",
        *SelectedBuilding->GetName()
    );

    // 건물 담당자가 이미 구현한 기존 함수 사용
    SelectedBuilding->UpgradeBuilding(1);
}

void USFInGameHUDWidget::BindSelectedHealthDelegate(AActor* Actor)
{
    UnbindSelectedHealthDelegate();

    if (!IsValid(Actor) || !Actor->GetClass()->ImplementsInterface(USFSelectable::StaticClass()))
    {
        return;
    }

    const FHealthData HealthData = ISFSelectable::Execute_GetHealthData(Actor);
    UHealthComponent* HealthComponent = HealthData.HealthComponent.Get();

    if (!IsValid(HealthComponent))
    {
        SF_LOG(Warning, "[SFInGameHUDWidget] Selected actor has no HealthComponent.");
        return;
    }

    SelectedHealthComponent = HealthComponent;

    HealthComponent->OnHealthValueChanged.RemoveDynamic(
        this,
        &USFInGameHUDWidget::HandleSelectedHealthChanged
    );

    HealthComponent->OnHealthValueChanged.AddUniqueDynamic(
        this,
        &USFInGameHUDWidget::HandleSelectedHealthChanged
    );

    SF_LOG(Log, "[SFInGameHUDWidget] Health delegate binding completed.");
}

void USFInGameHUDWidget::UnbindSelectedHealthDelegate()
{
    UHealthComponent* HealthComponent = SelectedHealthComponent.Get();

    if (IsValid(HealthComponent))
    {
        HealthComponent->OnHealthValueChanged.RemoveDynamic(
            this,
            &USFInGameHUDWidget::HandleSelectedHealthChanged
        );
    }

    SelectedHealthComponent.Reset();
}

void USFInGameHUDWidget::HandleSelectedHealthChanged(float BeforeHP, float CurrentHP, float MaxHP)
{
    if (!SelectedHealthComponent.IsValid())
    {
        return;
    }

    SelectedInfo.CurrentHealth = CurrentHP;
    SelectedInfo.MaximumHealth = MaxHP;

    if (IsValid(SelectionPanel))
    {
        SelectionPanel->ShowSelection(SelectedInfo);
    }

    SF_LOG(Log, "[SFInGameHUDWidget] Health changed | %.1f -> %.1f / %.1f", BeforeHP, CurrentHP, MaxHP);
}

void USFInGameHUDWidget::ToggleSettings()
{
    if (IsValid(ActiveSettingsWidget) && ActiveSettingsWidget->IsActivated())
    {
        ActiveSettingsWidget->DeactivateWidget();
        return;
    }

    if (!IsValid(SettingsUIStack) || !SettingsWidgetClass)
    {
        SF_LOG(Error, "[SFInGameHUDWidget] SettingsUIStack or SettingsWidgetClass is invalid.");
        return;
    }

    ActiveSettingsWidget = SettingsUIStack->AddWidget<UCommonActivatableWidget>(SettingsWidgetClass);

    if (!IsValid(ActiveSettingsWidget))
    {
        return;
    }

    ActiveSettingsWidget->OnDeactivated().RemoveAll(this);
    ActiveSettingsWidget->OnDeactivated().AddUObject(this, &USFInGameHUDWidget::HandleSettingsDeactivated);

    APlayerController* PlayerController = GetOwningPlayer();

    if (IsValid(PlayerController))
    {
        FInputModeGameAndUI InputMode;
        InputMode.SetWidgetToFocus(ActiveSettingsWidget->TakeWidget());
        InputMode.SetHideCursorDuringCapture(false);
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);

        PlayerController->SetInputMode(InputMode);
        PlayerController->bShowMouseCursor = true;
    }

    SF_LOG(Log, "[SFInGameHUDWidget] Settings opened.");
}

void USFInGameHUDWidget::HandleSettingsDeactivated()
{
    if (IsValid(ActiveSettingsWidget))
    {
        ActiveSettingsWidget->OnDeactivated().RemoveAll(this);
    }

    ActiveSettingsWidget = nullptr;

    APlayerController* PlayerController = GetOwningPlayer();

    if (IsValid(PlayerController))
    {
        FInputModeGameAndUI InputMode;
        InputMode.SetHideCursorDuringCapture(false);
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);

        PlayerController->SetInputMode(InputMode);
        PlayerController->bShowMouseCursor = true;
    }

    SF_LOG(Log, "[SFInGameHUDWidget] Settings closed.");
}

void USFInGameHUDWidget::ShowSelectionIndicator(AActor* Actor)
{
    if (!IsValid(Actor))
    {
        return;
    }

    if (!IsValid(SelectionIndicatorMaterial))
    {
        SF_LOG(Warning, "[SFInGameHUDWidget] SelectionIndicatorMaterial is invalid.");
        return;
    }

    if (SelectionIndicatorComponents.Contains(Actor))
    {
        return;
    }

    USceneComponent* RootComponent = Actor->GetRootComponent();
    if (!IsValid(RootComponent))
    {
        return;
    }

    const bool bIsBuilding = Actor->IsA<ASFBaseBuilding>();

    FVector Origin;
    FVector Extent;
    Actor->GetActorBounds(!bIsBuilding, Origin, Extent);

    const float UnitBaseRadius = FMath::Max(Extent.X, Extent.Y);
    const float BuildingBaseRadius = FVector2D(Extent.X, Extent.Y).Size();

    const float RingRadius = bIsBuilding
        ? FMath::Clamp(BuildingBaseRadius * 1.05f, 100.f, 1500.f)
        : FMath::Clamp(UnitBaseRadius * 0.7f, 32.f, 150.f);

    const FVector IndicatorLocation(
        Origin.X,
        Origin.Y,
        Origin.Z - Extent.Z + 8.f
    );

    UDecalComponent* Indicator = NewObject<UDecalComponent>(Actor);
    if (!IsValid(Indicator))
    {
        return;
    }

    Actor->AddInstanceComponent(Indicator);
    Indicator->SetupAttachment(RootComponent);
    Indicator->SetAbsolute(false, true, true);
    Indicator->SetMobility(EComponentMobility::Movable);
    Indicator->SetDecalMaterial(SelectionIndicatorMaterial);
    Indicator->DecalSize = FVector(32.f, RingRadius, RingRadius);
    Indicator->SetSortOrder(10);
    Indicator->RegisterComponent();
    Indicator->SetWorldLocation(IndicatorLocation);
    Indicator->SetWorldRotation(FRotator(-90.f, 0.f, 0.f));

    SelectionIndicatorComponents.Add(Actor, Indicator);

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] Selection indicator created | Actor: %s | Radius: %.1f",
        *Actor->GetName(),
        RingRadius
    );
}

void USFInGameHUDWidget::HideSelectionIndicator(AActor* Actor)
{
    if (Actor == nullptr)
    {
        return;
    }

    TObjectPtr<UDecalComponent>* IndicatorPtr =
        SelectionIndicatorComponents.Find(Actor);

    if (IndicatorPtr == nullptr)
    {
        return;
    }

    UDecalComponent* Indicator = IndicatorPtr->Get();

    if (IsValid(Indicator))
    {
        Indicator->DestroyComponent();
    }

    SelectionIndicatorComponents.Remove(Actor);
}

void USFInGameHUDWidget::HideAllSelectionIndicators()
{
    for (TPair<TObjectPtr<AActor>, TObjectPtr<UDecalComponent>>& Pair : SelectionIndicatorComponents)
    {
        if (IsValid(Pair.Value))
        {
            Pair.Value->DestroyComponent();
        }
    }

    SelectionIndicatorComponents.Empty();
}

void USFInGameHUDWidget::HandleSelectionDragStarted(
    FVector2D StartScreenPosition
)
{
    SelectionDragStartPosition = StartScreenPosition;
    bIsSelectionDragging = true;

    if (IsValid(SelectionDragBox))
    {
        SelectionDragBox->SetVisibility(
            ESlateVisibility::HitTestInvisible
        );
        UpdateSelectionDragBox(StartScreenPosition);
    }
}

void USFInGameHUDWidget::HandleSelectionDragCompleted(
    FVector2D StartScreenPosition,
    FVector2D EndScreenPosition
)
{
    bIsSelectionDragging = false;

    if (IsValid(SelectionDragBox))
    {
        UpdateSelectionDragBox(EndScreenPosition);
        SelectionDragBox->SetVisibility(
            ESlateVisibility::Collapsed
        );
    }

    const float DragDistance = FVector2D::Distance(
        StartScreenPosition,
        EndScreenPosition
    );

    if (DragDistance <= 8.f)
    {
        return;
    }

    PerformDragSelection(
        StartScreenPosition,
        EndScreenPosition
    );
}

void USFInGameHUDWidget::NativeTick(
    const FGeometry& MyGeometry,
    float InDeltaTime
)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!bIsSelectionDragging || !IsValid(SelectionDragBox))
    {
        return;
    }

    APlayerController* PlayerController = GetOwningPlayer();
    if (!IsValid(PlayerController))
    {
        return;
    }

    FVector2D CurrentScreenPosition;
    if (PlayerController->GetMousePosition(
        CurrentScreenPosition.X,
        CurrentScreenPosition.Y
    ))
    {
        UpdateSelectionDragBox(CurrentScreenPosition);
    }
}

void USFInGameHUDWidget::UpdateSelectionDragBox(
    FVector2D CurrentScreenPosition
)
{
    if (!IsValid(SelectionDragBox))
    {
        return;
    }

    UCanvasPanelSlot* CanvasSlot =
        Cast<UCanvasPanelSlot>(SelectionDragBox->Slot);

    if (!IsValid(CanvasSlot))
    {
        return;
    }

    const float ViewportScale =
        UWidgetLayoutLibrary::GetViewportScale(this);

    const float SafeViewportScale =
        FMath::Max(ViewportScale, KINDA_SMALL_NUMBER);

    const FVector2D LocalStart =
        SelectionDragStartPosition / SafeViewportScale;

    const FVector2D LocalCurrent =
        CurrentScreenPosition / SafeViewportScale;

    const FVector2D TopLeft(
        FMath::Min(LocalStart.X, LocalCurrent.X),
        FMath::Min(LocalStart.Y, LocalCurrent.Y)
    );

    const FVector2D BoxSize(
        FMath::Abs(LocalCurrent.X - LocalStart.X),
        FMath::Abs(LocalCurrent.Y - LocalStart.Y)
    );

    CanvasSlot->SetPosition(TopLeft);
    CanvasSlot->SetSize(BoxSize);
}

void USFInGameHUDWidget::PerformDragSelection(
    FVector2D StartScreenPosition,
    FVector2D EndScreenPosition
)
{
    APlayerController* PlayerController =
        GetOwningPlayer();

    UWorld* World = GetWorld();

    if (!IsValid(PlayerController) || !IsValid(World))
    {
        return;
    }

    const FVector2D RectangleMin(
        FMath::Min(
            StartScreenPosition.X,
            EndScreenPosition.X
        ),
        FMath::Min(
            StartScreenPosition.Y,
            EndScreenPosition.Y
        )
    );

    const FVector2D RectangleMax(
        FMath::Max(
            StartScreenPosition.X,
            EndScreenPosition.X
        ),
        FMath::Max(
            StartScreenPosition.Y,
            EndScreenPosition.Y
        )
    );

    const FBox2D SelectionRectangle(
        RectangleMin,
        RectangleMax
    );

    TArray<AActor*> FoundActors;

    for (TActorIterator<AActor> Iterator(World); Iterator; ++Iterator)
    {
        AActor* Actor = *Iterator;

        if (!IsValid(Actor))
        {
            continue;
        }

        if (!Actor->GetClass()->ImplementsInterface(
            USFSelectable::StaticClass()
        ))
        {
            continue;
        }

        FVector BoundsOrigin;
        FVector BoundsExtent;

        Actor->GetActorBounds(
            false,
            BoundsOrigin,
            BoundsExtent
        );

        FVector2D ActorScreenPosition;

        const bool bProjected =
            PlayerController->ProjectWorldLocationToScreen(
                BoundsOrigin,
                ActorScreenPosition,
                true
            );

        if (!bProjected)
        {
            continue;
        }

        if (!SelectionRectangle.IsInsideOrOn(
            ActorScreenPosition
        ))
        {
            continue;
        }

        FoundActors.Add(Actor);
    }

    FoundActors.Sort(
        [this](const AActor& Left, const AActor& Right)
        {
            const int32 LeftPriority =
                GetSelectionPriority(
                    const_cast<AActor*>(&Left)
                );

            const int32 RightPriority =
                GetSelectionPriority(
                    const_cast<AActor*>(&Right)
                );

            if (LeftPriority != RightPriority)
            {
                return LeftPriority < RightPriority;
            }

            return Left.GetName() < Right.GetName();
        }
    );

    if (FoundActors.Num() > 30)
    {
        FoundActors.SetNum(30);
    }

    ApplyMultipleSelection(FoundActors);
}

int32 USFInGameHUDWidget::GetSelectionPriority(
    AActor* Actor
) const
{
    if (!IsValid(Actor))
    {
        return 100;
    }

    if (!Actor->GetClass()->ImplementsInterface(
        USFSelectable::StaticClass()
    ))
    {
        return 100;
    }

    const FSFSelectableInfo SelectableInfo =
        ISFSelectable::Execute_GetSelectableInfo(Actor);

    switch (SelectableInfo.SelectableType)
    {
    case ESFSelectableType::Commander:
        return 0;

    case ESFSelectableType::MainBuilding:
    case ESFSelectableType::ProductionBuilding:
    case ESFSelectableType::ResourceBuilding:
        return 1;

    case ESFSelectableType::Unit:
        // 우선순위 2는 스킬 보유 유닛에 사용할 예정
        return 3;

    default:
        return 100;
    }
}

void USFInGameHUDWidget::ApplyMultipleSelection(
    const TArray<AActor*>& NewSelectedActors
)
{
    if (NewSelectedActors.IsEmpty())
    {
        ClearSelection();
        return;
    }

    AActor* PrimaryActor =
        NewSelectedActors[0];

    if (!IsValid(PrimaryActor))
    {
        ClearSelection();
        return;
    }

    // 0번 액터를 기존 왼쪽 정보 패널에 표시한다.
    SelectActor(PrimaryActor);

    SelectedActors.Reset();
    HideAllSelectionIndicators();

    for (AActor* Actor : NewSelectedActors)
    {
        if (!IsValid(Actor))
        {
            continue;
        }

        SelectedActors.Add(Actor);
        ShowSelectionIndicator(Actor);
    }

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] Multiple selection applied | Count: %d | Primary: %s",
        SelectedActors.Num(),
        *PrimaryActor->GetName()
    );
}