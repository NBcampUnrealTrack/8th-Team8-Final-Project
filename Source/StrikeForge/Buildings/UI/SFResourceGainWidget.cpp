#include "SFResourceGainWidget.h"

#include "ActorComponent/UI/SFOverheadStatusComponent.h"
#include "Animation/WidgetAnimation.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void USFResourceGainWidget::Play(EResourceType InResourceType, int32 InAmount)
{
	// 아이콘과 테두리 색이 한 자리에 있으니 조회도 한 번이면 된다.
	const FSFResourceGainVisual* Visual = ResourceVisuals.Find(InResourceType);

	if (IsValid(AmountText))
	{
		AmountText->SetText(FText::FromString(FString::Printf(TEXT("+%d"), InAmount)));

		// 진행도 바와 같은 색을 쓴다. 같은 자원이라는 게 색으로 읽혀야 한다.
		AmountText->SetColorAndOpacity(
			FSlateColor(SFOverheadUIColors::FromResourceType(InResourceType)));

		/*
		 * 테두리는 색만 바꾼다.
		 *
		 * 폰트 정보를 통째로 받아 OutlineColor 한 필드만 고쳐 되돌려준다.
		 * 그래야 WBP에 지정한 Outline Size와 폰트, 머티리얼이 그대로 남는다.
		 */
		FSlateFontInfo FontInfo = AmountText->GetFont();

		if (!bDefaultOutlineCached)
		{
			// 맵에 없는 자원을 만났을 때 돌아갈 자리. 덮어쓰기 전에 잡아둬야 한다.
			DefaultOutlineColor = FontInfo.OutlineSettings.OutlineColor;
			bDefaultOutlineCached = true;
		}

		FontInfo.OutlineSettings.OutlineColor = Visual
			? Visual->OutlineColor
			: DefaultOutlineColor;

		AmountText->SetFont(FontInfo);
	}

	if (IsValid(ResourceIcon))
	{
		const bool bHasIcon = Visual && IsValid(Visual->Icon);
		ResourceIcon->SetVisibility(bHasIcon
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);

		if (bHasIcon)
		{
			ResourceIcon->SetBrushFromTexture(Visual->Icon);
		}
	}

	if (!IsValid(GainAnim))
	{
		/*
		 * 애니메이션을 지정하지 않은 WBP도 동작해야 한다.
		 * 여기서 끝났다고 알리지 않으면 컴포넌트가 비트를 못 내려 판이 영원히 떠 있는다.
		 */
		OnFinished.Broadcast();
		return;
	}

	if (!bFinishedBound)
	{
		FWidgetAnimationDynamicEvent FinishedDelegate;
		FinishedDelegate.BindDynamic(this, &USFResourceGainWidget::HandleAnimationFinished);
		BindToAnimationFinished(GainAnim, FinishedDelegate);

		bFinishedBound = true;
	}

	// 재생 중이어도 0초부터 다시 시작한다.
	PlayAnimation(GainAnim);
}

void USFResourceGainWidget::HandleAnimationFinished()
{
	OnFinished.Broadcast();
}
