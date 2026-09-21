#include "SFProgressBarWidget.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"

void USFProgressBarWidget::SetRatio(float InRatio)
{
	if (!IsValid(ProgressBar))	return;

	ProgressBar->SetPercent(InRatio);
}

void USFProgressBarWidget::SetColor(const FLinearColor& InColor)
{
	if (!IsValid(ProgressBar))	return;

	ProgressBar->SetFillColorAndOpacity(InColor);

	if (IsValid(ProgressBarBackground))
	{
		// 채워진 색을 어둡게 깔아 같은 자원이라는 게 읽히게 한다.
		FLinearColor BackgroundColor = InColor * 0.3f;
		BackgroundColor.A = 0.5f;

		ProgressBarBackground->SetColorAndOpacity(BackgroundColor);
	}
}
