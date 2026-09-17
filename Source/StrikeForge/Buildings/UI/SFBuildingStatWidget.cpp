#include "SFBuildingStatWidget.h"
#include "SFHealthBarWidget.h"
#include "Components/Overlay.h"
#include "Components/ProgressBar.h"

void USFBuildingStatWidget::SetHealthBar(float InBeforeHP, float InCurrentHP, float InMaxHP)
{
	if (!IsValid(HealthBarWidget))	return;

	HealthBarWidget->SetHealthBar(InBeforeHP, InCurrentHP, InMaxHP);
}

void USFBuildingStatWidget::SetHealthBarColor(const FLinearColor& InColor)
{
	if (!IsValid(HealthBarWidget))	return;

	HealthBarWidget->SetHealthBarColor(InColor);
}

void USFBuildingStatWidget::SetHeathBarVisible(bool InVisible)
{
	if (!IsValid(HealthBarWidget))	return;

	HealthBarWidget->SetVisibility(
		InVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}

void USFBuildingStatWidget::SetProgressRatio(float InRatio)
{
	if (!IsValid(ProgressBar))	return;

	ProgressBar->SetPercent(FMath::Clamp(InRatio, 0.f, 1.f));
}

void USFBuildingStatWidget::SetProgressVisible(bool bInVisible)
{
	if (!IsValid(ProgressBar))	return;

	// Collapsed가 아니라 Hidden. 칸을 유지해야 체력바가 위아래로 안 움직인다.
	ProgressBarOverlay->SetVisibility(
		bInVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}