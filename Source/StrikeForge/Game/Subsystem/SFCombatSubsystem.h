#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Interfaces/Units/Targetable.h"
#include "SFCombatSubsystem.generated.h"

/**
 * 전투 대상 레지스트리.
 *
 * GetAllActorsOfClass 는 액터 수에 비례해 느려지고 클래스 캐스팅을 강제하므로
 * 쓰지 않는다. ITargetable 을 구현한 액터가 BeginPlay 에서 스스로 등록하고
 * EndPlay·사망 시 해제한다.
 *
 * 레벨마다 초기화되어야 하므로 GameInstanceSubsystem 이 아니라
 * WorldSubsystem 이다. (USFDataTableManager 와 수명이 다르다)
 */
UCLASS()
class STRIKEFORGE_API USFCombatSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 어디서든 접근하는 헬퍼. WorldContext 는 보통 this */
	static USFCombatSubsystem* Get(const UObject* WorldContext);

	/** ITargetable 구현 액터만 등록된다. 중복 등록은 무시 */
	UFUNCTION(BlueprintCallable, Category = "SF|Combat")
	void RegisterCombatant(AActor* InActor);

	UFUNCTION(BlueprintCallable, Category = "SF|Combat")
	void UnregisterCombatant(AActor* InActor);

	/**
	 * 가장 가까운 적대 대상 1기.
	 * @param InMaxRange 0 이하면 거리 제한 없음 (핵심 건물 탐색용)
	 */
	UFUNCTION(BlueprintCallable, Category = "SF|Combat")
	AActor* FindNearestHostile(const FVector& InOrigin, int32 InSeekerTeam, ETargetType InTargetType, float InMaxRange) const;

	/** 반경 내 적대 대상 전부 — 타겟 탐색·광역 피해용 */
	UFUNCTION(BlueprintCallable, Category = "SF|Combat")
	void GetHostilesInRadius(const FVector& InOrigin, int32 InSeekerTeam, float InRadius, TArray<AActor*>& OutActors) const;

	/** 팀 내 유효 대상 수 — 웨이브 판정·디버그용 */
	UFUNCTION(BlueprintCallable, Category = "SF|Combat")
	int32 GetCombatantCount(int32 InTeamIndex) const;

	/** 중립(-1)은 누구와도 적대하지 않는다 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Combat")
	static bool IsHostileTeam(int32 InTeamA, int32 InTeamB);

private:
	/** 팀 인덱스별 버킷. 약참조라 파괴된 액터는 자동으로 무효화된다 */
	TMap<int32, TArray<TWeakObjectPtr<AActor>>> TeamBuckets;

	/** 유효 + 타겟 가능 여부를 한 번에 판정 */
	static bool IsUsableTarget(const TWeakObjectPtr<AActor>& InWeak, AActor*& OutActor);
};
