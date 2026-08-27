#include "SFMultiSelectionEntryWidget.h"

#include "ActorComponent/HP/HealthComponent.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "GameFramework/Actor.h"
#include "SFSelectable.h"

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
    UnbindHealthDelegate();

    if (IsValid(EntryButton))
    {
        EntryButton->OnClicked.RemoveDynamic(
            this,
            &USFMultiSelectionEntryWidget::HandleEntryClicked
        );
    }

    RepresentedActor = nullptr;
    EntryIndex = INDEX_NONE;
    bShouldShowHealth = false;

    Super::NativeDestruct();
}

void USFMultiSelectionEntryWidget::InitializeEntry(
    AActor* Actor,
    const FSFSelectableInfo& SelectableInfo,
    int32 Index
)
{
    UnbindHealthDelegate();

    RepresentedActor = Actor;
    EntryIndex = Index;
    bShouldShowHealth = SelectableInfo.bShowHealth;

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

    RefreshHealthBar(
        SelectableInfo.CurrentHealth,
        SelectableInfo.MaximumHealth
    );

    BindHealthDelegate(RepresentedActor.Get());
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

void USFMultiSelectionEntryWidget::BindHealthDelegate(AActor* Actor)
{
    if (!bShouldShowHealth || !IsValid(Actor))
    {
        return;
    }

    if (!Actor->GetClass()->ImplementsInterface(USFSelectable::StaticClass()))
    {
        return;
    }

    const FHealthData HealthData =
        ISFSelectable::Execute_GetHealthData(Actor);

    UHealthComponent* HealthComponent =
        HealthData.HealthComponent.Get();

    if (!IsValid(HealthComponent))
    {
        return;
    }

    BoundHealthComponent = HealthComponent;

    HealthComponent->OnHealthValueChanged.RemoveDynamic(
        this,
        &USFMultiSelectionEntryWidget::HandleHealthChanged
    );

    HealthComponent->OnHealthValueChanged.AddUniqueDynamic(
        this,
        &USFMultiSelectionEntryWidget::HandleHealthChanged
    );
}

void USFMultiSelectionEntryWidget::UnbindHealthDelegate()
{
    UHealthComponent* HealthComponent =
        BoundHealthComponent.Get();

    if (IsValid(HealthComponent))
    {
        HealthComponent->OnHealthValueChanged.RemoveDynamic(
            this,
            &USFMultiSelectionEntryWidget::HandleHealthChanged
        );
    }

    BoundHealthComponent.Reset();
}

void USFMultiSelectionEntryWidget::HandleHealthChanged(
    float BeforeHP,
    float CurrentHP,
    float MaxHP
)
{
    static_cast<void>(BeforeHP);
    RefreshHealthBar(CurrentHP, MaxHP);
}

void USFMultiSelectionEntryWidget::RefreshHealthBar(
    float CurrentHP,
    float MaxHP
)
{
    if (!IsValid(HealthBar))
    {
        return;
    }

    const bool bCanShowHealth =
        bShouldShowHealth &&
        MaxHP > KINDA_SMALL_NUMBER;

    HealthBar->SetVisibility(
        bCanShowHealth
        ? ESlateVisibility::HitTestInvisible
        : ESlateVisibility::Collapsed
    );

    if (!bCanShowHealth)
    {
        HealthBar->SetPercent(0.f);
        return;
    }

    HealthBar->SetPercent(
        FMath::Clamp(
            CurrentHP / MaxHP,
            0.f,
            1.f
        )
    );
}