#include "UnitSpawner.h"

#include "Units/Unit/UnitBase.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "Game/Subsystem/SFCombatSubsystem.h"
#include "Units/Lane/LanePath.h"

#include "InGame/HUD/Tutorial/SFTutorialSubsystem.h"

#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"
#include "Logging/SFLog.h"

/**
 * 활성화 게이트를 전부 무시하고 처음부터 스폰하게 한다.
 *
 * 튜토리얼 후반의 전투 밸런스를 보려고 매번 건물을 다 지을 수는 없다.
 */
static TAutoConsoleVariable<int32> CVarSpawnerIgnoreActivationGate(
	TEXT("sf.Spawner.IgnoreActivationGate"),
	0,
	TEXT("1 이면 UnitSpawner 의 활성화 조건을 무시하고 레벨 시작과 함께 스폰한다."),
	ECVF_Cheat);

AUnitSpawner::AUnitSpawner()
	: bStartActive(true)
	, bActivated(false)
	, bStopWhenOwnerBuildingDestroyed(true)
	, bRequireOwnerBuilding(false)
	, bOwnerBuildingDestroyed(false)
	, bSpawningEnabled(true)
{
	// 타이머로 돌리므로 틱이 필요 없다
	PrimaryActorTick.bCanEverTick = false;

	SpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SpawnPoint"));
	SetRootComponent(SpawnPoint);
}

void AUnitSpawner::BeginPlay()
{
	Super::BeginPlay();

	ResolveInitialActivation();

	// 소유 건물 연결은 여기서 끝나지 않는다. 건물은 앵커에서 조금 뒤에 세워진다.
	StartOwnerBuildingResolveTimer();

	if (bActivated && bSpawningEnabled)
	{
		StartSpawnTimer(InitialDelay);
	}
}

// ─────────────────────────────────────────────────────────
// 활성화 게이트
//
// 스포너를 "언제 등장하는가" 로 나눠 쓰기 위한 장치다. 게이트를 쓰지 않는
// 스포너는 지금까지와 완전히 동일하게 동작한다 — bStartActive 가 true 라서
// BeginPlay 에서 곧바로 깨어난다.
// ─────────────────────────────────────────────────────────
void AUnitSpawner::ResolveInitialActivation()
{
	if (bStartActive || CVarSpawnerIgnoreActivationGate.GetValueOnGameThread() != 0)
	{
		bActivated = true;
		return;
	}

	// 튜토리얼이 시키기 전에 플레이어가 먼저 지어버렸을 수 있다.
	// 그러면 신호는 이미 지나갔으므로 구독만으로는 영영 깨지 않는다.
	if (ActivationObjectiveTag.IsValid())
	{
		if (const USFTutorialSubsystem* Tutorial = USFTutorialSubsystem::Get(this))
		{
			if (Tutorial->IsObjectiveCompleted(ActivationObjectiveTag))
			{
				bActivated = true;

				SF_LOG(Log, "[UnitSpawner] %s : 목표가 이미 달성돼 있어 바로 활성화합니다 | Tag: %s",
					*GetName(), *ActivationObjectiveTag.ToString());

				return;
			}
		}
	}

	bActivated = false;

	BindActivationSignal();

	SF_LOG(Log, "[UnitSpawner] %s : 대기 상태로 시작합니다 | Tag: %s",
		*GetName(),
		ActivationObjectiveTag.IsValid()
			? *ActivationObjectiveTag.ToString()
			: TEXT("(없음 - ActivateSpawner 를 직접 불러야 깨어납니다)"));
}

void AUnitSpawner::BindActivationSignal()
{
	if (!ActivationObjectiveTag.IsValid())
	{
		return;
	}

	USFTutorialSubsystem* Tutorial = USFTutorialSubsystem::Get(this);

	if (!IsValid(Tutorial))
	{
		SF_LOG(Warning, "[UnitSpawner] %s : 튜토리얼 서브시스템을 찾지 못했습니다. 활성화 신호를 받을 수 없습니다.", *GetName());
		return;
	}

	Tutorial->OnObjectiveRecorded.AddUniqueDynamic(this, &AUnitSpawner::HandleObjectiveRecorded);
}

