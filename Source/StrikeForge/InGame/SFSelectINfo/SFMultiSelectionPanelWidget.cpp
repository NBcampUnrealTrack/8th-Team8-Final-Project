#include "SFMultiSelectionPanelWidget.h"

#include "SFMultiSelectionEntryWidget.h"

#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "GameFramework/Actor.h"

#include "Logging/SFLog.h"

void USFMultiSelectionPanelWidget::ShowSelections(
    const TArray<TObjectPtr<AActor>>& Actors,
    const TArray<FSFSelectableInfo>& SelectableInfos
)
{
    ClearSelections();

    if (Actors.Num() <= 1)
    {
        return;
    }

    if (!IsValid(SelectionGrid))
    {
        SF_LOG(
            Error,
            "[SFMultiSelectionPanelWidget] SelectionGrid is invalid."
        );
        return;
    }

    if (!IsValid(EntryWidgetClass))
    {
        SF_LOG(
            Error,
            "[SFMultiSelectionPanelWidget] EntryWidgetClass is invalid."
        );
        return;
    }

    const int32 EntryCount = FMath::Min3(
        Actors.Num(),
        SelectableInfos.Num(),
        30
    );

    const int32 SafeColumnCount =
        FMath::Max(ColumnCount, 1);

    for (int32 Index = 0; Index < EntryCount; ++Index)
    {
        AActor* Actor = Actors[Index].Get();

        if (!IsValid(Actor))
        {
            continue;
        }

        USFMultiSelectionEntryWidget* EntryWidget =
            CreateWidget<USFMultiSelectionEntryWidget>(
                GetOwningPlayer(),
                EntryWidgetClass
            );

        if (!IsValid(EntryWidget))
        {
            continue;
        }

        const int32 Row = Index / SafeColumnCount;
        const int32 Column = Index % SafeColumnCount;

        SelectionGrid->AddChildToUniformGrid(
            EntryWidget,
            Row,
            Column
        );

        EntryWidget->InitializeEntry(
            Actor,
            SelectableInfos[Index],
            Index
        );

        EntryWidget->OnEntryClicked.AddUniqueDynamic(
            this,
            &USFMultiSelectionPanelWidget::HandleEntryClicked
        );

        EntryWidgets.Add(EntryWidget);
    }

    SetVisibility(
        EntryWidgets.IsEmpty()
        ? ESlateVisibility::Collapsed
        : ESlateVisibility::SelfHitTestInvisible
    );

    SF_LOG(
        Log,
        "[SFMultiSelectionPanelWidget] Entries created | Count: %d",
        EntryWidgets.Num()
    );
}

void USFMultiSelectionPanelWidget::ClearSelections()
{
    for (USFMultiSelectionEntryWidget* EntryWidget : EntryWidgets)
    {
        if (IsValid(EntryWidget))
        {
            EntryWidget->OnEntryClicked.RemoveDynamic(
                this,
                &USFMultiSelectionPanelWidget::HandleEntryClicked
            );
        }
    }

    EntryWidgets.Reset();

    if (IsValid(SelectionGrid))
    {
        SelectionGrid->ClearChildren();
    }

    SetVisibility(ESlateVisibility::Collapsed);
}

void USFMultiSelectionPanelWidget::HandleEntryClicked(
    AActor* Actor
)
{
    if (IsValid(Actor))
    {
        OnPrimarySelectionRequested.Broadcast(Actor);
    }
}

void USFMultiSelectionPanelWidget::NativeDestruct()
{
    ClearSelections();
    Super::NativeDestruct();
}