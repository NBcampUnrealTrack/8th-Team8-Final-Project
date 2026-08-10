#include "UnitSpawner.h"

#include "Units/Unit/UnitBase.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "Units/Lane/LanePath.h"

#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Logging/SFLog.h"

AUnitSpawner::AUnitSpawner()
	: bSpawningEnabled(true)
{
	// 타이머로 돌리므로 틱이 필요 없다
	PrimaryActorTick.bCanEverTick = false;

	SpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SpawnPoint"));
	SetRootComponent(SpawnPoint);
}

void AUnitSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (bSpawningEnabled)
	{
		GetWorldTimerManager().SetTimer(
			SpawnTimerHandle,
			this, &AUnitSpawner::HandleSpawnCycle,
			SpawnInterval,
			true,
			InitialDelay);
	}
}

void AUnitSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void AUnitSpawner::SetSpawningEnabled(bool bInEnabled)
{
	bSpawningEnabled = bInEnabled;

	if (bInEnabled)
	{
		if (!GetWorldTimerManager().IsTimerActive(SpawnTimerHandle))
		{
			GetWorldTimerManager().SetTimer(SpawnTimerHandle, this, &AUnitSpawner::HandleSpawnCycle, SpawnInterval, true, 0.f);
		}
	}
	else
	{
		GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
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
	if (!bSpawningEnabled || !UnitClass) return;

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
