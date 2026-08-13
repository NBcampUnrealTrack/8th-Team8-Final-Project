#include "SFSelectionPanelWidget.h"
#include "Logging/SFLog.h"

void USFSelectionPanelWidget::NativeConstruct()
{
    Super::NativeConstruct();

    ClearSelection();
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

    SF_LOG(
        Log,
        "[SFSelectionPanelWidget] ShowSelection | "
        "Widget: %s | Class: %s | RowName: %s",
        *GetName(),
        *GetClass()->GetName(),
        *SelectableInfo.DataRowName.ToString()
    );

    BP_OnSelectionInfoUpdated(
        CurrentSelectableInfo
    );
}

void USFSelectionPanelWidget::ClearSelection()
{
    CurrentSelectableInfo = FSFSelectableInfo();
    bHasSelection = false;

    // 패널은 유지하고 선택 정보만 초기화한다.
    SetVisibility(
        ESlateVisibility::HitTestInvisible
    );

    BP_OnSelectionCleared();
}

bool USFSelectionPanelWidget::HasSelection() const
{
    return bHasSelection;
}