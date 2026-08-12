#include "SFInGameHUDWidget.h"

#include "InGame/Controller/SFInGamePlayerController.h"
#include "SFSelectInfo/SFSelectable.h"
#include "SFSelectInfo/SFSelectionPanelWidget.h"
#include "HUD/Production/SFProductionBuildingPanelWidget.h"

#include "DrawDebugHelpers.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "Logging/SFLog.h"

#include "CommonActivatableWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Components/Button.h"

#include "SFSelectInfo/SFSelectionUIDataRow.h"
#include "Game/Subsystem/SFDataTableManager.h"
#include "Game/GameInstance/SFGameInstance.h"

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

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] "
        "OnSelectInput binding completed."
    );
}

void USFInGameHUDWidget::NativeDestruct()
{
    if (IsValid(ActiveBuildingModeWidget))
    {
        ActiveBuildingModeWidget->OnDeactivated().RemoveAll(this);
    }

    UnbindSelectedActorDestroyed();

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

    Super::NativeDestruct();
}

void USFInGameHUDWidget::HandleSelectInput()
{
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

    SelectedActor = nullptr;
    SelectedInfo = FSFSelectableInfo();

    RefreshBuildModeOpenButtonVisibility();

    if (IsValid(SelectionPanel))
    {
        SelectionPanel->ClearSelection();
    }

    if (IsValid(ProductionBuildingPanel))
    {
        ProductionBuildingPanel->HideProductionBuilding();
    }

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] "
        "Selected actor was destroyed. Selection cleared."
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
        SF_LOG(
            Log,
            "[SFInGameHUDWidget] "
            "Selection trace returned no hit."
        );

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

    const bool bIsSelectable =
        HitActor->GetClass()->ImplementsInterface(
            USFSelectable::StaticClass()
        );

    if (!bIsSelectable)
    {
        SF_LOG(
            Log,
            "[SFInGameHUDWidget] "
            "%s is not selectable.",
            *HitActor->GetName()
        );

        ClearSelection();
        return;
    }

    FSFSelectableInfo SelectableInfo =
        ISFSelectable::Execute_GetSelectableInfo(
            HitActor
        );

    ApplySelectionUIData(
        SelectableInfo
    );

    ApplySelection(
        HitActor,
        SelectableInfo
    );

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] "
        "Selected | Actor: %s | Name: %s | "
        "Health: %.1f / %.1f | RowName: %s",
        *HitActor->GetName(),
        *SelectableInfo.DisplayName.ToString(),
        SelectableInfo.CurrentHealth,
        SelectableInfo.MaximumHealth,
        *SelectableInfo.DataRowName.ToString()
    );

    DrawDebugSphere(
        GetWorld(),
        HitResult.ImpactPoint,
        30.0f,
        12,
        FColor::Green,
        false,
        1.5f
    );
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

    // 같은 대상을 다시 선택하면 파괴 이벤트는 재연결하지 않고
    // 최신 선택 정보만 다시 표시한다.
    if (SelectedActor == NewSelectedActor)
    {
        SelectedInfo = NewSelectedInfo;

        RefreshBuildModeOpenButtonVisibility();

        SF_LOG(
            Log,
            "[SFInGameHUDWidget] Selection Type: %d | "
            "ProductionPanel Valid: %s",
            static_cast<int32>(SelectedInfo.SelectableType),
            IsValid(ProductionBuildingPanel)
            ? TEXT("True")
            : TEXT("False")
        );

        if (IsValid(SelectionPanel))
        {
            SelectionPanel->ShowSelection(
                SelectedInfo
            );
        }
        if (IsValid(ProductionBuildingPanel))
        {
            if (
                SelectedInfo.SelectableType ==
                ESFSelectableType::ProductionBuilding
                )
            {
                SF_LOG(
                    Log,
                    "[SFInGameHUDWidget] "
                    "Show ProductionBuildingPanel"
                );

                ProductionBuildingPanel->ShowProductionBuilding(
                    SelectedActor.Get()
                );
            }
            else
            {
                SF_LOG(
                    Log,
                    "[SFInGameHUDWidget] "
                    "Hide ProductionBuildingPanel | Type: %d",
                    static_cast<int32>(SelectedInfo.SelectableType)
                );

                ProductionBuildingPanel->HideProductionBuilding();
            }
        }
        else
        {
            SF_LOG(
                Error,
                "[SFInGameHUDWidget] "
                "ProductionBuildingPanel is invalid."
            );
        }

        return;
    }

    UnbindSelectedActorDestroyed();

    SelectedActor = NewSelectedActor;
    SelectedInfo = NewSelectedInfo;

    RefreshBuildModeOpenButtonVisibility();

    SelectedActor->OnDestroyed.AddUniqueDynamic(
        this,
        &USFInGameHUDWidget::HandleSelectedActorDestroyed
    );

    if (IsValid(SelectionPanel))
    {
        SelectionPanel->ShowSelection(
            SelectedInfo
        );
    }

    if (IsValid(ProductionBuildingPanel))
    {
        if (
            SelectedInfo.SelectableType ==
            ESFSelectableType::ProductionBuilding
            )
        {
            ProductionBuildingPanel->ShowProductionBuilding(
                SelectedActor.Get()
            );
        }
        else
        {
            ProductionBuildingPanel->HideProductionBuilding();
        }
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

void USFInGameHUDWidget::ClearSelection()
{
    UnbindSelectedActorDestroyed();

    SelectedActor = nullptr;
    SelectedInfo = FSFSelectableInfo();

    RefreshBuildModeOpenButtonVisibility();

    if (IsValid(SelectionPanel))
    {
        SelectionPanel->ClearSelection();
    }

    if (IsValid(ProductionBuildingPanel))
    {
        ProductionBuildingPanel->HideProductionBuilding();
    }

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] "
        "Selection cleared."
    );
}

void USFInGameHUDWidget::OpenBuildingMode()
{
    if (
        !IsValid(BuildingUIStack) ||
        !IsValid(BuildingModeWidgetClass)
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

    if (IsValid(BuildModeOpenButton))
    {
        BuildModeOpenButton->SetVisibility(
            ESlateVisibility::Collapsed
        );
    }
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

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] "
        "Building mode deactivated."
    );
    
    ASFInGamePlayerController* SFPC = Cast<ASFInGamePlayerController>(GetOwningPlayer());
    if (!IsValid(SFPC)) return;
    
    SFPC->ClearSpawnedBuildingRef();
    SFPC->SetSFControlMode(ESFControlMode::Gameplay);
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