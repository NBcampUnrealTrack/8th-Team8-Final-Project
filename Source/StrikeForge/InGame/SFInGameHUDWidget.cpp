#include "SFInGameHUDWidget.h"

#include "InGame/Controller/SFInGamePlayerController.h"
#include "Interfaces/Units/Targetable.h"
#include "InGame/HUD/Tutorial/SFStageIntroWidget.h"

#include "SFSelectInfo/SFSelectable.h"
#include "SFSelectInfo/SFSelectionPanelWidget.h"
#include "SFSelectInfo/SFSelectionUIDataRow.h"
#include "SFSelectInfo/SFMultiSelectionPanelWidget.h"

#include "HUD/Production/SFProductionBuildingPanelWidget.h"
#include "UI/ResultWidget.h"

#include "HUD/Portrait/SFUnitPortraitActor.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

#include "Game/Subsystem/SFDataTableManager.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "ActorComponent/HP/HealthComponent.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"

#include "CommonActivatableWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/DecalComponent.h"

#include "Engine/EngineTypes.h"
#include "EngineUtils.h"
#include "Engine/DataTable.h"

#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "Game/GameState/SFGameState.h"

#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "Kismet/GameplayStatics.h"

#include "TimerManager.h"

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

    // 추가분
    UnitPortraitActor =
        Cast<ASFUnitPortraitActor>(
            UGameplayStatics::GetActorOfClass(
                GetWorld(),
                ASFUnitPortraitActor::StaticClass()
            )
        );

    if (!IsValid(UnitPortraitActor))
    {
        SF_LOG(
            Warning,
            "[SFInGameHUDWidget] "
            "UnitPortraitActor was not found."
        );
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

    if (IsValid(MultiSelectionPanel))
    {
        MultiSelectionPanel->OnPrimarySelectionRequested.RemoveDynamic(
            this,
            &USFInGameHUDWidget::HandlePrimarySelectionRequested
        );

        MultiSelectionPanel->OnPrimarySelectionRequested.AddUniqueDynamic(
            this,
            &USFInGameHUDWidget::HandlePrimarySelectionRequested
        );

        MultiSelectionPanel->ClearSelections();

        SF_LOG(Log, "[SFInGameHUDWidget] Multi selection delegate binding completed.");
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimerForNextTick(
            FTimerDelegate::CreateUObject(
                this,
                &USFInGameHUDWidget::TryStartStageIntro
            )
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

    UnbindSelectedActorsDestroyed();

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

    if (IsValid(MultiSelectionPanel))
    {
        MultiSelectionPanel->OnPrimarySelectionRequested.RemoveDynamic(
            this,
            &USFInGameHUDWidget::HandlePrimarySelectionRequested
        );
    }

    if (IsValid(ActiveStageIntroWidget))
    {
        ActiveStageIntroWidget->OnDeactivated().RemoveAll(this);
        ActiveStageIntroWidget->RemoveFromParent();
        ActiveStageIntroWidget = nullptr;
    }

    Super::NativeDestruct();
}

void USFInGameHUDWidget::HandleSelectInput()
{
    SF_LOG(Log, "[SFInGameHUDWidget] Select input received.");

    CloseBuildingMode();
    PerformSelectionTrace();
}

void USFInGameHUDWidget::HandleSelectedActorDestroyed(AActor* DestroyedActor)
{
    if (DestroyedActor == nullptr)
    {
        return;
    }

    const bool bWasPrimary =
        DestroyedActor == SelectedActor.Get();

    HideSelectionIndicator(DestroyedActor);

    SelectedActors.RemoveAll(
        [DestroyedActor](const TObjectPtr<AActor>& ActorPtr)
        {
            return
                ActorPtr.Get() == DestroyedActor ||
                !IsValid(ActorPtr.Get());
        }
    );

    if (SelectedActors.IsEmpty())
    {
        ClearSelection();
        return;
    }

    if (bWasPrimary)
    {
        UnbindSelectedHealthDelegate();

        SelectedActor = nullptr;
        SelectedInfo = FSFSelectableInfo();

        AActor* NewPrimaryActor =
            SelectedActors[0].Get();

        if (IsValid(NewPrimaryActor))
        {
            HandlePrimarySelectionRequested(
                NewPrimaryActor
            );
        }

        return;
    }

    RefreshMultiSelectionPanel();

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] Destroyed actor removed from selection | Remaining: %d",
        SelectedActors.Num()
    );
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
        NewSelectedInfo.SelectableType == ESFSelectableType::MainBuilding ||
        NewSelectedInfo.SelectableType == ESFSelectableType::NormalBuilding ||
        NewSelectedInfo.SelectableType == ESFSelectableType::ProductionBuilding ||
        NewSelectedInfo.SelectableType == ESFSelectableType::ResourceBuilding;

    if (bIsBuilding)
    {
        CloseBuildingMode();
    }

    UnbindSelectedActorsDestroyed();
    UnbindSelectedHealthDelegate();
    HideAllSelectionIndicators();

    SelectedActor = NewSelectedActor;

    SelectedActor->OnDestroyed.AddUniqueDynamic(
        this,
        &USFInGameHUDWidget::HandleSelectedActorDestroyed
    );

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
    RefreshMultiSelectionPanel();
    RefreshSelectedPortrait();
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

    InOutSelectableInfo.bShowSkillIcons =
        UIData->bShowSkillIcons;
}

