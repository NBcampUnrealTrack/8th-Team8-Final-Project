#include "CombatTargetComponent.h"

#include "ActorComponent/HP/HealthComponent.h"
#include "Game/Subsystem/SFCombatSubsystem.h"
#include "Interfaces/Units/Damagable.h"

UCombatTargetComponent::UCombatTargetComponent()
	: bRegistered(false)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCombatTargetComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!IsValid(Owner)) return;

	HealthComponent = Owner->FindComponentByClass<UHealthComponent>();

	if (!IsValid(HealthComponent))
	{
		// 체력이 없으면 공격받을 수 없으므로 등록하지 않는다.
		// 광물 노드처럼 파괴되지 않는 액터가 여기 해당한다.
		UE_LOG(LogTemp, Warning,
			TEXT("[CombatTarget] %s : HealthComponent 가 없어 전투 대상으로 등록하지 않습니다."),
			*Owner->GetName());
		return;
	}

	if (OverrideMaxHealth > 0.f)
	{
		HealthComponent->InitHealth(OverrideMaxHealth);
	}

	HealthComponent->OnHealthIsZero.AddDynamic(this, &UCombatTargetComponent::HandleOwnerDeath);

	if (USFCombatSubsystem* Combat = USFCombatSubsystem::Get(this))
	{
		Combat->RegisterTarget(this);
		bRegistered = true;
	}
}

void UCombatTargetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 레벨 전환·강제 파괴 등 어떤 경로로 사라져도 반드시 해제한다
	if (bRegistered)
	{
		if (USFCombatSubsystem* Combat = USFCombatSubsystem::Get(this))
		{
			Combat->UnregisterTarget(this);
		}
		bRegistered = false;
	}

	Super::EndPlay(EndPlayReason);
}

void UCombatTargetComponent::SetTeamIndex(int32 InTeamIndex)
{
	if (TeamIndex == InTeamIndex) return;

	TeamIndex = InTeamIndex;

	// 이미 등록된 뒤에 팀이 바뀌면 버킷을 옮겨야 한다
	if (bRegistered)
	{
		if (USFCombatSubsystem* Combat = USFCombatSubsystem::Get(this))
		{
			Combat->UnregisterTarget(this);
			Combat->RegisterTarget(this);
		}
	}
}

bool UCombatTargetComponent::IsAlive() const
{
	return IsValid(HealthComponent) && !HealthComponent->IsDead();
}

void UCombatTargetComponent::ApplyCombatDamage(float InFinalDamage, AActor* InInstigator)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner)) return;

	// 액터가 자체 피해 처리를 갖고 있으면 그쪽을 존중한다.
	// 없으면 체력 컴포넌트에 직접 적용한다.
	if (Owner->Implements<UDamagable>())
	{
		IDamagable::Execute_TakeDamaged(Owner, InFinalDamage);
		return;
	}

	if (IsValid(HealthComponent))
	{
		HealthComponent->ApplyDamage(InFinalDamage);
	}
}

void UCombatTargetComponent::HandleOwnerDeath()
{
	// 사망 즉시 목록에서 뺀다.
	// 이걸 먼저 해야 다른 유닛이 시체를 계속 때리지 않는다.
	if (bRegistered)
	{
		if (USFCombatSubsystem* Combat = USFCombatSubsystem::Get(this))
		{
			Combat->UnregisterTarget(this);
		}
		bRegistered = false;
	}

	OnTargetDestroyed.Broadcast(this);
}
