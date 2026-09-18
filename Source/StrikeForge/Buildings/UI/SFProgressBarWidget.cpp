#include "SFProgressBarWidget.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"

void USFProgressBarWidget::ApplyStatus(const FSFOverheadStatus& InStatus)
{
	if (!IsValid(ProgressBar))	return;

	// SetPercent는 레이아웃 무효화를 부른다. 눈에 안 띄는 변화는 건너뛴다.
	if (!FMath::IsNearlyEqual(LastRatio, InStatus.ProgressRatio, 0.005f))
	{
		LastRatio = InStatus.ProgressRatio;
		ProgressBar->SetPercent(InStatus.ProgressRatio);
	}

	if (LastColor.Equals(InStatus.ProgressColor))	return;

	LastColor = InStatus.ProgressColor;
	ProgressBar->SetFillColorAndOpacity(InStatus.ProgressColor);

	if (IsValid(ProgressBarBackground))
	{
		FLinearColor BackgroundColor = InStatus.ProgressColor * 0.3f;
		BackgroundColor.A = 0.5f;

		ProgressBarBackground->SetColorAndOpacity(BackgroundColor);
	}
}
