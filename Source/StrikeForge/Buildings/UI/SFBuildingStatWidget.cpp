#include "SFBuildingStatWidget.h"
#include "SFHealthBarWidget.h"
#include "Components/Image.h"
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

	const float NewRatio = FMath::Clamp(InRatio, 0.f, 1.f);

	// SetPercent는 레이아웃 무효화를 부른다. 눈에 안 띄는 변화는 건너뛴다.
	if (FMath::IsNearlyEqual(LastProgressRatio, NewRatio, 0.005f))	return;

	LastProgressRatio = NewRatio;
	ProgressBar->SetPercent(NewRatio);
}

void USFBuildingStatWidget::SetProgressVisible(bool bInVisible)
{
	if (!IsValid(ProgressBarOverlay))	return;

	// Collapsed가 아니라 Hidden. 칸을 유지해야 체력바가 위아래로 안 움직인다.
	ProgressBarOverlay->SetVisibility(
		bInVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}

void USFBuildingStatWidget::SetProgressBarColor(const FLinearColor& InColor)
{
	if (IsValid(ProgressBar))
	{
		ProgressBar->SetFillColorAndOpacity(InColor);
	}

	if (IsValid(ProgressBarBackground))
	{
		FLinearColor BackgroundColor = InColor * 0.3f;
		BackgroundColor.A = 0.5f;

		ProgressBarBackground->SetColorAndOpacity(BackgroundColor);
	}
}
