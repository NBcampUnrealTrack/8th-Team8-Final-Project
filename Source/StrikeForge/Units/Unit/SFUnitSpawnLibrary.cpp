#include "SFUnitSpawnLibrary.h"

#include "UnitBase.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "Buildings/SFRootBase.h"
#include "Datas/Unit/UnitStatData.h"
#include "Game/Subsystem/SFDataTableManager.h"
#include "Logging/SFLog.h"

#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"

int32 USFUnitSpawnLibrary::ResolveTeamIndex(const AActor* InActor)
{
	if (!IsValid(InActor)) return 0;

	// 건물 계열은 ASFRootBase 가 팀 번호를 들고 있다
	if (const ASFRootBase* RootBase = Cast<ASFRootBase>(InActor))
	{
		return RootBase->GetTeamIndex();
	}

	// 유닛·스포너 계열은 전투 컴포넌트가 들고 있다
	if (const UCombatTargetComponent* Combat = InActor->FindComponentByClass<UCombatTargetComponent>())
	{
		return Combat->GetTeamIndex();
	}

	SF_LOG(Warning, "[UnitSpawn] %s : 팀 번호를 찾지 못해 0 으로 생성합니다.", *InActor->GetName());
	return 0;
}

bool USFUnitSpawnLibrary::FindSpawnLocation(
	const UObject* InWorldContext,
	const FVector& InOrigin,
	float InScatterRadius,
	FVector& OutLocation)
{
	OutLocation = InOrigin;

	// const UWorld* 를 넘기면 GetCurrent 가 const 포인터를 돌려준다.
	// GetRandomReachablePointInRadius 는 const 함수가 아니므로 비-const 로 받는다.
	UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(InWorldContext, EGetWorldErrorMode::ReturnNull)
		: nullptr;

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

	// NavMesh 가 없으면 원점을 그대로 쓴다 (에디터 초기 세팅 중일 수 있다)
	if (!NavSys) return true;

	FNavLocation NavLoc;

	// 도달 가능한 지점을 뽑아야 벽 안이나 NavMesh 밖에 유닛이 끼지 않는다
	if (InScatterRadius > 0.f && NavSys->GetRandomReachablePointInRadius(InOrigin, InScatterRadius, NavLoc))
	{
		OutLocation = NavLoc.Location;
		return true;
	}

	// 산포에 실패하면 원점 자체를 NavMesh 에 투영해 본다
	if (NavSys->ProjectPointToNavigation(InOrigin, NavLoc, FVector(300.f, 300.f, 500.f)))
	{
		OutLocation = NavLoc.Location;
		return true;
	}

	SF_LOG(Warning, "[UnitSpawn] NavMesh 위에서 생성 지점을 찾지 못했습니다. 주변 통로를 확인하세요.");
	return false;
}

AUnitBase* USFUnitSpawnLibrary::SpawnUnitFromRow(
	AActor* InSpawnerActor,
	FName InUnitRowName,
	float InSpawnDistance,
	float InScatterRadius,
	TSubclassOf<AUnitBase> InOverrideClass)
{
	if (!IsValid(InSpawnerActor))
	{
		SF_LOG(Error, "[UnitSpawn] 생성 주체가 유효하지 않습니다.");
		return nullptr;
	}

	UWorld* World = InSpawnerActor->GetWorld();
	if (!IsValid(World)) return nullptr;

	// ── 테이블 조회 ─────────────────────────────────────
	const FUnitStatData* StatRow = nullptr;

	if (const UGameInstance* GI = World->GetGameInstance())
	{
		if (USFDataTableManager* TableManager = GI->GetSubsystem<USFDataTableManager>())
		{
			StatRow = TableManager->GetDataRow<FUnitStatData>(
				EDataTableType::Unit, InUnitRowName, TEXT("SFUnitSpawnLibrary"));
		}
	}

	// ── 생성할 클래스 결정 ──────────────────────────────
	// 인자로 넘어온 클래스가 최우선, 없으면 테이블의 UnitClass 를 로드한다.
	TSubclassOf<AUnitBase> SpawnClass = InOverrideClass;

	if (!SpawnClass && StatRow && !StatRow->UnitClass.IsNull())
	{
		// 생산 시점에만 호출되므로 동기 로드로 충분하다
		SpawnClass = StatRow->UnitClass.LoadSynchronous();
	}

	if (!SpawnClass)
	{
		SF_LOG(Error,
			"[UnitSpawn] %s : 생성할 유닛 클래스를 찾지 못했습니다. "
			"DT_Units 의 UnitClass 를 지정하거나 호출부에서 클래스를 넘기세요.",
			*InUnitRowName.ToString());
		return nullptr;
	}

	// ── 위치 결정 ───────────────────────────────────────
	// 건물 정면으로 InSpawnDistance 만큼 떨어진 지점을 기준으로 삼는다
	const FVector Origin =
		InSpawnerActor->GetActorLocation() + InSpawnerActor->GetActorForwardVector() * InSpawnDistance;

	FVector SpawnLocation;
	FindSpawnLocation(InSpawnerActor, Origin, InScatterRadius, SpawnLocation);

	// 캡슐 절반 높이만큼 띄워야 바닥에 파묻히지 않는다
	SpawnLocation.Z += 90.f;

	const FTransform SpawnTM(InSpawnerActor->GetActorRotation(), SpawnLocation);

	// ── 생성 ────────────────────────────────────────────
	// Deferred 스폰: BeginPlay 전에 팀과 능력치를 심어야
	// 전투 레지스트리에 올바른 팀으로 등록된다.
	AUnitBase* Unit = World->SpawnActorDeferred<AUnitBase>(
		SpawnClass,
		SpawnTM,
		InSpawnerActor,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

	if (!Unit)
	{
		SF_LOG(Error, "[UnitSpawn] %s 생성에 실패했습니다.", *InUnitRowName.ToString());
		return nullptr;
	}

	const int32 TeamIndex = ResolveTeamIndex(InSpawnerActor);

	if (UCombatTargetComponent* Combat = Unit->FindComponentByClass<UCombatTargetComponent>())
	{
		Combat->SetTeamIndex(TeamIndex);
	}

	Unit->InitStatFromTable(InUnitRowName);

	UGameplayStatics::FinishSpawningActor(Unit, SpawnTM);

	SF_LOG(Log, "[UnitSpawn] %s 생성 완료 (Team %d, 생성주체 %s)",
		*InUnitRowName.ToString(), TeamIndex, *InSpawnerActor->GetName());

	return Unit;
}
