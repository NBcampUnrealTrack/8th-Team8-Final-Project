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

void UHealthComponent::InitHealth(float InMaxHealth)
{
	MaxHealth = FMath::Max(1.f, InMaxHealth);
	CurrentHealth = MaxHealth;
	IsDead = false;

	OnHealthValueChanged.Broadcast(0.f, CurrentHealth, MaxHealth);
}

float UHealthComponent::GetHealthPercent() const
{
	return MaxHealth > 0.f ? CurrentHealth / MaxHealth : 0.f;
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	// 스폰 시 InitHealth 로 이미 채워졌다면 덮어쓰지 않는다.
	// 데이터테이블에서 주입한 수치가 기본값으로 되돌아가는 것을 막는다.
	if (CurrentHealth <= 0.f)
	{
		CurrentHealth = MaxHealth;
	}

	OnHealthValueChanged.Broadcast(0.f, CurrentHealth, MaxHealth);
}
