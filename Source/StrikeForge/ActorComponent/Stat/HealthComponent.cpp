#include "HealthComponent.h"
#include "Net/UnrealNetwork.h"


UHealthComponent::UHealthComponent()
	:IsDead(false)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

}
void UHealthComponent::ApplyDamage(float InDamage)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) return;
	// 이미 죽은 대상이 사망 이벤트를 다시 쏘지 않게 막는다.
	// 같은 프레임에 광역 피해를 여러 번 맞는 경우가 있다.
	if (IsDead) return;

	const float PreviousHealth = CurrentHealth;

	CurrentHealth = FMath::Clamp(CurrentHealth - InDamage,0.f,MaxHealth);

	// 건설 진행 중 누적된 부동소수점 오차를 제거한다.
	const bool bReachedZero = CurrentHealth <= KINDA_SMALL_NUMBER;

	if (bReachedZero)
	{
		CurrentHealth = 0.f;
		IsDead = true;
	}

	// UI와 손상 외형 갱신
	OnHealthValueChanged.Broadcast(PreviousHealth,	CurrentHealth,	MaxHealth);

	// 사망 처리는 하나의 전용 이벤트로 전달
	if (bReachedZero)
	{
		OnHealthIsZero.Broadcast();
	}
}

void UHealthComponent::InitializeHealth(float InMaxHealth)
{
	InitializeHealth(InMaxHealth, InMaxHealth);
}

void UHealthComponent::InitializeHealth(float InMaxHealth, float InCurrentHealth)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) return;
	const float PreviousHealth = CurrentHealth;

	MaxHealth = FMath::Max(1.f, InMaxHealth);
	InitialHealth = MaxHealth;
	CurrentHealth = FMath::Clamp(InCurrentHealth, 0.f, MaxHealth);
	IsDead = CurrentHealth <= 0.f; // 초기화할때 0이면 죽음판정. 지금은 당장 필요없음

	OnHealthValueChanged.Broadcast(PreviousHealth, CurrentHealth, MaxHealth);
}

void UHealthComponent::SetHealth(float InMaxHealth, float InCurrentHealth)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) return;
	const float PreviousHealth = CurrentHealth;

	MaxHealth = FMath::Max(1.f, InMaxHealth);
	CurrentHealth = FMath::Clamp(InCurrentHealth, 0.f, MaxHealth);
	IsDead = CurrentHealth <= 0.f; // 초기화할때 0이면 죽음판정. 지금은 당장 필요없음

	OnHealthValueChanged.Broadcast(PreviousHealth, CurrentHealth, MaxHealth);
}

void UHealthComponent::BoostHealth(float HealthMultiplier)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) return;
	const float PreviousHealth = CurrentHealth;
	const float HealthRatio = GetHealthPercent();

	MaxHealth = FMath::Max(1.f, InitialHealth * HealthMultiplier);
	CurrentHealth = FMath::Clamp(HealthRatio * MaxHealth, 1.f, MaxHealth);

	OnHealthValueChanged.Broadcast(PreviousHealth, CurrentHealth, MaxHealth);
}

void UHealthComponent::IncreaseCurrentHealth(float InAmount)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) return;
	// 현재체력 회복
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

void UHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 DOREPLIFETIME(UHealthComponent, MaxHealth);
 DOREPLIFETIME(UHealthComponent, CurrentHealth);
}
void UHealthComponent::OnRep_CurrentHealth(float PreviousHealth)
{
 const bool bWasDead=IsDead;
 IsDead=CurrentHealth<=0.f;
 OnHealthValueChanged.Broadcast(PreviousHealth,CurrentHealth,MaxHealth);
 if(IsDead&&!bWasDead) OnHealthIsZero.Broadcast();
}
void UHealthComponent::OnRep_MaxHealth()
{
 OnHealthValueChanged.Broadcast(CurrentHealth,CurrentHealth,MaxHealth);
}
