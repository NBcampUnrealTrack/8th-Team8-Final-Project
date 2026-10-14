#include "SFOverheadPanelWidget.h"

#include "Buildings/UI/SFHealthBarWidget.h"
#include "Buildings/UI/SFProgressBarWidget.h"
#include "Buildings/UI/SFResourceGainWidget.h"
#include "Buildings/UI/SFStatusIconWidget.h"
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
	/*
	 * 바뀐 필드만 골라 해당 자식에게만 넘긴다.
	 *
	 * 여기가 상태 구조체의 종점이다. 자식에게 통째로 넘기면 체력이 바뀔 때마다
	 * 진행도 바까지 깨어나고, 자식마다 "내 값이 바뀌었나" 판단하는 캐시를 따로 들어야 한다.
	 * 자식이 어느 필드를 쓰는지는 이 클래스가 이미 안다 — 타입이 박힌 멤버로 들고 있으니까.
	 */
	const bool bForce = !bAppliedOnce;

	if (IsValid(HealthBarWidget))
	{
		if (bForce || LastStatus.HealthRatio != InStatus.HealthRatio)
		{
			HealthBarWidget->SetRatio(InStatus.HealthRatio);
		}

		if (bForce || !LastStatus.HealthColor.Equals(InStatus.HealthColor))
		{
			HealthBarWidget->SetColor(InStatus.HealthColor);
		}
	}

	if (IsValid(ProgressBarWidget))
	{
		if (bForce || LastStatus.ProgressRatio != InStatus.ProgressRatio)
		{
			ProgressBarWidget->SetRatio(InStatus.ProgressRatio);
		}

		if (bForce || !LastStatus.ProgressColor.Equals(InStatus.ProgressColor))
		{
			ProgressBarWidget->SetColor(InStatus.ProgressColor);
		}

		if (bForce || LastStatus.SynergyBonusPercent != InStatus.SynergyBonusPercent)
		{
			ProgressBarWidget->SetSynergyBonusPercent(InStatus.SynergyBonusPercent);
		}
	}

	if (IsValid(StatusIconWidget) && (bForce || LastStatus.StatusIcon != InStatus.StatusIcon))
	{
		StatusIconWidget->SetIcon(InStatus.StatusIcon);
	}

	// 획득 배지는 상태가 아니라 사건을 그리므로 여기서 제외한다.

	LastStatus = InStatus;
	bAppliedOnce = true;
}

void USFOverheadPanelWidget::ResetForReuse()
{
	// 재생 중이던 배지를 멈춘다. 끝 신호는 매니저가 구독을 끊은 뒤라 아무도 받지 않는다.
	if (IsValid(ResourceGainWidget))
	{
		ResourceGainWidget->Stop();
	}

	for (int32 Index = 0; Index < static_cast<int32>(ESFOverheadElement::MAX); ++Index)
	{
		SetElementVisible(static_cast<ESFOverheadElement>(Index), false);
	}

	// 다음 건물의 첫 ApplyStatus가 비교 없이 전부 내려보내게 한다.
	LastStatus = FSFOverheadStatus();
	bAppliedOnce = false;
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
	 * 판 전체를 접는 건 매니저가 Collapsed로 따로 처리한다. 그쪽은 자리를 비워야 맞다.
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
	case ESFOverheadElement::StatusIcon:		return StatusIconWidget;
	default:									return nullptr;
	}
}
