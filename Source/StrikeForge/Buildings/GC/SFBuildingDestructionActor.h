#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Datas/Building/BuildingAppearance.h"
#include "Physics/Experimental/ChaosEventType.h"
#include "SFBuildingDestructionActor.generated.h"

class UGeometryCollection;
class UGeometryCollectionComponent;
class URadialFalloff;
class FGeometryCollectionDynamicStateFacade;
struct FSFPieceState;

class UNiagaraComponent;
class UNiagaraSystem;

/** 아직 건물에 붙어 있는 조각의 후보 위치. */
struct FSFPieceSpot
{
	FVector LocalLocation = FVector::ZeroVector;
	int32 PieceIndex = INDEX_NONE;
	// GC 조각 Attach에 사용
	FName BoneName = NAME_None;
	FVector BoneLocalLocation = FVector::ZeroVector;
	
	/** rest 상태 조각 스케일. 축소 판정의 분모. */
	float RestScale = 1.f;
	/** 바운드 경계 대비 수평 거리 비율. 레벨별 밴드 판정에 쓴다. */
	float RadialRatio = 0.f;
};

USTRUCT()
struct FSFSurfaceFire
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> Component = nullptr;

	int32 PieceIndex = INDEX_NONE;

	/** 건물에서 떨어져 나간 불인지 */
	bool bDetached = false;

	/** 건물 위 불들의 간격 판정용 */
	FVector LocalLocation = FVector::ZeroVector;
	
	float RestScale = 1.f;
	
	/** 이 불에 마지막으로 넘긴 User.ClusterLevel. */
	int32 ClusterLevel = 1;

	int32 BornFireStep = 1;
};

UCLASS()
class STRIKEFORGE_API ASFBuildingDestructionActor : public AActor
{
	GENERATED_BODY()

public:
	ASFBuildingDestructionActor();

	/** 건물 완성 시 GC와 Root Proxy 및 Chaos 시뮬레이션을 준비한다. */
	UFUNCTION(BlueprintCallable, Category = "SF|Building|Destruction")
	bool InitializeDestruction(const UGeometryCollection* InCollection);

	/** 동일하거나 이전 단계는 다시 적용하지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "SF|Building|Destruction")
	void ApplyDamageStage(int32 StageIndex, const FBuildingDestruction& StageData);

	/** 체력 0에서 호출한다. DebrisLifetime 후 Actor를 제거한다. */
	UFUNCTION(BlueprintCallable, Category = "SF|Building|Destruction")
	void ApplyFinalDestruction(int32 FinalStageIndex, const FBuildingDestruction& StageData);
	/** 불꽃 구간에 진입할 때 호출한다. 구간 번호는 Niagara 표현 단계이고 FireCount는 목표 개수다. */
	UFUNCTION(BlueprintCallable, Category = "SF|Building|Destruction")
	void SetSurfaceFireStep(int32 StepIndex, int32 FireCount);
	/** 목표 불꽃 개수. 구간 데이터에서 받는다. */
	UPROPERTY(Transient)
	int32 SurfaceFireTarget = 0;
	/** 기존 불꽃의 Niagara 표현 단계를 계산하는 불꽃 구간 번호. */
	UPROPERTY(Transient)
	int32 SurfaceFireStep = INDEX_NONE;
	
	UFUNCTION(BlueprintPure, Category = "SF|Building|Destruction")
	FORCEINLINE UGeometryCollectionComponent* GetGeometryCollectionComponent() const { return GeometryCollectionComponent; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// ---- 파괴 단계 ----
	void ApplyStageSettings(const FBuildingDestruction& StageData, int32 TargetClusterDepth);
	void ApplyNextStrainHit();
	void SchedulePendingImpulse();
	void ApplyPendingImpulse();

	UFUNCTION()
	void HandleChaosBreak(const FChaosBreakEvent& BreakEvent);

	// ---- 불꽃 유지 ----
	void StartSurfaceFireMaintenance();
	void UpdateSurfaceFires();
	static void RemoveSurfaceFire(FSFSurfaceFire& Fire);
	bool IsPieceShrunk(const FSFPieceState& Pieces, int32 PieceIndex, float RestScale) const;
	bool IsPieceOnBuilding(const FSFPieceState& Pieces, int32 PieceIndex) const;

	int32 AddSurfaceFires(int32 DesiredCount, const FSFPieceState& Pieces);
	void BuildPieceSpotTable();

	// ---- 공용 ----
	float GetComponentUniformScale() const;

private:
	// ---- 컴포넌트 ----
	UPROPERTY(VisibleAnywhere, Category = "Destruction")
	TObjectPtr<UGeometryCollectionComponent> GeometryCollectionComponent;

	UPROPERTY(VisibleAnywhere, Category = "Destruction")
	TObjectPtr<URadialFalloff> StrainField;

	// ---- 튜닝: Destruction ----
	UPROPERTY(EditDefaultsOnly, Category = "Destruction")
	float DebrisLifetime = 4.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Destruction",
		meta = (ClampMin = "0.01", ClampMax = "0.1"))
	float StrainHitInterval = 0.03f;

