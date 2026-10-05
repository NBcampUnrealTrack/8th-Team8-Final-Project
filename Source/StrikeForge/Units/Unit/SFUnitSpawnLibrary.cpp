#include "SFUnitSpawnLibrary.h"

#include "UnitBase.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "Buildings/SFRootBase.h"
#include "Datas/Unit/UnitStatData.h"
#include "Game/Subsystem/SFCombatSubsystem.h"
#include "Game/Subsystem/SFDataTableManager.h"
#include "Logging/SFLog.h"

#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Components/CapsuleComponent.h"

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

bool USFUnitSpawnLibrary::IsBuildableLocation(
	const UObject* InWorldContext,
	const FVector& InLocation,
	float InFootprintRadius)
{
	const USFCombatSubsystem* Combat = USFCombatSubsystem::Get(InWorldContext);

	// 서브시스템이 없거나(에디터 초기 세팅 중) 라인이 배치되지 않았으면
	// 제한하지 않는다. 코리도를 모르는 채로 막으면 어디에도 못 짓는다.
	if (!IsValid(Combat) || !Combat->HasAnyLane()) return true;

	// 코리도 밖으로 풋프린트만큼 완전히 벗어나야 허용한다.
	// 여유 거리는 따로 두지 않는다 — 길만 비어 있으면 바로 옆까지 지을 수 있다.
	return Combat->GetDistanceOutsideLane(InLocation) > FMath::Max(0.f, InFootprintRadius);
}

// ─────────────────────────────────────────────────────────
// 부착 정보 조회
//
// 값을 꺼내는 것까지만 한다. 붙이는 쪽이 스켈레탈 메시일 수도, 초상화용
// 사본일 수도, 나이아가라일 수도 있어서 이 라이브러리가 알 수 없다.
// ─────────────────────────────────────────────────────────
bool USFUnitSpawnLibrary::GetUnitAttachTransform(
	const UObject* InWorldContext,
	FName InUnitRowName,
	FName& OutSocketName,
	FTransform& OutRelativeTransform
)
{
	// 실패해도 출력값은 항상 채워 둔다.
	// 호출부가 반환값을 안 보고 그대로 써도 루트에 원본 크기로 붙을 뿐이다.
	OutSocketName = NAME_None;
	OutRelativeTransform = FTransform::Identity;

	if (InUnitRowName.IsNone())
	{
		return false;
	}

	const UWorld* World = GEngine != nullptr
		? GEngine->GetWorldFromContextObject(InWorldContext, EGetWorldErrorMode::ReturnNull)
		: nullptr;

	if (!IsValid(World))
	{
		return false;
	}

	UGameInstance* GameInstance = World->GetGameInstance();

	if (!IsValid(GameInstance))
	{
		return false;
	}

	USFDataTableManager* TableManager = GameInstance->GetSubsystem<USFDataTableManager>();

	if (!IsValid(TableManager))
	{
		return false;
	}

	const FUnitStatData* StatRow = TableManager->GetDataRow<FUnitStatData>(
		EDataTableType::Unit,
		InUnitRowName,
		TEXT("SFUnitSpawnLibrary::GetUnitAttachTransform")
	);

	if (StatRow == nullptr)
	{
		SF_LOG(Warning, "[UnitSpawn] 부착 정보를 찾지 못했습니다 | Row: %s", *InUnitRowName.ToString());
		return false;
	}

	OutSocketName = StatRow->WeaponAttach.SocketName;
	OutRelativeTransform = StatRow->WeaponAttach.GetRelativeTransform();

	return true;
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
	const AUnitBase* Defaults = SpawnClass->GetDefaultObject<AUnitBase>();
	const UCapsuleComponent* Capsule = Defaults->GetCapsuleComponent();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const FBox Bounds = InSpawnerActor->GetComponentsBoundingBox(false);
	const float SearchRadius = FMath::Max(InSpawnDistance, Bounds.IsValid ? Bounds.GetExtent().Size2D() + Radius + 100.f : 400.f);
	bool bFoundLocation = false;
	for (int32 Attempt = 0; Attempt < 49; ++Attempt)
	{
		const float Angle = (Attempt % 16) * 2.f * PI / 16.f;
		const FVector Candidate = Attempt == 0 ? Origin : InSpawnerActor->GetActorLocation() +
			FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * (SearchRadius + ((Attempt - 1) / 16) * 150.f);
		if (!FindSpawnLocation(InSpawnerActor, Candidate, Attempt == 0 ? InScatterRadius : 0.f, SpawnLocation)) continue;
		SpawnLocation.Z += HalfHeight + 2.f;
		// Check both buildings and units even when unit-to-unit collision is ignored.
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_WorldStatic);
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		Objects.AddObjectTypesToQuery(ECC_Pawn);
		if (!World->OverlapAnyTestByObjectType(SpawnLocation, FQuat::Identity, Objects,
			FCollisionShape::MakeCapsule(Radius, HalfHeight)))
		{
			bFoundLocation = true;
			break;
		}
	}
	if (!bFoundLocation) return nullptr;

	const FTransform SpawnTM(InSpawnerActor->GetActorRotation(), SpawnLocation);

	// ── 생성 ────────────────────────────────────────────
	// Deferred 스폰: BeginPlay 전에 팀과 능력치를 심어야
	// 전투 레지스트리에 올바른 팀으로 등록된다.
	AUnitBase* Unit = World->SpawnActorDeferred<AUnitBase>(
		SpawnClass,
		SpawnTM,
		InSpawnerActor,
		nullptr,
		ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding);

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