void AUnitSpawner::UnbindActivationSignal()
{
	if (USFTutorialSubsystem* Tutorial = USFTutorialSubsystem::Get(this))
	{
		Tutorial->OnObjectiveRecorded.RemoveDynamic(this, &AUnitSpawner::HandleObjectiveRecorded);
	}
}

void AUnitSpawner::HandleObjectiveRecorded(FGameplayTag InObjectiveTag)
{
	if (bActivated)
	{
		return;
	}

	// 계층 매칭. 걸어둔 쪽이 부모 태그면 자식 목표로도 열린다.
	if (!ActivationObjectiveTag.IsValid() || !InObjectiveTag.MatchesTag(ActivationObjectiveTag))
	{
		return;
	}

	SF_LOG(Log, "[UnitSpawner] %s : 목표 달성으로 활성화됩니다 | Tag: %s", *GetName(), *InObjectiveTag.ToString());

	ActivateSpawner();
}

void AUnitSpawner::ActivateSpawner()
{
	if (bActivated)
	{
		return;
	}

	// 건물이 부서진 뒤에 늦게 도착한 목표 신호로 되살아나면 안 된다.
	if (bOwnerBuildingDestroyed)
	{
		SF_LOG(Log, "[UnitSpawner] %s : 소유 건물이 파괴돼 다시 깨어나지 않습니다.", *GetName());
		return;
	}

	bActivated = true;

	// 한 번 깨면 신호를 더 받을 이유가 없다
	UnbindActivationSignal();

	if (bSpawningEnabled)
	{
		// 첫 스폰까지 InitialDelay 를 다시 준다. 조건을 만족한 순간 적이 눈앞에
		// 튀어나오면 플레이어가 무엇 때문에 나온 건지 알 수 없다.
		StartSpawnTimer(InitialDelay);
	}
}

void AUnitSpawner::DeactivateSpawner()
{
	bActivated = false;

	StopSpawnTimer();
}

bool AUnitSpawner::IsSpawnerActivated() const
{
	return bActivated;
}

// ─────────────────────────────────────────────────────────
// 소유 건물
//
// 스포너는 적 건물에 붙여 쓰는 물건인데, 건물이 부서져도 스포너 액터는
// 레벨에 그대로 남아 계속 유닛을 뽑아낸다. 그래서 건물의 파괴 신호를
// 받아 스스로 멈춰야 한다.
//
// 어려운 부분은 "내 건물" 을 찾는 일이다. 레벨에 놓인 BP_BuildingSpawn 은
// 건물이 아니라 앵커(마커)이고, 실제 건물은 SFGameMode 가 에셋 로딩을
// 마친 뒤에 세운다. 스포너 BeginPlay 보다 늦다. 그래서 연결을 타이머로
// 미뤄 두고, 찾을 때까지 짧은 주기로 재시도한다.
// ─────────────────────────────────────────────────────────

namespace
{
	/** 건물 계열인가. AUnitBase::IsBuildingTarget 과 같은 기준이다 */
	bool IsBuildingTargetType(ETargetType InType)
	{
		return InType == ETargetType::ProductionBuilding
			|| InType == ETargetType::CoreBuilding
			|| InType == ETargetType::ResourceBuilding;
	}

	/** 재시도 주기(초). 건물은 보통 첫 두세 번 안에 나타난다 */
	constexpr float OwnerBuildingResolveInterval = 0.5f;
}

AActor* AUnitSpawner::GetOwnerBuilding() const
{
	return OwnerBuilding.Get();
}

bool AUnitSpawner::IsShutDownByOwnerBuilding() const
{
	return bOwnerBuildingDestroyed;
}