	// ---- 튜닝: BurnTest ----
	//  에셋
	UPROPERTY(EditDefaultsOnly, Category="BurnTest")
	TObjectPtr<UNiagaraSystem> SurfaceFireEffect;

	// 루프 
	UPROPERTY(EditDefaultsOnly, Category="BurnTest|Loop", meta=(ClampMin="0.1"))
	float SurfaceFireUpdateInterval = 0.5f;
	UPROPERTY(EditDefaultsOnly, Category="BurnTest|Loop", meta=(ClampMin="0.0", ClampMax="1.0"))
	float SurfaceFireShrinkThreshold = 1.f;
	// 제거 
	UPROPERTY(EditDefaultsOnly, Category="BurnTest|Delete", meta=(ClampMin="0.0", ClampMax="1.0"))
	float SurfaceFireMinHeightRatio = 0.f;

	// 배치 범위
	UPROPERTY(EditDefaultsOnly, Category="BurnTest|SpawnBound",	meta=(ClampMin="0.0", ClampMax="1.0"))
	float SurfaceFireMinDistanceRatio = 0.2f;
	UPROPERTY(EditDefaultsOnly, Category="BurnTest|SpawnBound",	meta=(ClampMin="0.0", ClampMax="1.0"))
	float SurfaceFireMinRadialRatio = 0.f;
	UPROPERTY(EditDefaultsOnly, Category="BurnTest|SpawnBound",	meta=(ClampMin="0.0", ClampMax="1.0"))
	float SurfaceFireMaxRadialRatio = 0.85f;
	UPROPERTY(
	EditDefaultsOnly,
	Category="BurnTest|SpawnBound",
	meta=(ClampMin="0.0", ClampMax="0.5"))
	float SurfaceFireRadialRatioStepPerLevel = 0.15f;
	/** 조각 바운드가 레벨별 클러스터 바운드 가장자리에서 어느 정도 안쪽까지 있으면 표면 조각으로 인정할지 결정한다. */
	UPROPERTY(EditDefaultsOnly, Category="BurnTest|SpawnBound",	meta=(ClampMin="0.0", ClampMax="0.5"))
	float SurfaceFireClusterSurfaceTolerance = 0.08f;
	
	// ---- 런타임 상태: 파괴 단계 ----
	FBox CachedRestBounds = FBox(ForceInit);

	UPROPERTY(Transient) int32 AppliedStageIndex = INDEX_NONE;
	UPROPERTY(Transient) int32 AppliedClusterDepth = 0;
	UPROPERTY(Transient) int32 PendingTargetClusterDepth = 0;

	UPROPERTY(Transient)
	bool bInitialized = false;
	UPROPERTY(Transient)
	bool bFinalDestructionApplied = false;
	
	FVector PendingImpulseOrigin = FVector::ZeroVector;
	float PendingImpulseRadius = 0.f;
	float PendingImpulseStrength = 0.f;
	float PendingImpulseDelay = 0.f;

	// UPROPERTY(Transient) bool bInitialized = false;
	// UPROPERTY(Transient) bool bFinalDestructionApplied = false;

	// ---- 런타임 상태: 불꽃 ----
	UPROPERTY(Transient)
	TArray<FSFSurfaceFire> SurfaceFires;

	/** 배열 인덱스가 GC 계층 Level이다. 각 배열에는 해당 Level에서 표면으로 판단된 리프 조각 후보가 들어간다. */
	TArray<TArray<FSFPieceSpot>> CachedSurfaceSpotsByLevel;
	bool bPieceSpotsBuilt = false;
	uint64 LastBreakWakeFrame = MAX_uint64;

	// ---- 타이머 ----
	FTimerHandle StrainHitTimerHandle;
	FTimerHandle ImpulseTimerHandle;
	FTimerHandle SurfaceFireTimerHandle;
};