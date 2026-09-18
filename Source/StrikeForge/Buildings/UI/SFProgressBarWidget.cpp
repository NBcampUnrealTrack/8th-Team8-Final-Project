#include "SFProgressBarWidget.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"

void USFProgressBarWidget::ApplyStatus(const FSFOverheadStatus& InStatus)
{
	if (!IsValid(ProgressBar))	return;

	// 상태는 어느 필드가 바뀌어도 통째로 온다. 자기가 쓰는 값만 비교해 걸러낸다.
	// 상태 컴포넌트가 이미 유효 변화만 걸러 방송하므로, 받은 값을 그대로 비교하면 된다.
	if (LastRatio != InStatus.ProgressRatio)
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
