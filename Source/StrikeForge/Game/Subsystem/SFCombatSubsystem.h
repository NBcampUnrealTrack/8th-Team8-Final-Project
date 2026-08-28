#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Interfaces/Units/Targetable.h"
#include "SFCombatSubsystem.generated.h"

class UCombatTargetComponent;
class ALanePath;

/**
 * 대상 하나를 둘러싸는 공격 슬롯 링.
 *
 * 여러 유닛이 한 대상에게 몰릴 때 전부 같은 한 점을 향하면 서로 밀친다.
 * 대상 주위의 "각도" 를 미리 나눠 갖게 해서 각자 다른 방향에서 접근하게 한다.
 *
 * 링이 배분하는 것은 각도뿐이다. 실제로 서는 거리는 공격자마다 다르므로
 * (근접은 붙고 원거리는 멀리 선다) 위치를 물을 때 자기 접근 거리를 넘긴다.
 *
 * UPROPERTY 를 붙이지 않는다. 약참조만 담아 GC 앵커가 필요 없고,
 * TMap 키의 TWeakObjectPtr 리플렉션 처리가 엔진 버전을 타기 때문이다.
 */
struct FSFAttackSlotRing
{
	/** 슬롯 점유자. 배열 인덱스가 곧 각도다. 빈 칸은 무효 약참조 */
	TArray<TWeakObjectPtr<AActor>> Occupants;

	/** 대상의 수평 바운딩 반경. 슬롯 개수를 셀 때 쓴다 */
	float TargetRadius = 0.f;

	/**
	 * 대상 바운딩 박스의 수평 반크기.
	 *
	 * 슬롯을 "중심에서 최대 반경" 이 아니라 "박스 표면" 기준으로 놓기 위해 필요하다.
	 * 긴 직사각형 건물은 두 축의 크기가 크게 달라서, 최대 반경으로 재면
	 * 짧은 변 쪽 슬롯이 사거리 밖에 놓인다.
	 */
	FVector2D BoxExtent = FVector2D::ZeroVector;
};

/**
 * 한 팀의 전투 대상 목록.
 *
 * TMap<int32, TArray<...>> 형태로는 UPROPERTY 를 붙일 수 없다(중첩 컨테이너 미지원).
 * 구조체로 한 겹 풀어 리플렉션에 노출한다.
 */
USTRUCT()
struct FSFTargetBucket
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UCombatTargetComponent>> Targets;
};

/**
 * 전투 대상 레지스트리.
 *
 * 등록 단위는 액터가 아니라 UCombatTargetComponent 다.
 * 액터 클래스를 수정하지 않고 BP 에서 컴포넌트만 붙이면
 * 전투 시스템에 편입되도록 하기 위한 구조다.
 *
 * 외부 API 는 AActor* 를 주고받는다. 이동·거리 계산이 액터 기준이기 때문이다.
 *
 * 레벨마다 초기화되어야 하므로 GameInstanceSubsystem 이 아니라 WorldSubsystem 이다.
 */
