#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Interfaces/Units/Targetable.h"
#include "SFCombatSubsystem.generated.h"

class UCombatTargetComponent;

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

	/** 팀별 등록 현황을 로그로 출력한다. 콘솔 명령 sf.DumpCombatTargets 에서 호출 */
	void DumpTargets() const;

private:
	/** 팀 인덱스별 버킷. 약참조라 파괴된 컴포넌트는 자동으로 무효화된다 */
	UPROPERTY(Transient)
	TMap<int32, FSFTargetBucket> TeamBuckets;

	/** 유효 + 생존 + 소유 액터 유효 여부를 한 번에 판정 */
	static bool ResolveTarget(const TWeakObjectPtr<UCombatTargetComponent>& InWeak, AActor*& OutActor);
};
