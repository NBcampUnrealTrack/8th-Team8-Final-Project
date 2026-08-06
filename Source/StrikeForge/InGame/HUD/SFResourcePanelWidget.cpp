#include "SFResourcePanelWidget.h"

#include "Components/TextBlock.h"

void USFResourcePanelWidget::UpdateMineral(
    int32 NewMineralValue,
    float NewMineralRate)
{
    if (MineralValueText)
    {
        MineralValueText->SetText(FText::AsNumber(NewMineralValue));
    }

    if (MineralRateText)
    {
        MineralRateText->SetText(MakeResourceRateText(NewMineralRate));
    }
}

void USFResourcePanelWidget::UpdateEther(
    int32 NewEtherValue,
    float NewEtherRate)
{
    if (EtherValueText)
    {
        EtherValueText->SetText(FText::AsNumber(NewEtherValue));
    }

    if (EtherRateText)
    {
        EtherRateText->SetText(MakeResourceRateText(NewEtherRate));
    }
}

void USFResourcePanelWidget::UpdatePopulation(
    int32 CurrentPopulation,
    int32 MaximumPopulation)
{
    if (!PopulationValueText)
    {
        return;
    }

    PopulationValueText->SetText(
        FText::Format(
            FText::FromString(TEXT("{0} / {1}")),
            FText::AsNumber(CurrentPopulation),
            FText::AsNumber(MaximumPopulation)));
}

FText USFResourcePanelWidget::MakeResourceRateText(float ResourceRate) const
{
    const int32 RoundedRate = FMath::RoundToInt(ResourceRate);

    if (RoundedRate > 0)
    {
        return FText::Format(
            FText::FromString(TEXT("+{0}/s")),
            FText::AsNumber(RoundedRate));
    }

    return FText::Format(
        FText::FromString(TEXT("{0}/s")),
        FText::AsNumber(RoundedRate));
}