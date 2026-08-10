#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SFUnitSpawnLibrary.generated.h"

class AUnitBase;

/**
 * 유닛 생성 공통 로직.
 *
 * 스포너(적)와 생산 건물(아군)이 같은 방식으로 유닛을 만들어야 하는데,
 * 트리거 시점만 다르고 생성 절차는 동일하다. 그 절차를 여기 모았다.
 *
 * 처리하는 것
 *   - NavMesh 위의 안전한 위치 탐색 (벽 안이나 NavMesh 밖에 끼는 것 방지)
 *   - 소유 액터의 팀 번호 계승
 *   - Deferred 스폰으로 BeginPlay 전에 팀·능력치 주입
 *   - EDataTableType::Unit 에서 유닛 클래스와 스탯 조회
 */
UCLASS()
class STRIKEFORGE_API USFUnitSpawnLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 데이터테이블 행 이름으로 유닛을 생성한다.
	 *
	 * @param InSpawnerActor   생성 주체(건물·스포너). 위치와 팀 번호의 기준이 된다.
	 * @param InUnitRowName    EDataTableType::Unit 의 RowName
	 * @param InOverrideClass  지정하면 테이블의 UnitClass 보다 우선한다
	 * @return 생성된 유닛. 실패하면 nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "SF|Unit",
		meta = (AdvancedDisplay = "InSpawnDistance,InScatterRadius,InOverrideClass"))
	static AUnitBase* SpawnUnitFromRow(
		AActor* InSpawnerActor,
		FName InUnitRowName,
		float InSpawnDistance = 300.f,
		float InScatterRadius = 200.f,
		TSubclassOf<AUnitBase> InOverrideClass = nullptr);

	/**
	 * 액터의 팀 번호를 알아낸다.
	 * ASFRootBase 파생(건물)이면 그쪽 값을, 아니면 CombatTargetComponent 를 본다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Unit")
	static int32 ResolveTeamIndex(const AActor* InActor);

	/**
	 * NavMesh 위의 생성 가능한 지점을 찾는다.
	 * 실패해도 OutLocation 에는 원점이 들어가므로 호출부는 그대로 진행해도 된다.
	 */
	static bool FindSpawnLocation(
		const UObject* InWorldContext,
		const FVector& InOrigin,
		float InScatterRadius,
		FVector& OutLocation);
};
