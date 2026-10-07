#include "SFStatusIconWidget.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"

void USFStatusIconWidget::SetIcon(ESFOverheadStatusIcon InIcon)
{
	if (!IsValid(IconImage))	return;

	// 그림과 X 여부가 한 자리에 있으니 조회도 한 번이면 된다.
	const FSFStatusIconVisual* Visual = IconVisuals.Find(InIcon);
	const bool bHasIcon = Visual && IsValid(Visual->Icon);

	IconImage->SetVisibility(bHasIcon
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed);

	if (bHasIcon)
	{
		IconImage->SetBrushFromTexture(Visual->Icon);
	}

	if (IsValid(CrossImage))
	{
		CrossImage->SetVisibility(bHasIcon && Visual->bShowCross
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}
