#include "SFResourceGainWidget.h"

#include "ActorComponent/UI/SFOverheadStatusComponent.h"
#include "Animation/WidgetAnimation.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void USFResourceGainWidget::Play(EResourceType InResourceType, int32 InAmount)
{
	if (IsValid(AmountText))
	{
		AmountText->SetText(FText::FromString(FString::Printf(TEXT("+%d"), InAmount)));

		// 진행도 바와 같은 색을 쓴다. 같은 자원이라는 게 색으로 읽혀야 한다.
		//AmountText->SetColorAndOpacity(
		//	FSlateColor(SFOverheadUIColors::FromResourceType(InResourceType)));
	}

	if (IsValid(ResourceIcon))
	{
		if (const TObjectPtr<UTexture2D>* Found = ResourceIcons.Find(InResourceType))
		{
			ResourceIcon->SetBrushFromTexture(Found->Get());
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
