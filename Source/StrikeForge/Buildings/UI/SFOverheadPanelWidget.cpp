#include "SFOverheadPanelWidget.h"

#include "Buildings/UI/SFHealthBarWidget.h"
#include "Buildings/UI/SFProgressBarWidget.h"
#include "Buildings/UI/SFResourceGainWidget.h"
#include "Components/Spacer.h"

void USFOverheadPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 위젯 트리가 만들어진 직후 한 번. 화면에 올라가기 전이라도 호출된다.
	if (IsValid(ResourceGainWidget))
	{
		ResourceGainWidget->OnFinished.AddUObject(
			this, &USFOverheadPanelWidget::HandleResourceGainFinished);
	}
}

void USFOverheadPanelWidget::ApplyStatus(const FSFOverheadStatus& InStatus)
{
	// 걸러내기는 자식이 한다. 자기가 쓰는 필드가 안 바뀌었으면 자식이 되돌아간다.
	if (IsValid(HealthBarWidget))
	{
		HealthBarWidget->ApplyStatus(InStatus);
	}

	if (IsValid(ProgressBarWidget))
	{
		ProgressBarWidget->ApplyStatus(InStatus);
	}

	// 획득 배지는 상태가 아니라 사건을 그리므로 여기서 제외한다.
}

void USFOverheadPanelWidget::SetElementVisible(ESFOverheadElement InElement, bool bInVisible)
{
	UUserWidget* Element = FindElement(InElement);
	if (!IsValid(Element))	return;

	/*
	 * Hidden이지 Collapsed가 아니다.
	 *
	 * Collapsed는 레이아웃 자리까지 없애서, 체력바가 꺼지면 진행도 바가 그 자리로 올라온다.
	 * 요소마다 화면 위치가 고정돼 있어야 눈이 따라가기 쉬우므로 자리는 남긴다.
	 *
	 * 판 전체를 접는 건 컴포넌트가 Collapsed로 따로 처리한다. 그쪽은 자리를 비워야 맞다.
	 */
	Element->SetVisibility(bInVisible
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Hidden);
}

void USFOverheadPanelWidget::SetElementSpacing(float InSpacing)
{
	// 스페이서를 안 넣은 판도 있다. 그런 판은 간격 조절을 쓰지 않는 것이다.
	if (!IsValid(ElementSpacer))	return;

	ElementSpacer->SetSize(FVector2D(0.f, FMath::Max(InSpacing, 0.f)));
}

void USFOverheadPanelWidget::PlayResourceGain(EResourceType InResourceType, int32 InAmount)
{
	if (!IsValid(ResourceGainWidget))
	{
		// 배지가 없는 판이면 켜진 채로 남지 않도록 바로 끝났다고 알린다.
		OnElementFinished.Broadcast(ESFOverheadElement::ResourceGain);
		return;
	}

	ResourceGainWidget->Play(InResourceType, InAmount);
}

void USFOverheadPanelWidget::HandleResourceGainFinished()
{
	OnElementFinished.Broadcast(ESFOverheadElement::ResourceGain);
}

UUserWidget* USFOverheadPanelWidget::FindElement(ESFOverheadElement InElement) const
{
	switch (InElement)
	{
	case ESFOverheadElement::Health:			return HealthBarWidget;
	case ESFOverheadElement::Progress:			return ProgressBarWidget;
	case ESFOverheadElement::ResourceGain:		return ResourceGainWidget;
	default:									return nullptr;
	}
}
