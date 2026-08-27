#include "CombatTargetComponent.h"

#include "ActorComponent/HP/HealthComponent.h"
#include "Game/Subsystem/SFCombatSubsystem.h"
#include "Interfaces/Units/Damagable.h"
#include "GameFramework/Pawn.h"
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

	// 분류가 기본값 그대로다.
	//
	// ETargetType 기본값은 ProductionBuilding 이다. 폰에 붙은 채로 남아 있으면
	// 그 유닛은 건물로 분류되어 분리력 대상에서 빠지고(GetNearbyAlliesForSeparation
	// 이 CombatUnit 만 모은다), 사거리도 건물 규칙(BuildingRangeBonus)을 탄다.
	//
	// 조용히 어긋나는 종류의 실수라 스폰 시점에 한 번 알린다.
	if (TargetType == ETargetType::ProductionBuilding && Owner->IsA<APawn>())
	{
		SF_LOG(Warning,
			"[CombatTarget] %s : Target Type 이 기본값(ProductionBuilding)입니다. "
			"유닛이라면 BP 에서 CombatUnit 으로 지정하세요. "
			"지금 상태로는 분리력 대상에서 빠지고 건물 사거리 규칙을 탑니다.",
			*Owner->GetName());
	}

	if (USFCombatSubsystem* Combat = USFCombatSubsystem::Get(this))
	{
		Combat->RegisterTarget(this);
		bRegistered = true;
	}
	else
	{
		// 등록 실패는 "아무 일도 안 일어난다" 로만 나타난다.
		// HealthComponent 누락은 경고하면서 이쪽만 침묵하고 있었다.
		SF_LOG(Error,
			"[CombatTarget] %s : USFCombatSubsystem 을 찾지 못해 전투 대상으로 등록하지 못했습니다.",
			*Owner->GetName());
	}
}

void UCombatTargetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 체력 통지 구독을 되돌린다.
	//
	// 컴포넌트만 제거되고 소유 액터와 체력 컴포넌트가 살아남는 경우
	// (런타임 DestroyComponent, 컴포넌트 재부착 등) 체력 쪽 호출 목록에
	// 죽은 항목이 남는다. 약참조라 크래시는 없지만 목록이 계속 자란다.
	if (IsValid(HealthComponent))
	{
		HealthComponent->OnHealthIsZero.RemoveDynamic(this, &UCombatTargetComponent::HandleOwnerDeath);
	}

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

	// 이미 쓰러진 대상에는 넣지 않는다.
	//
	// 넣으면 OnHealthIsZero 가 다시 울려 HandleOwnerDeath 가 두 번 돌고,
	// 그 안의 OnTargetDestroyed 브로드캐스트도 두 번 나간다.
	// 같은 프레임에 광역 피해를 여러 번 맞는 상황에서 실제로 발생한다.
	if (bDead) return;

	// 가해자를 알리는 유일한 지점이다.
	//
	// 아래 IDamagable 경로는 피해량만 넘기므로 여기서 놓치면 가해자 정보가
	// 영구히 사라진다. 피해 적용 전에 울려서, 이 타격으로 쓰러지는 경우에도
	// 구독자가 가해자를 알 수 있게 한다.
	OnCombatDamaged.Broadcast(this, InFinalDamage, InInstigator);

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
	// 두 번 들어오는 경로가 있다.
	//
	// 아래 등록 해제는 bRegistered 로 막혀 있었지만 OnTargetDestroyed
	// 브로드캐스트는 막히지 않아, 사망 처리·킬 카운트·연출이 두 번 돌 수 있었다.
	if (bDead) return;

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
