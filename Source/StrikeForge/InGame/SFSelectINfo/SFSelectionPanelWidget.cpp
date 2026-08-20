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

    ClearSelection();
}

void USFSelectionPanelWidget::ShowSelection(
    const FSFSelectableInfo& SelectableInfo
)
{
    CurrentSelectableInfo = SelectableInfo;
    bHasSelection = true;

    // 버튼 표시 여부는 BP의 SelectableType 조건이 결정한다.
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
}

bool USFSelectionPanelWidget::HasSelection() const
{
    return bHasSelection;
}

void USFSelectionPanelWidget::NativeDestruct()
{
    if (IsValid(TierUpButton))
    {
        TierUpButton->OnClicked.RemoveDynamic(
            this,
            &USFSelectionPanelWidget::HandleTierUpgradeButtonClicked
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