void AUnitSpawner::StartOwnerBuildingResolveTimer()
{
	if (!bStopWhenOwnerBuildingDestroyed || bOwnerBuildingDestroyed)
	{
		return;
	}

	OwnerBuildingResolveElapsed = 0.f;

	// 첫 시도는 즉시. 건물에 직접 어태치해 둔 배치는 여기서 바로 끝난다.
	ResolveOwnerBuilding();

	if (OwnerBuildingTarget.IsValid())
	{
		return;
	}

	GetWorldTimerManager().SetTimer(
		OwnerBuildingResolveTimerHandle,
		this, &AUnitSpawner::ResolveOwnerBuilding,
		OwnerBuildingResolveInterval,
		true);
}

void AUnitSpawner::StopOwnerBuildingResolveTimer()
{
	GetWorldTimerManager().ClearTimer(OwnerBuildingResolveTimerHandle);
}

void AUnitSpawner::ResolveOwnerBuilding()
{
	if (OwnerBuildingTarget.IsValid() || bOwnerBuildingDestroyed)
	{
		StopOwnerBuildingResolveTimer();
		return;
	}

	// ① 어태치 부모부터 거슬러 올라간다.
	//    건물에 직접 붙여둔 배치를 그대로 존중한다. 앵커에 붙였다면 여기서는 안 걸린다.
	for (AActor* Parent = GetAttachParentActor(); IsValid(Parent); Parent = Parent->GetAttachParentActor())
	{
		UCombatTargetComponent* Target = USFCombatSubsystem::FindTargetComponent(Parent);

		if (IsValid(Target) && IsBuildingTargetType(Target->GetTargetType()))
		{
			BindOwnerBuilding(Target);
			return;
		}
	}

	// ② 같은 팀 건물 중 반경 안 최근접.
	//    앵커에 붙여둔 스포너는 건물이 자기 위에 서므로 이쪽으로 잡힌다.
	if (const USFCombatSubsystem* Combat = USFCombatSubsystem::Get(this))
	{
		TArray<AActor*> TeamActors;
		Combat->GetTeamTargets(TeamIndex, TeamActors);

		const FVector Origin = GetActorLocation();

		UCombatTargetComponent* Nearest = nullptr;
		float NearestDistSq = TNumericLimits<float>::Max();

		for (AActor* Actor : TeamActors)
		{
			if (!IsValid(Actor))
			{
				continue;
			}

			// 유닛은 제외한다. DT 를 안 거친 유닛은 TargetType 이 기본값
			// (ProductionBuilding) 이라 건물로 잘못 잡힌다.
			if (Actor->IsA<AUnitBase>())
			{
				continue;
			}

			UCombatTargetComponent* Target = USFCombatSubsystem::FindTargetComponent(Actor);

			if (!IsValid(Target) || !IsBuildingTargetType(Target->GetTargetType()))
			{
				continue;
			}

			// 높이는 보지 않는다. 앵커와 건물 원점의 Z 가 어긋나 있어도 상관없게.
			const float DistSq = FVector::DistSquared2D(Origin, Actor->GetActorLocation());

			if (DistSq <= OwnerBuildingSearchRadius * OwnerBuildingSearchRadius && DistSq < NearestDistSq)
			{
				NearestDistSq = DistSq;
				Nearest = Target;
			}
		}

		if (IsValid(Nearest))
		{
			BindOwnerBuilding(Nearest);
			return;
		}
	}

	OwnerBuildingResolveElapsed += OwnerBuildingResolveInterval;

	if (OwnerBuildingResolveElapsed < OwnerBuildingResolveTimeout)
	{
		return;
	}

	// 조용히 포기하지 않는다. 다음 사람이 "왜 안 멈추지" 로 시간을 쓰지 않도록.
	StopOwnerBuildingResolveTimer();

	SF_LOG(Warning,
		"[UnitSpawner] %s : 반경 %.0fcm 안에서 팀 %d 건물을 찾지 못했습니다. "
		"건물이 부서져도 멈추지 않습니다.%s",
		*GetName(),
		OwnerBuildingSearchRadius,
		TeamIndex,
		bRequireOwnerBuilding
			? TEXT(" Require Owner Building 이 켜져 있어 스폰하지 않습니다.")
			: TEXT(""));
}

