#include "SFProductionBuildingPanelWidget.h"

#include "GameFramework/Actor.h"
#include "Logging/SFLog.h"

void USFProductionBuildingPanelWidget::NativeConstruct()
{
    Super::NativeConstruct();

    HideProductionBuilding();
}

void USFProductionBuildingPanelWidget::ShowProductionBuilding(
    AActor* InProductionBuilding
)
{
    SF_LOG(
        Log,
        "[SFProductionBuildingPanelWidget] "
        "ShowProductionBuilding called | Actor: %s",
        IsValid(InProductionBuilding)
        ? *InProductionBuilding->GetName()
        : TEXT("Invalid")
    );

    if (!IsValid(InProductionBuilding))
    {
        SF_LOG(
            Warning,
            "[SFProductionBuildingPanelWidget] "
            "ProductionBuilding is invalid."
        );

        HideProductionBuilding();
        return;
    }

    CurrentProductionBuilding =
        InProductionBuilding;

    SetVisibility(
        ESlateVisibility::HitTestInvisible
    );

    BP_OnProductionBuildingUpdated(
        InProductionBuilding
    );

    SF_LOG(
        Log,
        "[SFProductionBuildingPanelWidget] "
        "Production building shown: %s",
        *InProductionBuilding->GetName()
    );
}

void USFProductionBuildingPanelWidget::HideProductionBuilding()
{
    CurrentProductionBuilding.Reset();

    SetVisibility(
        ESlateVisibility::Collapsed
    );

    BP_OnProductionBuildingHidden();
}

bool USFProductionBuildingPanelWidget::HasProductionBuilding() const
{
    return CurrentProductionBuilding.IsValid();
}

AActor*
USFProductionBuildingPanelWidget::GetProductionBuilding() const
{
    return CurrentProductionBuilding.Get();
}