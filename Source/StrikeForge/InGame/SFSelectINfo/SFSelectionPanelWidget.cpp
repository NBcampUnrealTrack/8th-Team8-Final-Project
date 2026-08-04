#include "SFSelectionPanelWidget.h"

void USFSelectionPanelWidget::NativeConstruct()
{
    Super::NativeConstruct();

    HideSelection();
}

void USFSelectionPanelWidget::ShowSelection(
    const FSFSelectableInfo& SelectableInfo
)
{
    CurrentSelectableInfo = SelectableInfo;
    bHasSelection = true;

    SetVisibility(
        ESlateVisibility::HitTestInvisible
    );

    BP_OnSelectionInfoUpdated(
        CurrentSelectableInfo
    );
}

void USFSelectionPanelWidget::HideSelection()
{
    CurrentSelectableInfo = FSFSelectableInfo();
    bHasSelection = false;

    SetVisibility(
        ESlateVisibility::Collapsed
    );

    BP_OnSelectionHidden();
}

bool USFSelectionPanelWidget::HasSelection() const
{
    return bHasSelection;
}