void AUnitSpawner::BindOwnerBuilding(UCombatTargetComponent* InTarget)
{
	if (!IsValid(InTarget))
	{
		return;
	}

	StopOwnerBuildingResolveTimer();

	OwnerBuildingTarget = InTarget;
	OwnerBuilding = InTarget->GetOwner();

	InTarget->OnTargetDestroyed.AddUniqueDynamic(this, &AUnitSpawner::HandleOwnerBuildingDestroyed);

	SF_LOG(Log, "[UnitSpawner] %s : 소유 건물 연결 | Building: %s | Team: %d",
		*GetName(), *GetNameSafe(OwnerBuilding.Get()), TeamIndex);

	// 연결한 순간 이미 죽어 있을 수 있다. 그 경우 신호는 이미 지나갔다.
	if (!InTarget->IsAlive())
	{
		HandleOwnerBuildingDestroyed(InTarget);
	}
}

void AUnitSpawner::UnbindOwnerBuilding()
{
	if (UCombatTargetComponent* Target = OwnerBuildingTarget.Get())
	{
		Target->OnTargetDestroyed.RemoveDynamic(this, &AUnitSpawner::HandleOwnerBuildingDestroyed);
	}

	OwnerBuildingTarget.Reset();
}

void AUnitSpawner::HandleOwnerBuildingDestroyed(UCombatTargetComponent* InTarget)
{
	if (bOwnerBuildingDestroyed)
	{
		return;
	}

	bOwnerBuildingDestroyed = true;

	SF_LOG(Log,
		"[UnitSpawner] %s : 소유 건물이 파괴돼 스폰을 영구 정지합니다 | Building: %s | "
		"이미 나온 유닛 %d 기는 그대로 둡니다.",
		*GetName(),
		*GetNameSafe(OwnerBuilding.Get()),
		GetAliveCount());

	UnbindOwnerBuilding();
	StopOwnerBuildingResolveTimer();

	// 목표 신호로 다시 깨어나지 않도록 구독도 끊는다.
	UnbindActivationSignal();

	DeactivateSpawner();
}


void AUnitSpawner::StartSpawnTimer(float InFirstDelay)
{
	if (GetWorldTimerManager().IsTimerActive(SpawnTimerHandle))
	{
		return;
	}

	GetWorldTimerManager().SetTimer(
		SpawnTimerHandle,
		this, &AUnitSpawner::HandleSpawnCycle,
		SpawnInterval,
		true,
		InFirstDelay);
}

void AUnitSpawner::StopSpawnTimer()
{
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
}

void AUnitSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindActivationSignal();
	UnbindOwnerBuilding();
	StopOwnerBuildingResolveTimer();

	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void AUnitSpawner::SetSpawningEnabled(bool bInEnabled)
{
	bSpawningEnabled = bInEnabled;

	if (bInEnabled)
	{
		if (bOwnerBuildingDestroyed)
		{
			SF_LOG(Log, "[UnitSpawner] %s : 소유 건물이 파괴돼 스폰을 다시 켤 수 없습니다.", *GetName());
			return;
		}

		// 아직 깨어날 조건을 만족하지 않았으면 타이머를 돌리지 않는다.
		// 여기서 켜버리면 게이트가 있으나 마나가 된다.
		if (bActivated)
		{
			StartSpawnTimer(0.f);
		}
	}
	else
	{
		StopSpawnTimer();
	}
}

int32 AUnitSpawner::GetAliveCount() const
{
	int32 Count = 0;

	for (const TWeakObjectPtr<AUnitBase>& Weak : SpawnedUnits)
	{
		if (IsValid(Weak.Get()))
		{
			++Count;
		}
	}

	return Count;
}

