#include "HealthComponent.h"


UHealthComponent::UHealthComponent()
	:IsDead(false)
{
	PrimaryComponentTick.bCanEverTick = false;

}
void UHealthComponent::ApplyDamage(float InDamage)
{
	// 이미 죽은 대상이 사망 이벤트를 다시 쏘지 않게 막는다.
	// 같은 프레임에 광역 피해를 여러 번 맞는 경우가 있다.
	if (IsDead) return;

	const float TempCurrentHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth - InDamage, 0, MaxHealth);
	OnHealthValueChanged.Broadcast(TempCurrentHealth, CurrentHealth, MaxHealth);

	if (CurrentHealth <= 0.f )
	{
		IsDead = true;
		OnHealthIsZero.Broadcast();
	}
}

void UHealthComponent::InitializeHealth(float InMaxHealth)
{
	InitializeHealth(InMaxHealth, InMaxHealth);
}

void UHealthComponent::InitializeHealth(float InMaxHealth, float InCurrentHealth)
{
	const float PreviousHealth = CurrentHealth;

	MaxHealth = FMath::Max(1.f, InMaxHealth);
	CurrentHealth = FMath::Clamp(InCurrentHealth, 0.f, MaxHealth);
	IsDead = CurrentHealth <= 0.f; // 초기화할때 0이면 죽음판정. 지금은 당장 필요없음

	OnHealthValueChanged.Broadcast(PreviousHealth,	CurrentHealth, MaxHealth);
}

void UHealthComponent::IncreaseCurrentHealth(float InAmount)
{	// 현재체력 회복
	if (IsDead || InAmount <= 0.f) return;

	const float PreviousHealth = CurrentHealth;
	// 최대 체력 넘지 않게 clamp
	CurrentHealth = FMath::Clamp(CurrentHealth + InAmount,0.f,MaxHealth);
	// 풀피에서 회복할때 broadcast 안돼게
	if (!FMath::IsNearlyEqual(PreviousHealth, CurrentHealth))
	{
		OnHealthValueChanged.Broadcast(PreviousHealth,	CurrentHealth,MaxHealth);
	}
}

// void UHealthComponent::IncreaseMaxAndCurrentHealth(float InAmount)
// {	// 최대체력과 현재체력을 같이 증가 ( 건설중 )
// 	if (IsDead || InAmount <= 0.f) return;
//
// 	const float PreviousHealth = CurrentHealth;
// 	MaxHealth += InAmount;
// 	CurrentHealth += InAmount;
// 	OnHealthValueChanged.Broadcast(PreviousHealth,	CurrentHealth,	MaxHealth);
// }

float UHealthComponent::GetHealthPercent() const
{
	return MaxHealth > 0.f ? CurrentHealth / MaxHealth : 0.f;
}
