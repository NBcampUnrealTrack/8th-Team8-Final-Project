#pragma once

#include "CoreMinimal.h"
#include "Datas/Unit/UnitStatData.h"
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
	 * 이 지점에 건물을 세울 수 있는가 — 진격로 코리도 밖이면 세울 수 있다.
	 *
	 * 배치 검증은 건물 쪽 담당이라 이쪽에서 손대지 않는다. 대신 판정만 BP 노드로
	 * 내어준다. 배치 고스트의 틱과 배치 확정 직전 양쪽에서 부르면 된다 —
	 * 확정 직전에만 부르면 플레이어는 클릭할 때까지 안 되는 줄 모른다.
	 *
	 * 레벨에 ALanePath 가 하나도 없으면 언제나 true 다. 코리도를 모르는 채로
	 * 막으면 어디에도 건물을 못 짓게 된다.
	 *
	 * @param InLocation        배치하려는 지점(건물 중심)
	 * @param InFootprintRadius 건물의 수평 반경(cm). 0 이면 중심점만 본다.
	 *                          중심만 검사하면 큰 건물이 길을 반쯤 덮을 수 있다 —
	 *                          유닛끼리는 통과해도 건물은 통과하지 못하므로
	 *                          그대로 진격로가 봉쇄된다.
	 * @return 코리도 밖이면 true
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Build",
		meta = (WorldContext = "InWorldContext", AdvancedDisplay = "InFootprintRadius"))
	static bool IsBuildableLocation(
		const UObject* InWorldContext,
		const FVector& InLocation,
		float InFootprintRadius = 0.f);

	/**
	 * 유닛 행에 적어둔 부착 정보를 읽는다.
	 *
	 * 붙이는 일 자체는 하지 않는다 — 무엇을(메시인지 이펙트인지) 어디에
	 * (유닛 본체인지 초상화용 사본인지) 붙일지는 부르는 쪽 사정이고,
	 * 이 라이브러리는 값만 꺼내 준다.
	 *
	 * 블루프린트에서는 이렇게 쓴다.
	 *   Get Unit Attach Transform -> Attach Component To Component (소켓 이름)
	 *                             -> Set Relative Transform (트랜스폼)
	 *
	 * @param OutSocketName        붙일 소켓 이름. 비어 있으면 루트에 붙인다
	 * @param OutRelativeTransform 소켓 기준 보정값
	 * @return 행을 찾았으면 true. 실패해도 출력값은 기본값으로 채워진다
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Unit",
		meta = (WorldContext = "InWorldContext"))
	static bool GetUnitAttachTransform(
		const UObject* InWorldContext,
		FName InUnitRowName,
		FName& OutSocketName,
		FTransform& OutRelativeTransform);

	/**
	 * NavMesh 위의 생성 가능한 지점을 찾는다.
	 * 실패하면 생성하지 않고, 다른 후보를 찾거나 완료된 생산을 대기시킨다.
	 */
	static bool FindSpawnLocation(
		const UObject* InWorldContext,
		const FVector& InOrigin,
		float InScatterRadius,
		FVector& OutLocation);
};
