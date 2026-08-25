#include "CombatTargetComponent.h"

#include "ActorComponent/HP/HealthComponent.h"
#include "Game/Subsystem/SFCombatSubsystem.h"
#include "Interfaces/Units/Damagable.h"
#include "Logging/SFLog.h"

UCombatTargetComponent::UCombatTargetComponent()
	: bRegistered(false)
	, bDead(false)
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
		SF_LOG(Warning, "[CombatTarget] %s : HealthComponent 가 없어 전투 대상으로 등록하지 않습니다.",
			*Owner->GetName());
		return;
	}

	if (OverrideMaxHealth > 0.f)
	{
		HealthComponent->InitializeHealth(OverrideMaxHealth);
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
	// 사망 통지를 이미 받았으면 그것으로 끝이다.
	// 소유 액터가 DeathLifeSpan 동안 남아 있어도 전투 대상으로 보지 않는다.
	if (bDead) return false;

	// UPROPERTY 포인터는 GC 전까지 nullptr 이 되지 않는다.
	// Destroy() 예약 상태를 따로 확인해야 파괴 중인 액터를 걸러낸다.
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner) || Owner->IsActorBeingDestroyed()) return false;

	return IsValid(HealthComponent) && !HealthComponent->GetIsDead();
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
	// 등록 해제보다 먼저 세운다.
	// 해제 도중 돌아가는 델리게이트에서 IsAlive() 를 물어도 이미 false 여야 한다.
	bDead = true;

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
