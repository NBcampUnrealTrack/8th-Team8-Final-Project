#include "SFMineableResourceWidget.h"
#include "ActorComponent/Controller/BuildingPlacement.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "Components/TextBlock.h"

void USFMineableResourceWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::Collapsed);
    if (ASFInGamePlayerController* PC = GetOwningPlayer<ASFInGamePlayerController>())
    {
        BoundPlacement = PC->GetBuildingPlacementComponent();
        if (UBuildingPlacement* Placement = BoundPlacement.Get())
        {
            Placement->OnMineableResourceAmountChanged.AddUniqueDynamic(this, &ThisClass::HandleMineableResourceAmountChanged);
            HandleMineableResourceAmountChanged(Placement->GetMineableResourceAmount());
        }
    }
}

void USFMineableResourceWidget::NativeDestruct()
{
    if (UBuildingPlacement* Placement = BoundPlacement.Get())
        Placement->OnMineableResourceAmountChanged.RemoveDynamic(this, &ThisClass::HandleMineableResourceAmountChanged);
    BoundPlacement.Reset();
    Super::NativeDestruct();
}

void USFMineableResourceWidget::HandleMineableResourceAmountChanged(int32 Amount)
{
    const bool bShow = BoundPlacement.IsValid() && BoundPlacement->IsMiningPlacementActive();
    if (AmountText)
    {
        AmountText->SetText(FText::AsNumber(FMath::Max(0, Amount)));
        const bool bGold = BoundPlacement.IsValid()
            && BoundPlacement->GetPlacementResourceType() == EResourceType::Mineral;
        AmountText->SetColorAndOpacity(FSlateColor(bGold
            ? FLinearColor(1.f,.86f,.05f) : FLinearColor(.28f,.94f,.87f)));
    }
    SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
