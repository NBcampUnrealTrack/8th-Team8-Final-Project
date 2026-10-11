#include "SFStatusIconWidget.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"

void USFStatusIconWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 게임 중에만 불린다. 디자이너에서는 배경이 그대로 보여 배치를 잡을 수 있다.
	if (IsValid(IconBackground))
	{
		IconBackground->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void USFStatusIconWidget::SetIcon(ESFOverheadStatusIcon InIcon)
{
	if (!IsValid(IconImage))	return;

	// 그림과 X 여부가 한 자리에 있으니 조회도 한 번이면 된다.
	const FSFStatusIconVisual* Visual = IconVisuals.Find(InIcon);
	const bool bHasIcon = Visual && IsValid(Visual->Icon);

	const ESlateVisibility IconVisibility = bHasIcon
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed;

	// 배경은 아이콘과 함께 뜨고 함께 접힌다.
	if (IsValid(IconBackground))
	{
		IconBackground->SetVisibility(IconVisibility);
	}

	IconImage->SetVisibility(IconVisibility);

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
