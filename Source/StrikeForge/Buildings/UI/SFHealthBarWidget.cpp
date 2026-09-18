#include "SFHealthBarWidget.h"

#include "Components/ProgressBar.h"

void USFHealthBarWidget::ApplyStatus(const FSFOverheadStatus& InStatus)
{
	if (!IsValid(HealthBar))	return;

	// 상태는 어느 필드가 바뀌어도 통째로 온다. 자기가 쓰는 값만 비교해 걸러낸다.
	// 상태 컴포넌트가 이미 유효 변화만 걸러 방송하므로, 받은 값을 그대로 비교하면 된다.
	if (LastRatio != InStatus.HealthRatio)
	{
		LastRatio = InStatus.HealthRatio;
		HealthBar->SetPercent(InStatus.HealthRatio);
	}

	if (!LastColor.Equals(InStatus.HealthColor))
	{
		LastColor = InStatus.HealthColor;
		HealthBar->SetFillColorAndOpacity(InStatus.HealthColor);
	}
}