void USFInGameHUDWidget::RefreshSelectionPanels()
{
    const bool bHasValidSelection =
        IsValid(SelectedActor) &&
        SelectedInfo.SelectableType != ESFSelectableType::None;

    if (IsValid(SelectionPanel))
    {
        if (bHasValidSelection)
        {
            SelectionPanel->ShowSelection(SelectedInfo);
        }
        else
        {
            SelectionPanel->ClearSelection();
        }
    }

    if (!IsValid(ProductionBuildingPanel))
    {
        return;
    }

    const bool bIsBuildingModeActive =
        IsValid(ActiveBuildingModeWidget) &&
        ActiveBuildingModeWidget->IsActivated();

    if (bIsBuildingModeActive)
    {
        ProductionBuildingPanel->HideProductionBuilding();
        return;
    }

    if (SelectedActors.Num() > 1)
    {
        ProductionBuildingPanel->HideProductionBuilding();
        return;
    }

    if (
        bHasValidSelection &&
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

void USFInGameHUDWidget::RefreshSelectedPortrait()
{
    if (!IsValid(UnitPortraitActor))
    {
        return;
    }

    const bool bUses3DPortrait =
        SelectedInfo.SelectableType == ESFSelectableType::Commander ||
        SelectedInfo.SelectableType == ESFSelectableType::Unit;

    if (!bUses3DPortrait)
    {
        UnitPortraitActor->ClearPortrait();
        return;
    }

    ACharacter* SelectedCharacter =
        Cast<ACharacter>(SelectedActor.Get());

    if (!IsValid(SelectedCharacter))
    {
        UnitPortraitActor->ClearPortrait();
        return;
    }

    USkeletalMeshComponent* SelectedMeshComponent =
        SelectedCharacter->GetMesh();

    if (!IsValid(SelectedMeshComponent))
    {
        UnitPortraitActor->ClearPortrait();
        return;
    }

    USkeletalMesh* SelectedSkeletalMesh =
        SelectedMeshComponent->GetSkeletalMeshAsset();

    if (!IsValid(SelectedSkeletalMesh))
    {
        UnitPortraitActor->ClearPortrait();
        return;
    }

    UnitPortraitActor->ApplyPortrait(
        SelectedSkeletalMesh,
        nullptr
    );
}

void USFInGameHUDWidget::ClearSelection()
{

    HideAllSelectionIndicators();

    if (IsValid(UnitPortraitActor))
    {
        UnitPortraitActor->ClearPortrait();
    }

    UnbindSelectedActorsDestroyed();
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

    if (IsValid(MultiSelectionPanel))
    {
        MultiSelectionPanel->ClearSelections();
    }
}

void USFInGameHUDWidget::BindSelectedActorsDestroyed()
{
    for (const TObjectPtr<AActor>& ActorPtr : SelectedActors)
    {
        AActor* Actor = ActorPtr.Get();

        if (!IsValid(Actor))
        {
            continue;
        }

        Actor->OnDestroyed.AddUniqueDynamic(
            this,
            &USFInGameHUDWidget::HandleSelectedActorDestroyed
        );
    }
}

void USFInGameHUDWidget::UnbindSelectedActorsDestroyed()
{
    for (const TObjectPtr<AActor>& ActorPtr : SelectedActors)
    {
        AActor* Actor = ActorPtr.Get();

        if (!IsValid(Actor))
        {
            continue;
        }

        Actor->OnDestroyed.RemoveDynamic(
            this,
            &USFInGameHUDWidget::HandleSelectedActorDestroyed
        );
    }

    if (IsValid(SelectedActor))
    {
        SelectedActor->OnDestroyed.RemoveDynamic(
            this,
            &USFInGameHUDWidget::HandleSelectedActorDestroyed
        );
    }
}

void USFInGameHUDWidget::ToggleBuildingMode()
{
    const bool bIsActive =
        IsValid(ActiveBuildingModeWidget) &&
        ActiveBuildingModeWidget->IsActivated();

    if (bIsActive)
    {
        CloseBuildingMode();
        return;
    }

    OpenBuildingMode();
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
    RefreshSelectionPanels();
    RefreshMultiSelectionPanel();
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
    RefreshSelectionPanels();
    RefreshMultiSelectionPanel();

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
    ActiveSettingsWidget->OnDeactivated().AddUObject(
        this,
        &USFInGameHUDWidget::HandleSettingsDeactivated
    );

    UGameplayStatics::SetGamePaused(this, true);

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

    UGameplayStatics::SetGamePaused(this, false);

    APlayerController* PlayerController = GetOwningPlayer();
    if (IsValid(PlayerController))
    {
        FInputModeGameAndUI InputMode;
        InputMode.SetHideCursorDuringCapture(false);
        InputMode.SetLockMouseToViewportBehavior(
            EMouseLockMode::LockAlways
        );

        PlayerController->SetInputMode(InputMode);
        PlayerController->bShowMouseCursor = true;
    }

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] "
        "Settings closed. Game resumed."
    );
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

    CloseBuildingMode();

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

        if (!IsOwnedByLocalPlayer(Actor))
        {
            continue;
        }

        FVector BoundsOrigin;
        FVector BoundsExtent;
        Actor->GetActorBounds(false, BoundsOrigin, BoundsExtent);

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

        if (!SelectionRectangle.IsInsideOrOn(ActorScreenPosition))
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
    case ESFSelectableType::NormalBuilding:
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

    CloseBuildingMode();

    AActor* PrimaryActor = NewSelectedActors[0];

    if (!IsValid(PrimaryActor))
    {
        ClearSelection();
        return;
    }

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

    BindSelectedActorsDestroyed();
    RefreshMultiSelectionPanel();

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] Multiple selection applied | Count: %d | Primary: %s",
        SelectedActors.Num(),
        *PrimaryActor->GetName()
    );
}

