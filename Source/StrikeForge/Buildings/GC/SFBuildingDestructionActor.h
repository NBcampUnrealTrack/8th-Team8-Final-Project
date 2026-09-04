#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Datas/Building/BuildingAppearance.h"
#include "SFBuildingDestructionActor.generated.h"

class UGeometryCollection;
class UGeometryCollectionComponent;
class URadialFalloff;

class UNiagaraComponent;
class UNiagaraSystem;
struct FChaosBreakEvent;

UCLASS()
class STRIKEFORGE_API ASFBuildingDestructionActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ASFBuildingDestructionActor();
	
    UFUNCTION(BlueprintCallable, Category = "SF|Building|Destruction")
    bool InitializeDestruction(const UGeometryCollection* InCollection);
    /**
     * 동일하거나 이전 단계는 다시 적용하지 않는다.
     */
    UFUNCTION(BlueprintCallable, Category = "SF|Building|Destruction")
	void ApplyDamageStage(int32 StageIndex,	const FBuildingDestruction& StageData);

    /**
     * 체력 0에서 호출한다.
     * 최종 파괴를 적용하고 DebrisLifetime 후 Actor를 제거한다.
     */
    UFUNCTION(BlueprintCallable, Category = "SF|Building|Destruction")
	void ApplyFinalDestruction(int32 FinalStageIndex, const FBuildingDestruction& StageData);
	
    UFUNCTION(BlueprintPure, Category = "SF|Building|Destruction")
    FORCEINLINE UGeometryCollectionComponent* GetGeometryCollectionComponent() const { return GeometryCollectionComponent; }

	
protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ApplyStageSettings(const FBuildingDestruction& StageData, int32 TargetClusterDepth);
	void ApplyNextStrainHit();
	void SchedulePendingImpulse();
    void ApplyPendingImpulse();

	// fire
private:
	/** [BURN-TEST] 조각 위에 붙일 불꽃. BP_DestructionActor에서 지정. */
	UPROPERTY(EditDefaultsOnly, Category = "Destruction|BurnTest",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> SurfaceFireEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Destruction|BurnTest",
		meta = (ClampMin = "1", AllowPrivateAccess = "true"))
	int32 SurfaceFireMinCount = 3;

	UPROPERTY(EditDefaultsOnly, Category = "Destruction|BurnTest",
		meta = (ClampMin = "1", AllowPrivateAccess = "true"))
	int32 SurfaceFireMaxCount = 6;

	/** 바운드 높이의 이 비율보다 낮은 조각은 제외한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Destruction|BurnTest",
		meta = (ClampMin = "0.0", ClampMax = "1.0", AllowPrivateAccess = "true"))
	float SurfaceFireMinHeightRatio = 0.25f;

	/** rest 상태 바운드. 조각이 흩어져도 MinZ 기준이 흔들리지 않게 한다. */
	FBox CachedRestBounds = FBox(ForceInit);
	
	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> SurfaceFires;

	void SpawnSurfaceFires();
	void ClearSurfaceFires();
	
private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Destruction",
        meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UGeometryCollectionComponent> GeometryCollectionComponent;

    /**
     * External Cluster Strain을 만드는 Field Node.
     * 다른 건물에는 영향을 주지 않고 이 GC 컴포넌트에만 적용한다.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Destruction",
        meta = (AllowPrivateAccess = "true"))
    TObjectPtr<URadialFalloff> StrainField;
	
	FVector CachedLocalBoundsCenter = FVector::ZeroVector;
	float CachedLocalBoundsRadius = 1.f;;
	
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Destruction",
        meta = (ClampMin = "0.1", AllowPrivateAccess = "true"))
    float DebrisLifetime = 4.f;

    UPROPERTY(Transient)
    int32 AppliedStageIndex = INDEX_NONE;
	UPROPERTY(Transient)
	int32 AppliedClusterDepth = 0;

	// 현재 요청받은 목표 깊이
	UPROPERTY(Transient)
	int32 PendingTargetClusterDepth = 0;

	FVector PendingImpulseOrigin = FVector::ZeroVector;
	float PendingImpulseRadius = 0.f;
	float PendingImpulseStrength = 0.f;
	float PendingImpulseDelay = 0.f;

	/**
	 * 연속 Strain 사이의 물리 처리 간격.
	 * Level 수와 관계없이 하나의 타이머가 재사용된다.
	 */
	UPROPERTY(EditDefaultsOnly,	BlueprintReadOnly, Category = "Destruction",
		meta = (ClampMin = "0.01",	ClampMax = "0.1", AllowPrivateAccess = "true"))
	float StrainHitInterval = 0.03f;

    UPROPERTY(Transient)
    bool bInitialized = false;

    UPROPERTY(Transient)
    bool bFinalDestructionApplied = false;

	FTimerHandle StrainHitTimerHandle;
	FTimerHandle ImpulseTimerHandle;
};