UCLASS()
class STRIKEFORGE_API USFCombatSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 어디서든 접근하는 헬퍼. WorldContext 는 보통 this */
	static USFCombatSubsystem* Get(const UObject* WorldContext);

	/** 액터에 붙은 전투 대상 컴포넌트를 찾는다. 없으면 nullptr */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Combat")
	static UCombatTargetComponent* FindTargetComponent(const AActor* InActor);

	// ── 등록 ────────────────────────────────────────────
	void RegisterTarget(UCombatTargetComponent* InTarget);
	void UnregisterTarget(UCombatTargetComponent* InTarget);

	// ── 진격로 코리도 ───────────────────────────────────
	//
	// ALanePath 가 BeginPlay 에서 스스로 등록한다. 전투 대상과 같은 방식이다.
	//
	// 코리도는 "전투가 벌어져야 하는 길" 을 정의하고, 두 곳에서 쓰인다.
	//   AUnitBase                             자동 전투가 길 밖으로 흘러나가지 않게
	//   USFUnitSpawnLibrary::IsBuildableLocation  길 위에 건물을 못 세우게
	//
	// 등록된 라인이 하나도 없으면 두 제한 모두 꺼진다. 호출부는 HasAnyLane 으로
	// 먼저 확인한다 — 코리도를 모르는 채로 유닛을 묶으면 전부 그 자리에 굳고,
	// 건물은 어디에도 못 짓게 된다.

	void RegisterLane(ALanePath* InLane);
	void UnregisterLane(ALanePath* InLane);

	/** 레벨에 진격로가 하나라도 배치돼 있는가. 코리도 제한의 on/off 스위치다 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Lane")
	bool HasAnyLane() const;

	/**
	 * 이 지점이 어느 코리도 밖으로 얼마나 벗어났는가(cm). 안이면 0.
	 *
	 * 라인이 여럿이면 "가장 가까운 라인" 기준이다. 갈래길에서는 어느 한
	 * 라인 안이기만 하면 안으로 친다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Lane")
	float GetDistanceOutsideLane(const FVector& InLocation) const;

	/** 라인이 없으면 true(제한 없음)를 돌려준다. 호출부가 따로 분기하지 않아도 되게 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Lane")
	bool IsInsideAnyLane(const FVector& InLocation) const;

	/**
	 * 이 팀이 쓸 수 있는 라인 중 가장 가까운 것.
	 * 스포너가 SetAssignedLane 을 부르지 않았을 때 유닛이 스스로 찾는 경로다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Lane")
	ALanePath* FindNearestLane(const FVector& InLocation, int32 InTeamIndex) const;

	// ── 질의 ────────────────────────────────────────────
	/**
	 * 가장 가까운 적대 대상 1기.
	 * @param InMaxRange 0 이하면 거리 제한 없음
	 */
	UFUNCTION(BlueprintCallable, Category = "SF|Combat")
	AActor* FindNearestHostile(const FVector& InOrigin, int32 InSeekerTeam, ETargetType InTargetType, float InMaxRange) const;

	/** 반경 내 적대 대상 전부 — 타겟 탐색·광역 피해용 */
	UFUNCTION(BlueprintCallable, Category = "SF|Combat")
	void GetHostilesInRadius(const FVector& InOrigin, int32 InSeekerTeam, float InRadius, TArray<AActor*>& OutActors) const;

	/**
	 * 반경 내 같은 팀 전투 유닛 전부(자기 자신 제외) — 근접 조향의 분리력 계산용.
	 *
	 * 건물은 뺀다. 크고 고정된 대상이라 분리 대상에 넣으면 그 근처에서
	 * 밀려나 버려, 오히려 건물에 못 붙는 원인이 된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "SF|Combat")
	void GetNearbyAlliesForSeparation(const AActor* InSelf, int32 InTeamIndex, float InRadius, TArray<AActor*>& OutActors) const;

	/** 분류 무관, 거리 제한 없이 가장 가까운 적 — 진격 목표 탐색용 */
	UFUNCTION(BlueprintCallable, Category = "SF|Combat")
	AActor* FindNearestHostileAny(const FVector& InOrigin, int32 InSeekerTeam) const;

	/**
	 * 특정 팀의 생존 대상 전부.
	 * 드래그 박스 선택처럼 "우리 편 전체" 를 훑어야 할 때 쓴다.
	 * GetAllActorsOfClass 보다 훨씬 빠르다.
	 */
	UFUNCTION(BlueprintCallable, Category = "SF|Combat")
	void GetTeamTargets(int32 InTeamIndex, TArray<AActor*>& OutActors) const;

	/** 팀 내 생존 대상 수 — 웨이브 판정·디버그용 */
	UFUNCTION(BlueprintCallable, Category = "SF|Combat")
	int32 GetTargetCount(int32 InTeamIndex) const;

	/** 중립(-1)은 누구와도 적대하지 않는다 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Combat")
	static bool IsHostileTeam(int32 InTeamA, int32 InTeamB);

	// ── 공격 슬롯 ───────────────────────────────────────
	//
	// 대상 주위 각도를 나눠 갖게 해서 포위 대형이 저절로 생기게 한다.
	// 슬롯은 "접근 경로" 만 정한다. 사거리에 들어오면 그 자리에서 때린다 —
	// 자리를 맞추겠다고 더 움직이면 그게 곧 공격 안 하고 이동만 하는 유닛이 된다.

	/**
	 * 각도상 가장 가까운 빈 슬롯을 잡는다.
	 *
	 * 이미 이 대상에 자리를 갖고 있으면 그것을 그대로 돌려준다.
	 * 매번 다시 배정하면 대상이 조금 움직일 때마다 자리가 바뀌어
	 * 유닛이 링 위를 빙빙 돌기만 한다.
	 *
	 * @return 슬롯 인덱스. 링이 가득 찼으면 INDEX_NONE
	 */
	int32 ClaimAttackSlot(AActor* InTarget, AActor* InAttacker, float InApproachDistance);

	/** 슬롯을 반납한다. 타겟 변경·사망 시 반드시 부른다 */
	void ReleaseAttackSlot(AActor* InTarget, AActor* InAttacker);

	/**
	 * 슬롯의 현재 월드 위치. 대상이 움직이면 따라 움직인다.
	 *
	 * @param InApproachDistance 대상 표면에서 떨어져 설 거리. 공격자가 자기 사거리로 정한다
	 */
	bool GetAttackSlotLocation(const AActor* InTarget, int32 InSlotIndex, float InApproachDistance, FVector& OutLocation) const;

	/** 링이 가득 찼는가. 타겟 점수에 혼잡도 페널티를 줄 때 쓴다 */
	bool IsSlotRingFull(const AActor* InTarget) const;

	/** 이 대상을 노리는 공격자 수 */
	int32 GetAttackerCount(const AActor* InTarget) const;

	/**
	 * 이 대상이 실제로 받아줄 수 있는 공격자 수.
	 *
	 * 슬롯 개수와 다르다. 링에 9칸이 있어도 실제로 도달 가능한 자리는 훨씬 적다.
	 * 뒤에서 오는 유닛은 앞줄 캡슐을 통과할 수 없기 때문이다.
	 * sf.MaxAttackersPerTarget 로 상한을 건다.
	 */
	int32 GetSlotCapacity(const AActor* InTarget) const;

	/** 팀별 등록 현황을 로그로 출력한다. 콘솔 명령 sf.DumpCombatTargets 에서 호출 */
	void DumpTargets() const;