void USFInGameHUDWidget::RefreshMultiSelectionPanel()
{
    if (!IsValid(MultiSelectionPanel))
    {
        return;
    }

    const bool bIsBuildingModeActive =
        IsValid(ActiveBuildingModeWidget) &&
        ActiveBuildingModeWidget->IsActivated();

    if (bIsBuildingModeActive)
    {
        MultiSelectionPanel->ClearSelections();
        return;
    }

    if (SelectedActors.Num() <= 1)
    {
        MultiSelectionPanel->ClearSelections();
        return;
    }

    TArray<TObjectPtr<AActor>> ValidActors;
    TArray<FSFSelectableInfo> SelectableInfos;

    ValidActors.Reserve(SelectedActors.Num());
    SelectableInfos.Reserve(SelectedActors.Num());

    for (const TObjectPtr<AActor>& ActorPtr : SelectedActors)
    {
        AActor* Actor = ActorPtr.Get();

        if (!IsValid(Actor))
        {
            continue;
        }

        if (!Actor->GetClass()->ImplementsInterface(USFSelectable::StaticClass()))
        {
            continue;
        }

        FSFSelectableInfo SelectableInfo =
            ISFSelectable::Execute_GetSelectableInfo(Actor);

        ApplySelectionUIData(SelectableInfo);

        ValidActors.Add(Actor);
        SelectableInfos.Add(SelectableInfo);
    }

    SelectedActors = MoveTemp(ValidActors);

    if (SelectedActors.Num() <= 1)
    {
        MultiSelectionPanel->ClearSelections();
        return;
    }

    if (IsValid(ProductionBuildingPanel))
    {
        ProductionBuildingPanel->HideProductionBuilding();
    }

    MultiSelectionPanel->ShowSelections(
        SelectedActors,
        SelectableInfos
    );
}

