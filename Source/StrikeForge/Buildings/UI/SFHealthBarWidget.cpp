#include "SFHealthBarWidget.h"

#include "Components/ProgressBar.h"

void USFHealthBarWidget::SetRatio(float InRatio)
{
	if (!IsValid(HealthBar))	return;

	HealthBar->SetPercent(InRatio);
}

void USFHealthBarWidget::SetColor(const FLinearColor& InColor)
{
	if (!IsValid(HealthBar))	return;

	HealthBar->SetFillColorAndOpacity(InColor);
}
