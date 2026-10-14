#include "SFSelectionPanelWidget.h"

#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/Widget.h"
#include "Components/ListView.h"
#include "InGame/HUD/SFBuffTooltipWidget.h"
#include "UObject/UnrealType.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "InGame/HUD/Tutorial/SFTutorialSubsystem.h"

#include "Logging/SFLog.h"


void USFSelectionPanelWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (auto* BuffList = Cast<UListView>(GetWidgetFromName(TEXT("BuffListView"))))
    {
        BuffList->OnEntryWidgetGenerated().RemoveAll(this);
        BuffList->OnEntryWidgetGenerated().AddUObject(this, &ThisClass::HandleBuffEntryGenerated);
    }

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

    if (IsValid(AttackCommandButton))
    {
        AttackCommandButton->OnClicked.RemoveDynamic(this,&USFSelectionPanelWidget::HandleAttackCommandButtonClicked);

        AttackCommandButton->OnClicked.AddUniqueDynamic(this,&USFSelectionPanelWidget::HandleAttackCommandButtonClicked);
    }

    if (IsValid(StopCommandButton))
    {
        StopCommandButton->OnClicked.RemoveDynamic(this,&USFSelectionPanelWidget::HandleStopCommandButtonClicked);

        StopCommandButton->OnClicked.AddUniqueDynamic(this,&USFSelectionPanelWidget::HandleStopCommandButtonClicked);
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

    if (auto* Bar = Cast<UProgressBar>(GetWidgetFromName(TEXT("SelectedHealthBar"))))
    {
        const float Ratio = CurrentSelectableInfo.MaximumHealth > 0.f
            ? CurrentSelectableInfo.CurrentHealth / CurrentSelectableInfo.MaximumHealth : 0.f;
        FProgressBarStyle BarStyle = Bar->GetWidgetStyle();
        BarStyle.FillImage.TintColor = FSlateColor(FLinearColor::White);
        Bar->SetWidgetStyle(BarStyle);
        Bar->SetFillColorAndOpacity(Ratio < .3f ? FLinearColor(1.f,.06f,.035f)
            : Ratio < .6f ? FLinearColor(1.f,.5f,.05f) : FLinearColor(.03f,.65f,1.f));
    }

    // Blueprint refreshes (including health changes) must not resurrect a max-tier button.
    if (CurrentSelectableInfo.Tier >= 3) SetTierUpButtonVisible(false);

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

        SetRallyPointButtonVisible(bIsProductionBuilding);
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

    AttackCommandButton->SetVisibility(
        ESlateVisibility::Collapsed
    );

    StopCommandButton->SetVisibility(
        ESlateVisibility::Collapsed
    );

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

    SetCommandButtonsVisible(false);
    SetRallyPointButtonVisible(false);
    SetTierUpStageLocked(false);

    ClearMiningResourceAmount();
}

bool USFSelectionPanelWidget::HasSelection() const
{
    return bHasSelection;
}

void USFSelectionPanelWidget::SetMiningResourceAmount(
    int32 RemainingResourceAmount
)
{
    if (!IsValid(MiningResourceAmountText))
    {
        return;
    }

    MiningResourceAmountText->SetText(
        FText::Format(
            NSLOCTEXT(
                "SFSelectionPanel",
                "RemainingResourceFormat",
                "남은 자원: {0}"
            ),
            FText::AsNumber(
                FMath::Max(0, RemainingResourceAmount)
            )
        )
    );

    MiningResourceAmountText->SetVisibility(
        ESlateVisibility::Visible
    );
}

void USFSelectionPanelWidget::ClearMiningResourceAmount()
{
    if (IsValid(MiningResourceAmountText))
    {
        MiningResourceAmountText->SetVisibility(
            ESlateVisibility::Collapsed
        );
    }
}

void USFSelectionPanelWidget::NativeDestruct()
{
    if (auto* BuffList = Cast<UListView>(GetWidgetFromName(TEXT("BuffListView"))))
    {
        BuffList->OnEntryWidgetGenerated().RemoveAll(this);
    }
    if (IsValid(RallyPointButton))
    {
        RallyPointButton->OnClicked.RemoveDynamic(
            this,
            &USFSelectionPanelWidget::HandleRallyPointButtonClicked
        );
    }

    Super::NativeDestruct();
}