void AUnitSpawner::HandleSpawnCycle()
{
	if (!bActivated || !bSpawningEnabled || !UnitClass) return;

	// 건물이 부서진 뒤에는 어떤 경로로 들어와도 스폰하지 않는다.
	if (bOwnerBuildingDestroyed) return;

	if (bStopWhenOwnerBuildingDestroyed && bRequireOwnerBuilding && !OwnerBuildingTarget.IsValid()) return;

	// 죽은 유닛의 약참조를 정리한다
	SpawnedUnits.RemoveAllSwap([](const TWeakObjectPtr<AUnitBase>& InWeak)
	{
		return !InWeak.IsValid();
	});

	for (int32 i = 0; i < SpawnCountPerCycle; ++i)
	{
		if (MaxTotal > 0 && TotalSpawned >= MaxTotal)
		{
			// 목표치를 다 채웠으면 타이머를 끈다
			GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
			return;
		}

		if (MaxAlive > 0 && GetAliveCount() >= MaxAlive)
		{
			return;		// 상한에 걸렸다. 다음 주기에 다시 시도.
		}

		SpawnOne();
	}
}

bool AUnitSpawner::FindSpawnLocation(FVector& OutLocation) const
{
	const FVector Origin = SpawnPoint ? SpawnPoint->GetComponentLocation() : GetActorLocation();

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

	// NavMesh 가 없으면 그냥 스폰 지점을 쓴다 (에디터 초기 세팅 중일 수 있다)
	if (!NavSys)
	{
		OutLocation = Origin;
		return true;
	}

	// 반경 안의 "도달 가능한" 지점을 뽑는다.
	// 이렇게 해야 벽 안이나 NavMesh 밖에 유닛이 끼는 사고를 막는다.
	FNavLocation NavLoc;
	if (SpawnScatterRadius > 0.f && NavSys->GetRandomReachablePointInRadius(Origin, SpawnScatterRadius, NavLoc))
	{
		OutLocation = NavLoc.Location;
		return true;
	}

	// 산포에 실패하면 스폰 지점 자체를 NavMesh 에 투영해 본다
	if (NavSys->ProjectPointToNavigation(Origin, NavLoc, FVector(200.f, 200.f, 500.f)))
	{
		OutLocation = NavLoc.Location;
		return true;
	}

	SF_LOG(Warning, "[UnitSpawner] %s : NavMesh 위에서 스폰 지점을 찾지 못했습니다. NavMeshBoundsVolume 을 확인하세요.", *GetName());
	OutLocation = Origin;
	return false;
}

AUnitBase* AUnitSpawner::SpawnOne()
{
    // BP의 직접 호출도 타이머와 같은 게이트를 통과해야 한다.
    if (!bActivated || !bSpawningEnabled || bOwnerBuildingDestroyed) return nullptr;
    if (bStopWhenOwnerBuildingDestroyed && bRequireOwnerBuilding && !OwnerBuildingTarget.IsValid()) return nullptr;

	if (!UnitClass)
	{
		SF_LOG(Warning, "[UnitSpawner] %s : UnitClass 가 비어 있습니다.", *GetName());
		return nullptr;
	}

	FVector SpawnLocation;
	FindSpawnLocation(SpawnLocation);

	// 캡슐 절반 높이만큼 띄워야 바닥에 파묻히지 않는다
	SpawnLocation.Z += 90.f;

	const FTransform SpawnTM(GetActorRotation(), SpawnLocation);

	// Deferred 스폰: BeginPlay 가 돌기 전에 팀과 능력치를 심어야 한다.
	// 그래야 레지스트리에 올바른 팀으로 등록된다.
	AUnitBase* Unit = GetWorld()->SpawnActorDeferred<AUnitBase>(
		UnitClass,
		SpawnTM,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

	if (!Unit) return nullptr;

	// BeginPlay 전에 팀을 심어야 레지스트리에 올바른 버킷으로 들어간다
	if (UCombatTargetComponent* Comp = Unit->FindComponentByClass<UCombatTargetComponent>())
	{
		Comp->SetTeamIndex(TeamIndex);
	}
	Unit->SetAssignedLane(Lane);

	if (!UnitRowName.IsNone())
	{
		Unit->InitStatFromTable(UnitRowName);
	}

	UGameplayStatics::FinishSpawningActor(Unit, SpawnTM);

	SpawnedUnits.Add(Unit);
	++TotalSpawned;

	OnUnitSpawned(Unit);

	return Unit;
}
