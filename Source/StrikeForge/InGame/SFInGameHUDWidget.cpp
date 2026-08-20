#include "SFInGameHUDWidget.h"

#include "InGame/Controller/SFInGamePlayerController.h"

#include "SFSelectInfo/SFSelectable.h"
#include "SFSelectInfo/SFSelectionPanelWidget.h"
#include "SFSelectInfo/SFSelectionUIDataRow.h"

#include "HUD/Production/SFProductionBuildingPanelWidget.h"

#include "UI/ResultWidget.h"

#include "Game/Subsystem/SFDataTableManager.h"
#include "GameFramework/PlayerController.h"

#include "CommonActivatableWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#include "Components/Button.h"

#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"

#include "Buildings/Building/SFBaseBuilding.h"
#include "ActorComponent/HP/HealthComponent.h"

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

        SelectedActor = NewSelectedActor;

        SelectedActor->OnDestroyed.AddUniqueDynamic(
            this,
            &USFInGameHUDWidget::HandleSelectedActorDestroyed
        );
    }

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
    UnbindSelectedActorDestroyed();
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