void USFInGameHUDWidget::HandlePrimarySelectionRequested(AActor* Actor)
{
    if (!IsValid(Actor))
    {
        return;
    }

    const int32 RequestedIndex = SelectedActors.IndexOfByPredicate(
        [Actor](const TObjectPtr<AActor>& ActorPtr)
        {
            return ActorPtr.Get() == Actor;
        }
    );

    if (RequestedIndex == INDEX_NONE)
    {
        SF_LOG(Warning, "[SFInGameHUDWidget] Requested actor is not in SelectedActors.");
        return;
    }

    if (RequestedIndex > 0)
    {
        TObjectPtr<AActor> PromotedActor = SelectedActors[RequestedIndex];
        SelectedActors.RemoveAt(RequestedIndex);
        SelectedActors.Insert(PromotedActor, 0);
    }

    UnbindSelectedHealthDelegate();

    SelectedActor = Actor;

    FSFSelectableInfo NewSelectedInfo =
        ISFSelectable::Execute_GetSelectableInfo(Actor);

    const bool bIsBuilding =
        NewSelectedInfo.SelectableType == ESFSelectableType::MainBuilding ||
        NewSelectedInfo.SelectableType == ESFSelectableType::NormalBuilding ||
        NewSelectedInfo.SelectableType == ESFSelectableType::ProductionBuilding ||
        NewSelectedInfo.SelectableType == ESFSelectableType::ResourceBuilding;

    if (bIsBuilding)
    {
        const FHealthData HealthData =
            ISFSelectable::Execute_GetHealthData(Actor);

        NewSelectedInfo.Tier = HealthData.CurrentTier + 1;
    }

    ApplySelectionUIData(NewSelectedInfo);
    SelectedInfo = NewSelectedInfo;

    SelectedActor->OnDestroyed.AddUniqueDynamic(
        this,
        &USFInGameHUDWidget::HandleSelectedActorDestroyed
    );

    if (ASFInGamePlayerController* PlayerController =
        Cast<ASFInGamePlayerController>(GetOwningPlayer()))
    {
        PlayerController->SetSelectedActor(SelectedActor.Get());
    }

    BindSelectedHealthDelegate(SelectedActor.Get());
    RefreshBuildModeOpenButtonVisibility();

    if (IsValid(SelectionPanel))
    {
        SelectionPanel->ShowSelection(SelectedInfo);
    }

    if (IsValid(ProductionBuildingPanel))
    {
        ProductionBuildingPanel->HideProductionBuilding();
    }

    RefreshSelectedPortrait();
    RefreshMultiSelectionPanel();

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] Primary selection changed | Actor: %s",
        *Actor->GetName()
    );
}

bool USFInGameHUDWidget::IsOwnedByLocalPlayer(AActor* Actor) const
{
    if (!IsValid(Actor))
    {
        return false;
    }

    const UCombatTargetComponent* CombatTarget =
        Actor->FindComponentByClass<UCombatTargetComponent>();

    if (!IsValid(CombatTarget))
    {
        return false;
    }

    return CombatTarget->GetTeamIndex() == Targetable::PlayerTeam;
}

void USFInGameHUDWidget::ShowMoveDestinationIndicator(
    const FVector& WorldLocation
)
{
    if (!IsValid(MoveDestinationIndicatorMaterial))
    {
        SF_LOG(
            Warning,
            "[SFInGameHUDWidget] "
            "MoveDestinationIndicatorMaterial is invalid."
        );
        return;
    }

    UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        return;
    }

    UMaterialInstanceDynamic* DynamicMaterial =
        UMaterialInstanceDynamic::Create(
            MoveDestinationIndicatorMaterial,
            this
        );

    if (!IsValid(DynamicMaterial))
    {
        return;
    }

    DynamicMaterial->SetScalarParameterValue(
        TEXT("StartTime"),
        World->GetTimeSeconds()
    );

    DynamicMaterial->SetScalarParameterValue(
        TEXT("AnimDuration"),
        MoveDestinationIndicatorLifeTime
    );

    UDecalComponent* Indicator =
        UGameplayStatics::SpawnDecalAtLocation(
            this,
            DynamicMaterial,
            MoveDestinationIndicatorSize,
            WorldLocation + FVector(0.f, 0.f, 8.f),
            FRotator(-90.f, 0.f, 0.f),
            MoveDestinationIndicatorLifeTime
        );

    if (!IsValid(Indicator))
    {
        return;
    }

    Indicator->SetSortOrder(20);

    const float FadeStartDelay = FMath::Max(
        0.f,
        MoveDestinationIndicatorLifeTime -
        MoveDestinationIndicatorFadeDuration
    );

    Indicator->SetFadeOut(
        FadeStartDelay,
        MoveDestinationIndicatorFadeDuration,
        false
    );

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] "
        "Move destination indicator shown | Location: %s",
        *WorldLocation.ToString()
    );
}

FName USFInGameHUDWidget::ResolveStageIntroRowName() const
{
    if (UWorld* World = GetWorld())
    {
        if (ASFGameState* GameState = World->GetGameState<ASFGameState>())
        {
            const FName StageRowName = GameState->GetCurrentStageRowName();

            if (!StageRowName.IsNone())
            {
                return StageRowName;
            }
        }
    }

    if (const USFGameInstance* GameInstance = Cast<USFGameInstance>(GetGameInstance()))
    {
        const FName StageRowName = GameInstance->GetSelectedStageRowName();

        if (!StageRowName.IsNone())
        {
            return StageRowName;
        }
    }

    return StageIntroFallbackRowName;
}

