#include "SFHealthBarWidget.h"

#include "Components/ProgressBar.h"

void USFHealthBarWidget::ApplyStatus(const FSFOverheadStatus& InStatus)
{
	if (!IsValid(HealthBar))	return;

	// 상태는 어느 필드가 바뀌어도 통째로 온다. 자기가 쓰는 값만 비교해 걸러낸다.
	if (!FMath::IsNearlyEqual(LastRatio, InStatus.HealthRatio, 0.005f))
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
