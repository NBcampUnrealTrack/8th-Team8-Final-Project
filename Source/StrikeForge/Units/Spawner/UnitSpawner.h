#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
//#include "Interfaces/Units/Targetable.h"
#include "UnitSpawner.generated.h"

class AUnitBase;
class ALanePath;

/**
 * 유닛 자동 생성기 (프로토타입 검증용).
 *
 * 최종 설계에서는 생산 건물이 유닛을 만들지만, 전투 AI 만 먼저 검증하려면
 * 건설·자원 시스템 없이 돌릴 수단이 필요하다.
 * 이 액터를 레벨에 놓고 TeamIndex 만 바꾸면 아군/적군 양쪽 다 테스트할 수 있다.
 *
 * 검증이 끝나면 SpawnOne() 내용을 생산 건물 쪽으로 옮기면 된다.
 */
UCLASS()
class STRIKEFORGE_API AUnitSpawner : public AActor
{
	GENERATED_BODY()

public:
	AUnitSpawner();

	UFUNCTION(BlueprintCallable, Category = "SF|Spawn")
	AUnitBase* SpawnOne();

	UFUNCTION(BlueprintCallable, Category = "SF|Spawn")
	void SetSpawningEnabled(bool bInEnabled);

	/**
	 * 스포너를 깨운다. 이미 깨어 있으면 아무 일도 하지 않는다.
	 *
	 * bSpawningEnabled 와는 다른 축이다.
	 *   bSpawningEnabled : 스폰을 켜고 끄는 런타임 토글
	 *   bActivated       : "아직 등장할 때가 아니다" 라는 시나리오 게이트
	 * 둘 다 켜져야 실제로 스폰한다. 두 축을 하나로 합치면 튜토리얼이 스포너를
	 * 깨운 뒤에 다른 코드가 잠시 껐다 켤 때 게이트가 통째로 풀려버린다.
	 */
	UFUNCTION(BlueprintCallable, Category = "SF|Spawn")
	void ActivateSpawner();

	UFUNCTION(BlueprintCallable, Category = "SF|Spawn")
	void DeactivateSpawner();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Spawn")
	bool IsSpawnerActivated() const;

	/** 현재 살아있는 자식 유닛 수 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Spawn")
	int32 GetAliveCount() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "SF|Spawn")
	void OnUnitSpawned(AUnitBase* InUnit);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void HandleSpawnCycle();

	/** NavMesh 위의 유효한 스폰 위치를 찾는다 */
	bool FindSpawnLocation(FVector& OutLocation) const;

	/**
	 * 목표 달성 신호를 받는다.
	 *
	 * 델리게이트에 바인드하려면 UFUNCTION 이어야 한다.
	 */
	UFUNCTION()
	void HandleObjectiveRecorded(FGameplayTag InObjectiveTag);

	/** BeginPlay 시점에 깨어 있어야 하는지 판정한다 */
	void ResolveInitialActivation();

	void BindActivationSignal();
	void UnbindActivationSignal();

	void StartSpawnTimer(float InFirstDelay);
	void StopSpawnTimer();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SpawnPoint;

private:
#pragma region Spawn Target

	/** 스폰할 유닛 BP. AUnitBase 를 상속한 클래스여야 한다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn", meta = (AllowPrivateAccess = true))
	TSubclassOf<AUnitBase> UnitClass;

	/** 데이터테이블 RowName. 비워두면 BP 기본값을 그대로 쓴다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn", meta = (AllowPrivateAccess = true))
	FName UnitRowName;

	/** 이 스포너가 만드는 유닛의 팀 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn", meta = (AllowPrivateAccess = true))
	int32 TeamIndex = 1;
	//int32 TeamIndex = Targetable::EnemyTeam;
#pragma endregion

#pragma region Timing

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (AllowPrivateAccess = true, ClampMin = "0.1"))
	float SpawnInterval = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (AllowPrivateAccess = true, ClampMin = "0.0"))
	float InitialDelay = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (AllowPrivateAccess = true, ClampMin = "1"))
	int32 SpawnCountPerCycle = 1;

#pragma endregion

#pragma region Limit

	/** 동시 생존 상한. 0 이면 무제한 — 테스트 중에는 반드시 설정할 것 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limit", meta = (AllowPrivateAccess = true, ClampMin = "0"))
	int32 MaxAlive = 10;

	/** 누적 생성 상한. 0 이면 무한 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limit", meta = (AllowPrivateAccess = true, ClampMin = "0"))
	int32 MaxTotal = 0;

	/** 스폰 지점 주변 산포 반경(cm). 같은 자리에 겹쳐 나오는 것을 막는다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limit", meta = (AllowPrivateAccess = true, ClampMin = "0.0"))
	float SpawnScatterRadius = 300.f;

#pragma endregion

#pragma region Activation

	/**
	 * 레벨에 놓자마자 스폰을 시작할지.
	 *
	 * 기본이 true 인 이유는 기존 맵 때문이다. 이 값을 false 로 두면 지금까지
	 * 잘 돌던 전투 테스트 맵의 스포너가 전부 조용해진다. 게이트가 필요한
	 * 스포너만 끄는 편이 안전하다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Activation", meta = (AllowPrivateAccess = true))
	uint8 bStartActive : 1;

	/**
	 * 이 목표가 달성되면 깨어난다. bStartActive 가 false 일 때만 의미가 있다.
	 *
	 * 계층으로 매칭하므로 Tutorial.Build 를 걸어두면 그 아래 아무 목표에나
	 * 반응한다. 특정 건물 하나만 원하면 끝까지 적는다.
	 *
	 * 비워두면 신호로는 절대 깨지 않는다 — ActivateSpawner() 를 직접 부르는
	 * 경우를 위한 상태다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Activation", meta = (AllowPrivateAccess = true, EditCondition = "!bStartActive"))
	FGameplayTag ActivationObjectiveTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Activation", meta = (AllowPrivateAccess = true))
	uint8 bActivated : 1;

#pragma endregion

#pragma region State

	/** 지정하면 생성된 유닛이 이 라인을 따라 전진한다 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = true))
	TObjectPtr<ALanePath> Lane;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = true))
	uint8 bSpawningEnabled : 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = true))
	int32 TotalSpawned = 0;

#pragma endregion

	FTimerHandle SpawnTimerHandle;

	/** 약참조라 유닛이 파괴되면 자동으로 무효가 된다 */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AUnitBase>> SpawnedUnits;
};