void USFInGameHUDWidget::TryStartStageIntro()
{
    SF_LOG(Log, "[SFInGameHUDWidget] TryStartStageIntro called.");

    if (!StageIntroWidgetClass)
    {
        SF_LOG(Error, "[SFInGameHUDWidget] StageIntroWidgetClass is invalid.");
        return;
    }

    if (!IsValid(StageIntroDataTable))
    {
        SF_LOG(Error, "[SFInGameHUDWidget] StageIntroDataTable is invalid.");
        return;
    }

    FName IntroRowName = StageIntroFallbackRowName;

    if (IntroRowName.IsNone())
    {
        SF_LOG(Error, "[SFInGameHUDWidget] StageIntro row name is None.");
        return;
    }

    if (!StageIntroDataTable->GetRowNames().Contains(IntroRowName))
    {
        SF_LOG(
            Error,
            "[SFInGameHUDWidget] StageIntro row was not found: %s",
            *IntroRowName.ToString()
        );
        return;
    }

    APlayerController* PlayerController = GetOwningPlayer();

    if (!IsValid(PlayerController))
    {
        SF_LOG(Error, "[SFInGameHUDWidget] OwningPlayer is invalid.");
        return;
    }

    ActiveStageIntroWidget = CreateWidget<USFStageIntroWidget>(
        PlayerController,
        StageIntroWidgetClass
    );

    if (!IsValid(ActiveStageIntroWidget))
    {
        SF_LOG(Error, "[SFInGameHUDWidget] Failed to create StageIntroWidget.");
        return;
    }

    ActiveStageIntroWidget->OnDeactivated().AddUObject(
        this,
        &USFInGameHUDWidget::HandleStageIntroDeactivated
    );

    ActiveStageIntroWidget->AddToViewport(500);
    ActiveStageIntroWidget->ActivateWidget();

    if (!ActiveStageIntroWidget->StartStageIntro(
        StageIntroDataTable,
        IntroRowName
    ))
    {
        SF_LOG(Error, "[SFInGameHUDWidget] StartStageIntro failed.");

        ActiveStageIntroWidget->OnDeactivated().RemoveAll(this);
        ActiveStageIntroWidget->RemoveFromParent();
        ActiveStageIntroWidget = nullptr;
        return;
    }

    VisibilityBeforeStageIntro = GetVisibility();
    bWasGamePausedBeforeStageIntro =
        UGameplayStatics::IsGamePaused(this);

    SetVisibility(ESlateVisibility::Collapsed);

    if (!bWasGamePausedBeforeStageIntro)
    {
        UGameplayStatics::SetGamePaused(this, true);
    }

    FInputModeGameAndUI InputMode;
    InputMode.SetWidgetToFocus(
        ActiveStageIntroWidget->TakeWidget()
    );
    InputMode.SetHideCursorDuringCapture(false);
    InputMode.SetLockMouseToViewportBehavior(
        EMouseLockMode::LockAlways
    );

    PlayerController->SetInputMode(InputMode);
    PlayerController->bShowMouseCursor = true;

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] StageIntro started | Row: %s",
        *IntroRowName.ToString()
    );
}

void USFInGameHUDWidget::HandleStageIntroDeactivated()
{
    if (IsValid(ActiveStageIntroWidget))
    {
        ActiveStageIntroWidget->OnDeactivated().RemoveAll(this);
        ActiveStageIntroWidget->RemoveFromParent();
    }

    ActiveStageIntroWidget = nullptr;

    SetVisibility(VisibilityBeforeStageIntro);

    if (!bWasGamePausedBeforeStageIntro)
    {
        UGameplayStatics::SetGamePaused(this, false);
    }

    bWasGamePausedBeforeStageIntro = false;

    APlayerController* PlayerController = GetOwningPlayer();

    if (IsValid(PlayerController))
    {
        FInputModeGameAndUI InputMode;
        InputMode.SetHideCursorDuringCapture(false);
        InputMode.SetLockMouseToViewportBehavior(
            EMouseLockMode::LockAlways
        );

        PlayerController->SetInputMode(InputMode);
        PlayerController->bShowMouseCursor = true;
    }

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] Stage intro finished."
    );
}

bool USFInGameHUDWidget::RequestProductionBySlot(int32 SlotIndex)
{
    if (!IsValid(ProductionBuildingPanel))
    {
        return false;
    }

    return ProductionBuildingPanel->RequestProductionBySlot(SlotIndex);
}