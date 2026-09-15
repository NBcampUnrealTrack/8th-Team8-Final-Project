#include "SFMultiSelectionEntryWidget.h"

#include "ActorComponent/HP/HealthComponent.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "InputCoreTypes.h"
#include "GameFramework/PlayerController.h"
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
            SelectableInfo.PortraitImage,
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
    if (!IsValid(RepresentedActor))
    {
        return;
    }

    if (IsValid(SelectionClickSound))
    {
        UGameplayStatics::PlaySound2D(
            this,
            SelectionClickSound
        );
    }

    APlayerController* PlayerController =
        GetOwningPlayer();

    const bool bShiftPressed =
        PlayerController != nullptr &&
        (
            PlayerController->IsInputKeyDown(EKeys::LeftShift) ||
            PlayerController->IsInputKeyDown(EKeys::RightShift)
            );

    if (bShiftPressed)
    {
        OnEntryShiftClicked.Broadcast(
            RepresentedActor.Get()
        );

        return;
    }

    OnEntryClicked.Broadcast(
        RepresentedActor.Get()
    );
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

    // const FHealthData HealthData =
    //     ISFSelectable::Execute_GetHealthData(Actor);
    //
    // UHealthComponent* HealthComponent =
    //     HealthData.HealthComponent.Get();

    UHealthComponent* HealthComponent = Actor->FindComponentByClass<UHealthComponent>();
    
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
        HealthBar->SetPercent(0.0f);
        return;
    }

    const float HealthPercent =
        FMath::Clamp(
            CurrentHP / MaxHP,
            0.0f,
            1.0f
        );

    HealthBar->SetPercent(HealthPercent);

    FLinearColor HealthColor;

    if (HealthPercent >= 0.6f)
    {
        // 정상: 청록색
        HealthColor = FLinearColor(
            0.02f,
            0.85f,
            0.72f,
            1.0f
        );
    }
    else if (HealthPercent >= 0.3f)
    {
        // 주의: 주황색
        HealthColor = FLinearColor(
            0.95f,
            0.52f,
            0.06f,
            1.0f
        );
    }
    else 
    {
        // 위험: 빨간색
        HealthColor = FLinearColor(
            1.0f,
            0.0f,
            0.0f,
            1.0f
        );
    }

    HealthBar->SetFillColorAndOpacity(
        HealthColor
    );
}
