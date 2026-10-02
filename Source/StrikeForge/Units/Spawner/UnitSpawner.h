#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
//#include "Interfaces/Units/Targetable.h"
#include "UnitSpawner.generated.h"

class AUnitBase;
class ALanePath;
class UCombatTargetComponent;

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

	/** Defence controller owns the schedule; autonomous timers and direct BP calls are gated. */
	void SetWaveControlled(bool bInControlled);
	AUnitBase* SpawnWaveUnit(FName InRowName, TSubclassOf<AUnitBase> InClass);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Defence")
	bool bWaveControlled = false;

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

	/**
	 * 이 스포너를 소유한 건물. 아직 연결되지 않았으면 nullptr.
	 *
	 * 건물은 앵커에서 레벨 시작 직후에 생성되므로 BeginPlay 시점에는 아직 없다.
	 * 그래서 연결은 타이머로 미뤄서 한다.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Spawn")
	AActor* GetOwnerBuilding() const;

	/** 앵커에서 생성한 실제 생산 건물을 스포너에 직접 연결한다. */
	bool AssignOwnerBuilding(AActor* InBuilding);

	/** 소유 건물이 파괴돼 영구 정지한 상태인가 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Spawn")
	bool IsShutDownByOwnerBuilding() const;

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

	/**
	 * 소유 건물을 찾아 파괴 신호에 연결한다.
	 *
	 * ① 어태치 부모(건물에 직접 붙여둔 배치)
	 * ② 같은 팀 건물 중 반경 안 최근접 (앵커에 붙여둔 배치 — 건물이 자기 위에 선다)
	 * 찾지 못하면 타임아웃까지 재시도한다.
	 */
	void ResolveOwnerBuilding();

	void StartOwnerBuildingResolveTimer();
	void StopOwnerBuildingResolveTimer();

	void BindOwnerBuilding(UCombatTargetComponent* InTarget);
	void UnbindOwnerBuilding();

	/** 델리게이트에 바인드하려면 UFUNCTION 이어야 한다 */
	UFUNCTION()
	void HandleOwnerBuildingDestroyed(UCombatTargetComponent* InTarget);

	UFUNCTION()
	void HandleOwnerActorDestroyed(AActor* InDestroyedActor);

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

	/** Keep the actor at its factory anchor for ownership, but spawn outside its footprint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn", meta = (AllowPrivateAccess = true))
	FVector SpawnOffset = FVector::ZeroVector;

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

#pragma region Owner Building

	/**
	 * 소유 건물이 파괴되면 스폰을 영구 정지할지.
	 *
	 * 기본이 true 인 이유는 이게 원하는 동작이기 때문이고, 건물을 못 찾은
	 * 스포너는 예전과 똑같이 계속 돈다(bRequireOwnerBuilding 참고).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Owner Building", meta = (AllowPrivateAccess = true))
	uint8 bStopWhenOwnerBuildingDestroyed : 1;

	/** 어태치 부모가 건물이 아닐 때 같은 팀 건물을 찾을 반경(cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Owner Building",
		meta = (AllowPrivateAccess = true, ClampMin = "0.0", EditCondition = "bStopWhenOwnerBuildingDestroyed"))
	float OwnerBuildingSearchRadius = 800.f;

	/**
	 * 건물을 찾을 때까지 재시도하는 시간(초).
	 *
	 * 건물은 SFGameMode 가 에셋 비동기 로딩을 마친 뒤에 세우므로 스포너보다
	 * 늦게 나타난다. 실측 0.6초 남짓이지만 로딩이 길어지는 경우가 있어 넉넉히 둔다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Owner Building",
		meta = (AllowPrivateAccess = true, ClampMin = "0.0", EditCondition = "bStopWhenOwnerBuildingDestroyed"))
	float OwnerBuildingResolveTimeout = 15.f;

	/**
	 * 건물을 끝내 못 찾았을 때 스폰을 막을지.
	 *
	 * 기본이 false 인 이유는 기존 전투 테스트 맵 때문이다. 건물 없이 놓아둔
	 * 스포너가 이 기능 때문에 통째로 조용해지면 안 된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Owner Building",
		meta = (AllowPrivateAccess = true, EditCondition = "bStopWhenOwnerBuildingDestroyed"))
	uint8 bRequireOwnerBuilding : 1;

	/** 소유 건물이 파괴됐는가. 한 번 서면 다시 내려가지 않는다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Owner Building", meta = (AllowPrivateAccess = true))
	uint8 bOwnerBuildingDestroyed : 1;

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
	bool bExecutingWaveSpawn = false;

	FTimerHandle OwnerBuildingResolveTimerHandle;

	/** 재시도 누적 시간(초) */
	float OwnerBuildingResolveElapsed = 0.f;

	/**
	 * 약참조라 건물이 파괴되면 자동으로 무효가 된다.
	 * TWeakObjectPtr 은 리플렉션에 올리지 않는다.
	 */
	TWeakObjectPtr<AActor> OwnerBuilding;
	TWeakObjectPtr<UCombatTargetComponent> OwnerBuildingTarget;

	/** 약참조라 유닛이 파괴되면 자동으로 무효가 된다 */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AUnitBase>> SpawnedUnits;
};
