#include "SFOverheadStatusComponent.h"

#include "ActorComponent/Stat/HealthComponent.h"
#include "Kismet/KismetMathLibrary.h"

namespace SFOverheadStatus
{
	/** 이보다 작은 변화는 화면에서 구분되지 않는다. 불필요한 슬레이트 무효화를 막는다. */
	constexpr float RatioEpsilon = 0.005f;
}

USFOverheadStatusComponent::USFOverheadStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USFOverheadStatusComponent::BindHealth(UHealthComponent* InHealthComponent)
{
	if (!IsValid(InHealthComponent))	return;

	InHealthComponent->OnHealthValueChanged.AddUniqueDynamic(
		this, &USFOverheadStatusComponent::HandleHealthChanged);

	// 구독 이후의 변화만 오므로 현재 값을 한 번 반영한다.
	HandleHealthChanged(
		InHealthComponent->GetCurrentHealth(),
		InHealthComponent->GetCurrentHealth(),
		InHealthComponent->GetMaxHealth());
}

void USFOverheadStatusComponent::UnbindHealth(UHealthComponent* InHealthComponent)
{
	if (!IsValid(InHealthComponent))	return;

	InHealthComponent->OnHealthValueChanged.RemoveDynamic(
		this, &USFOverheadStatusComponent::HandleHealthChanged);
}

void USFOverheadStatusComponent::HandleHealthChanged(float InBeforeHP, float InCurrentHP, float InMaxHP)
{
	const float NewRatio = FMath::Clamp(
		UKismetMathLibrary::SafeDivide(InCurrentHP, InMaxHP), 0.f, 1.f);

	if (FMath::IsNearlyEqual(Status.HealthRatio, NewRatio, SFOverheadStatus::RatioEpsilon))	return;

	Status.HealthRatio = NewRatio;
	NotifyChanged();
}

void USFOverheadStatusComponent::HandleProgressRatio(float InRatio)
{
	SetProgressRatio(InRatio);
}

void USFOverheadStatusComponent::HandleExtractProgressRatio(EResourceType InResourceType, float InRatio)
{
	// 자원 종류는 색을 정할 때 한 번 쓰이고, 매 갱신에는 필요 없다.
	SetProgressRatio(InRatio);
}

void USFOverheadStatusComponent::SetProgressRatio(float InRatio)
{
	const float NewRatio = FMath::Clamp(InRatio, 0.f, 1.f);

	if (FMath::IsNearlyEqual(Status.ProgressRatio, NewRatio, SFOverheadStatus::RatioEpsilon))	return;

	Status.ProgressRatio = NewRatio;
	NotifyChanged();
}

void USFOverheadStatusComponent::SetHealthColor(const FLinearColor& InColor)
{
	if (Status.HealthColor.Equals(InColor))	return;

	Status.HealthColor = InColor;
	NotifyChanged();
}

void USFOverheadStatusComponent::SetProgressColor(const FLinearColor& InColor)
{
	if (Status.ProgressColor.Equals(InColor))	return;

	Status.ProgressColor = InColor;
	NotifyChanged();
}

void USFOverheadStatusComponent::NotifyChanged()
{
	OnStatusChanged.Broadcast(Status);
}
