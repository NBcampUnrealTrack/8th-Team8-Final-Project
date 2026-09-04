#include "UnitSpawner.h"

#include "Units/Unit/UnitBase.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
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

	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void AUnitSpawner::SetSpawningEnabled(bool bInEnabled)
{
	bSpawningEnabled = bInEnabled;

	if (bInEnabled)
	{
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
