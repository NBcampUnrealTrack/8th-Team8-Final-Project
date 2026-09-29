#include "SFProgressBarWidget.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

#define LOCTEXT_NAMESPACE "SFProgressBarWidget"

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

void USFProgressBarWidget::SetSynergyBonusPercent(float InBonusPercent)
{
	if (!IsValid(SynergyAmountText))	return;

	// 보너스가 없을 때는 텍스트가 진행도 바의 레이아웃을 차지하지 않도록 접는다.
	const bool bHasSynergyBonus = InBonusPercent > 0.f;
	SynergyAmountText->SetVisibility(bHasSynergyBonus
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed);

	if (!bHasSynergyBonus)
	{
		SynergyAmountText->SetText(FText::GetEmpty());
		return;
	}

	FNumberFormattingOptions NumberFormat;
	NumberFormat.MinimumFractionalDigits = 0;
	NumberFormat.MaximumFractionalDigits = 1;
	SynergyAmountText->SetText(FText::Format(
		LOCTEXT("SynergyBonusFormat", "+{0}%"),
		FText::AsNumber(InBonusPercent, &NumberFormat)));
}

#undef LOCTEXT_NAMESPACE
