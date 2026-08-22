#include "SFMultiSelectionEntryWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "GameFramework/Actor.h"

void USFMultiSelectionEntryWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (IsValid(EntryButton))
    {
        EntryButton->OnClicked.RemoveDynamic(
            this,
            &USFMultiSelectionEntryWidget::HandleEntryClicked
        );
        EntryButton->OnClicked.AddUniqueDynamic(
            this,
            &USFMultiSelectionEntryWidget::HandleEntryClicked
        );
    }
}

void USFMultiSelectionEntryWidget::NativeDestruct()
{
    if (IsValid(EntryButton))
    {
        EntryButton->OnClicked.RemoveDynamic(
            this,
            &USFMultiSelectionEntryWidget::HandleEntryClicked
        );
    }

    Super::NativeDestruct();
}

void USFMultiSelectionEntryWidget::InitializeEntry(
    AActor* Actor,
    const FSFSelectableInfo& SelectableInfo,
    int32 Index
)
{
    RepresentedActor = Actor;
    EntryIndex = Index;

    if (IsValid(EntryButton))
    {
        EntryButton->SetIsEnabled(IsValid(RepresentedActor));
    }

    if (IsValid(SelectionIconImage))
    {
        SelectionIconImage->SetBrushFromTexture(
            SelectableInfo.SelectionIcon,
            false
        );
    }

    if (IsValid(HealthBar))
    {
        const bool bHasHealth =
            SelectableInfo.bShowHealth &&
            SelectableInfo.MaximumHealth > KINDA_SMALL_NUMBER;

        HealthBar->SetVisibility(
            bHasHealth
            ? ESlateVisibility::HitTestInvisible
            : ESlateVisibility::Collapsed
        );

        if (bHasHealth)
        {
            HealthBar->SetPercent(
                FMath::Clamp(
                    SelectableInfo.CurrentHealth /
                    SelectableInfo.MaximumHealth,
                    0.f,
                    1.f
                )
            );
        }
    }
}

AActor* USFMultiSelectionEntryWidget::GetRepresentedActor() const
{
    return RepresentedActor.Get();
}

int32 USFMultiSelectionEntryWidget::GetSelectionIndex() const
{
    return EntryIndex;
}

void USFMultiSelectionEntryWidget::HandleEntryClicked()
{
    if (IsValid(RepresentedActor))
    {
        OnEntryClicked.Broadcast(
            RepresentedActor.Get()
        );
    }
}