private:
	/** 팀 인덱스별 버킷. 약참조라 파괴된 컴포넌트는 자동으로 무효화된다 */
	UPROPERTY(Transient)
	TMap<int32, FSFTargetBucket> TeamBuckets;

	/**
	 * 배치된 진격로. 약참조만 담으므로 리플렉션에 올리지 않는다.
	 *
	 * 레벨에 놓인 정적 액터라 개수가 한 자릿수다. 매번 훑어도 부담이 없어
	 * 공간 분할 없이 선형 탐색으로 둔다.
	 */
	TArray<TWeakObjectPtr<ALanePath>> Lanes;

	/** "라인이 없어 코리도 제한이 꺼졌다" 를 한 번만 알리기 위한 플래그 */
	mutable bool bNoLaneReported = false;

	/** 유효 + 생존 + 소유 액터 유효 여부를 한 번에 판정 */
	static bool ResolveTarget(const TWeakObjectPtr<UCombatTargetComponent>& InWeak, AActor*& OutActor);

	// ── 공격 슬롯 내부 ──────────────────────────────────

	/** 대상별 공격 슬롯 링. 약참조만 담으므로 리플렉션에 올리지 않는다 */
	TMap<TWeakObjectPtr<AActor>, FSFAttackSlotRing> SlotRings;

	// 슬롯 사이 최소 간격은 콘솔 변수 sf.SlotSpacing 이 갖는다(cpp 상단).
	// PIE 에서 밀집도를 바로 바꿔볼 수 있어야 해서 멤버로 두지 않는다.

	/** 대상 하나에 붙일 수 있는 최대 슬롯 수. 거대 건물에서 링이 폭주하는 것을 막는다 */
	int32 MaxSlotsPerTarget = 32;

	/**
	 * 대상 하나에 두는 최소 슬롯 수.
	 *
	 * 근접 사거리에서는 링이 작아 6칸 남짓밖에 안 나온다. 그게 정상이다 —
	 * 한 마리를 둘러쌀 수 있는 유닛 수에는 물리적 한계가 있다.
	 * 억지로 늘리면 슬롯끼리 겹쳐 서로 밀치게 된다.
	 */
	int32 MinSlotsPerTarget = 3;

	/**
	 * 링을 찾거나 만든다. 사라진 점유자도 이 기회에 비운다.
	 * @param InApproachDistance 링 크기를 정한다. 처음 자리를 잡는 유닛의 값이 기준이 된다
	 */
	FSFAttackSlotRing* FindOrCreateRing(AActor* InTarget, float InApproachDistance);

	/** 조회 전용. TMap 키가 비상수 약참조라 const_cast 를 여기 한 곳에 가둔다 */
	const FSFAttackSlotRing* FindRing(const AActor* InTarget) const;

	/** 링 반지름에 몇 칸이 겹치지 않고 들어가는지 계산한다 */
	int32 ComputeSlotCount(float InRingRadius) const;

	/** 대상의 수평 바운딩 크기를 잰다 */
	static void MeasureTargetBounds(const AActor* InTarget, float& OutRadius, FVector2D& OutExtent);
};
