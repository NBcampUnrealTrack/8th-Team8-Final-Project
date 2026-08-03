#include "HealthComponent.h"


UHealthComponent::UHealthComponent()
	:bIsDead(false)
{
	PrimaryComponentTick.bCanEverTick = false;

}

void UHealthComponent::ApplyDamage(float InDamage)
{
	const float TempCurrentHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth - InDamage, 0, MaxHealth);
	OnHealthValueChanged.Broadcast(TempCurrentHealth, CurrentHealth, MaxHealth);
	
	if (CurrentHealth <= 0.f )
	{
		OnHealthStateChanged.Broadcast();
		bIsDead = true;
	}
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
	OnHealthValueChanged.Broadcast(0.f, CurrentHealth, MaxHealth);
}

