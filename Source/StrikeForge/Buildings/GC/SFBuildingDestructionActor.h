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

/** 아직 건물에 붙어 있는 조각의 후보 위치. */
struct FSFPieceSpot
{
	FVector LocalLocation = FVector::ZeroVector;
	int32 PieceIndex = INDEX_NONE;

	// GC 조각 Attach에 사용
	FName BoneName = NAME_None;
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
};

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
	float DebrisLifetime = 2.5f;
	
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

	/**
	 * 불 사이 최소 간격. 건물 바닥 대각선 대비 비율.
	 * 조건을 만족하는 자리가 모자라면 간격을 무시하고 개수를 채운다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Destruction|BurnTest",
		meta = (ClampMin = "0.0", ClampMax = "1.0", AllowPrivateAccess = "true"))
	float SurfaceFireMinDistanceRatio = 0.15f;
	
	/** rest 상태 바운드. 조각이 흩어져도 MinZ 기준이 흔들리지 않게 한다. */
	FBox CachedRestBounds = FBox(ForceInit);
	
	/** 조각에 붙어 있는 불들. 구조체로 묶어 인덱스 어긋남을 원천 차단한다. */
	UPROPERTY(Transient)
	TArray<FSFSurfaceFire> SurfaceFires;
	
	/** 불 하나가 담당하는 바닥 대각선 길이(cm). 건물이 클수록 불이 많아진다. */
	UPROPERTY(EditDefaultsOnly, Category = "Destruction|BurnTest",
		meta = (ClampMin = "1.0", AllowPrivateAccess = "true"))
	float SurfaceFireSpacing = 250.f;
	
	/** 조각 스케일이 이 값 아래로 내려가면 축소가 시작된 것으로 보고 페이드한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Destruction|BurnTest",
		meta = (ClampMin = "0.0", ClampMax = "1.0", AllowPrivateAccess = "true"))
	// 1.f는 조각메쉬 크기가 변함없을때(처음) 0.2는 떨어진후 크기가 작아지고 비율이 0.2일때
	float SurfaceFireShrinkThreshold = 0.65f;

	/** 불 유지 루프 주기(초). 불이 하나도 없으면 루프가 멈춘다. */
	UPROPERTY(EditDefaultsOnly, Category = "Destruction|BurnTest",
		meta = (ClampMin = "0.02", AllowPrivateAccess = "true"))
	float FireTickInterval = 1.f;
	
	/** 바닥 대각선에 비례한 목표 개수. Min/Max로 잘라낸다. */
	int32 GetTargetSurfaceFireCount() const;
	
	/** 후보 중 최대 DesiredCount개까지 붙이고, 실제로 붙인 개수를 돌려준다. */
	int32 AddSurfaceFires(int32 DesiredCount);

	/** 아직 건물에 붙어 있고 불이 없는 조각들의 위치를 모은다. */
	void CollectAttachedPieceSpots(TArray<FSFPieceSpot>& OutSpots);
	
	// 교체
	/** 불 유지 루프를 시작한다. 이미 돌고 있으면 아무것도 하지 않는다. */
	void StartFireTick();

	/**
	 * 불의 전 생애를 한 번의 순회로 처리한다.
	 * 떨어짐 판정 → 축소 감지 → 페이드 → 부족분 보충 → 종료 판단.
	 */
	void TickSurfaceFires();

	/** 페이드를 시작한다. 파티클은 수명을 다 채우고 컴포넌트는 스스로 소멸한다. */
	static void StartFireFade(FSFSurfaceFire& Fire);
	
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

	FTimerHandle FireTickTimerHandle;
	FTimerHandle StrainHitTimerHandle;
	FTimerHandle ImpulseTimerHandle;
};