void USFSelectionPanelWidget::HandleBuffEntryGenerated(UUserWidget& EntryWidget)
{
    auto* BuffList = Cast<UListView>(GetWidgetFromName(TEXT("BuffListView")));
    UObject* Item = BuffList ? BuffList->GetListObjectFromEntry(EntryWidget) : nullptr;
    if (!Item) return;
    // BP_BuffEntry remains the source of both strings, including buff/debuff wording.
    const auto* Name = FindFProperty<FTextProperty>(Item->GetClass(), TEXT("Name"));
    const auto* Description = FindFProperty<FTextProperty>(Item->GetClass(), TEXT("Description"));
    if (!Name || !Description) return;
    auto* Tooltip = Cast<USFBuffTooltipWidget>(EntryWidget.GetToolTip());
    if (!Tooltip) Tooltip = CreateWidget<USFBuffTooltipWidget>(GetOwningPlayer());
    if (!Tooltip) return;
    Tooltip->SetDetails(Name->GetPropertyValue_InContainer(Item), Description->GetPropertyValue_InContainer(Item));
    EntryWidget.SetToolTip(Tooltip);
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
    bVisible = bVisible && bHasSelection && CurrentSelectableInfo.Tier >= 1 && CurrentSelectableInfo.Tier < 3;
    if (!IsValid(TierUpButton))
    {
        return;
    }

    // 튜토리얼이 이 스테이지에서 넥서스 업그레이드를 잠갔으면 띄우지 않는다.
    //
    // 넥서스(MainBuilding)에만 건다. 생산 건물의 티어업은 그대로 둔다 —
    // 잠그는 대상이 늘어나면 여기가 아니라 태그를 하나 더 만드는 쪽이 맞다.
    //
    // 버튼을 Visible 로 만드는 곳이 이 함수뿐이라 여기 한 곳만 막으면
    // 호출부 전부가 덮인다. 튜토리얼이 없는 스테이지에서는 잠금이 항상
    // 비어 있어 기존 동작 그대로다.
    if (bVisible &&
        CurrentSelectableInfo.SelectableType == ESFSelectableType::MainBuilding &&
        USFTutorialSubsystem::IsTierUpgradeLocked(this))
    {
        bVisible = false;
    }

    TierUpButton->SetVisibility(
        bVisible
        ? ESlateVisibility::Visible
        : ESlateVisibility::Collapsed
    );
}

void USFSelectionPanelWidget::HandleAttackCommandButtonClicked()
{
    ASFInGamePlayerController* PlayerController =
        Cast<ASFInGamePlayerController>(
            GetOwningPlayer()
        );

    if (!IsValid(PlayerController))
    {
        return;
    }

    PlayerController->BeginAttackCommand();
}

void USFSelectionPanelWidget::HandleStopCommandButtonClicked()
{
    ASFInGamePlayerController* PlayerController =
        Cast<ASFInGamePlayerController>(
            GetOwningPlayer()
        );

    if (!IsValid(PlayerController))
    {
        return;
    }

    PlayerController->StopSelectedUnits();
}

void USFSelectionPanelWidget::SetCommandButtonsVisible(
    bool bVisible
)
{
    const ESlateVisibility ButtonVisibility =
        bVisible
        ? ESlateVisibility::Visible
        : ESlateVisibility::Collapsed;

    if (IsValid(AttackCommandButton))
    {
        AttackCommandButton->SetVisibility(
            ButtonVisibility
        );
    }

    if (IsValid(StopCommandButton))
    {
        StopCommandButton->SetVisibility(
            ButtonVisibility
        );
    }
}

void USFSelectionPanelWidget::SetAttackCommandButtonVisible(
    bool bVisible
)
{
    if (!IsValid(AttackCommandButton))
    {
        return;
    }

    AttackCommandButton->SetVisibility(
        bVisible
        ? ESlateVisibility::Visible
        : ESlateVisibility::Collapsed
    );
}

void USFSelectionPanelWidget::SetUpgradeCost(
    const FResources& InUpgradeCost
)
{
    if (IsValid(UpgradeMineralCostText))
    {
        UpgradeMineralCostText->SetText(
            FText::AsNumber(InUpgradeCost.Mineral)
        );
    }

    if (IsValid(UpgradeEtherCostText))
    {
        UpgradeEtherCostText->SetText(
            FText::AsNumber(InUpgradeCost.Ether)
        );
    }
}

void USFSelectionPanelWidget::SetTierUpStageLocked(
    bool bLocked
)
{
    BP_OnTierUpStageLockChanged(
        bLocked
    );
}

void USFSelectionPanelWidget::SetUpgradeAffordability(
    bool bInCanAfford
)
{
    BP_OnUpgradeAffordabilityChanged(
        bInCanAfford
    );
}
