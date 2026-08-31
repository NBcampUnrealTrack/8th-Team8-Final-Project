#include "SFSelectionPanelWidget.h"

#include "Components/Button.h"

#include "Logging/SFLog.h"


void USFSelectionPanelWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (IsValid(TierUpButton))
    {
        TierUpButton->SetIsEnabled(true);

        TierUpButton->SetVisibility(
            ESlateVisibility::Collapsed
        );

        TierUpButton->OnClicked.RemoveDynamic(
            this,
            &USFSelectionPanelWidget::HandleTierUpgradeButtonClicked
        );

        TierUpButton->OnClicked.AddDynamic(
            this,
            &USFSelectionPanelWidget::HandleTierUpgradeButtonClicked
        );

        SF_LOG(
            Log,
            "[SFSelectionPanelWidget] "
            "TierUpButton OnClicked binding completed."
        );
    }

    else
    {
        SF_LOG(
            Error,
            "[SFSelectionPanelWidget] "
            "TierUpButton is invalid."
        );
    }


    if (IsValid(RallyPointButton))
    {
        RallyPointButton->SetVisibility(
            ESlateVisibility::Collapsed
        );
        RallyPointButton->OnClicked.RemoveDynamic(
            this,
            &USFSelectionPanelWidget::HandleRallyPointButtonClicked
        );

        RallyPointButton->OnClicked.AddUniqueDynamic(
            this,
            &USFSelectionPanelWidget::HandleRallyPointButtonClicked
        );

        SF_LOG(
            Log,
            "[SFSelectionPanelWidget] "
            "RallyPointButton OnClicked binding completed."
        );
    }
    ClearSelection();
}

void USFSelectionPanelWidget::ShowSelection(
    const FSFSelectableInfo& SelectableInfo
)
{
    CurrentSelectableInfo = SelectableInfo;
    bHasSelection = true;

    BP_OnSelectionInfoUpdated(
        CurrentSelectableInfo
    );

    SetIsEnabled(true);

    SetVisibility(
        ESlateVisibility::SelfHitTestInvisible
    );

    if (IsValid(TierUpButton))
    {
        TierUpButton->SetIsEnabled(true);
    }

    if (IsValid(RallyPointButton))
    {
        const bool bIsProductionBuilding =
            SelectableInfo.SelectableType ==
            ESFSelectableType::ProductionBuilding;

        RallyPointButton->SetVisibility(
            bIsProductionBuilding
            ? ESlateVisibility::Visible
            : ESlateVisibility::Collapsed
        );
    }

    SF_LOG(
        Log,
        "[SFSelectionPanelWidget] "
        "ShowSelection | Type: %d | "
        "ButtonValid: %s | ButtonEnabled: %s",
        static_cast<int32>(
            SelectableInfo.SelectableType
            ),
        IsValid(TierUpButton)
        ? TEXT("True")
        : TEXT("False"),
        IsValid(TierUpButton) &&
        TierUpButton->GetIsEnabled()
        ? TEXT("True")
        : TEXT("False")
    );
}

void USFSelectionPanelWidget::ClearSelection()
{
    CurrentSelectableInfo = FSFSelectableInfo();
    bHasSelection = false;

    SetVisibility(
        ESlateVisibility::SelfHitTestInvisible
    );

    BP_OnSelectionCleared();

    if (IsValid(TierUpButton))
    {
        TierUpButton->SetVisibility(
            ESlateVisibility::Collapsed
        );
    }

    if (IsValid(RallyPointButton))
    {
        RallyPointButton->SetVisibility(
            ESlateVisibility::Collapsed
        );
    }

    SetRallyPointButtonVisible(false);
}

bool USFSelectionPanelWidget::HasSelection() const
{
    return bHasSelection;
}

void USFSelectionPanelWidget::NativeDestruct()
{
    if (IsValid(RallyPointButton))
    {
        RallyPointButton->OnClicked.RemoveDynamic(
            this,
            &USFSelectionPanelWidget::HandleRallyPointButtonClicked
        );
    }

    Super::NativeDestruct();
}

void USFSelectionPanelWidget::HandleTierUpgradeButtonClicked()
{
    SF_LOG(
        Log,
        "[SFSelectionPanelWidget] "
        "Tier upgrade button clicked."
    );

    OnTierUpgradeRequested.Broadcast();
}

bool USFSelectionPanelWidget::IsTierUpButtonHovered() const
{
    return
        IsValid(TierUpButton) &&
        TierUpButton->IsHovered();
}

void USFSelectionPanelWidget::SetRallyPointButtonVisible(bool bVisible)
{
    if (!IsValid(RallyPointButton))
    {
        return;
    }

    RallyPointButton->SetVisibility(
        bVisible
        ? ESlateVisibility::Visible
        : ESlateVisibility::Collapsed
    );
}

void USFSelectionPanelWidget::HandleRallyPointButtonClicked()
{
    OnRallyPointRequested.Broadcast();
}

void USFSelectionPanelWidget::SetTierUpButtonVisible(bool bVisible)
{
    if (!IsValid(TierUpButton))
    {
        return;
    }

    TierUpButton->SetVisibility(
        bVisible
        ? ESlateVisibility::Visible
        : ESlateVisibility::Collapsed
    );
}