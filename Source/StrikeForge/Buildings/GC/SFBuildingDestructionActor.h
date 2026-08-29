#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SFBuildingDestructionActor.generated.h"

USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFBuildingDestructionStage
{
	GENERATED_BODY()

	/**
	 * 이 Stage가 도달해야 하는 누적 클러스터 깊이.
	 *
	 * 1: 첫 번째 파괴
	 * 2: 두 번째 파괴
	 * 3: 최종 파괴
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Destruction",
		meta = (ClampMin = "1"))
	int32 TargetClusterDepth = 1;
	
	/**
	 * Geometry Collection에 적용할 외부 Strain.
	 * GC의 Damage Threshold와 함께 조정한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction",
		meta = (ClampMin = "0.0"))
	float Strain = 550000.f;

	/**
	 * LocalOffset을 중심으로 Strain을 적용할 반경.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "Destruction",
		meta = (ClampMin = "0.1"))
	float RadiusScale = 1.f;

	/**
	 * GC 컴포넌트 로컬 좌표 기준 파괴 중심점.
	 *
	 * 1단계 반파:
	 * 건물 한쪽 기둥 또는 지붕 방향으로 설정한다.
	 *
	 * 2단계 붕괴:
	 * 건물 중앙 또는 중앙보다 약간 위로 설정한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction")
	FVector LocalOffset = FVector::ZeroVector;

	/**
	 * Strain 적용 후 분리된 파편에 가할 힘.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction",
		meta = (ClampMin = "0.0"))
	float ImpulseStrength = 150.f;

	/**
	 * Impulse를 적용하기 전 기다리는 시간.
	 * Chaos가 먼저 클러스터를 분리할 시간을 준다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction",
		meta = (ClampMin = "0.0", ClampMax = "0.2"))
	float ImpulseDelay = 0.04f;
};

class UGeometryCollection;
class UGeometryCollectionComponent;
class URadialFalloff;

UCLASS()
class STRIKEFORGE_API ASFBuildingDestructionActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ASFBuildingDestructionActor();
	
    UFUNCTION(BlueprintCallable, Category = "SF|Building|Destruction")
    bool InitializeDestruction(
        const UGeometryCollection* InCollection);

    /**
     * 동일하거나 이전 단계는 다시 적용하지 않는다.
     */
    UFUNCTION(BlueprintCallable, Category = "SF|Building|Destruction")
    void ApplyDamageStage(int32 StageIndex);

    /**
     * 체력 0에서 호출한다.
     * 최종 파괴를 적용하고 DebrisLifetime 후 Actor를 제거한다.
     */
    UFUNCTION(BlueprintCallable, Category = "SF|Building|Destruction")
    void ApplyFinalDestruction();

    UFUNCTION(BlueprintPure, Category = "SF|Building|Destruction")
    UGeometryCollectionComponent* GetGeometryCollectionComponent() const
    {
        return GeometryCollectionComponent;
    }

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void ApplyStageSettings(
        const FSFBuildingDestructionStage& StageSettings);
	void ApplyNextStrainHit();
	void SchedulePendingImpulse();
    void ApplyPendingImpulse();

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

	/**
	 * BaseBuilding의 중간 파괴 단계 인덱스와 순서가 같아야 한다.
	 *
	 * 현재 테스트:
	 * [0] 체력 50% 중간 파괴
	 *
	 * 체력 0은 FinalStage가 담당한다.
	 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Destruction",
        meta = (AllowPrivateAccess = "true"))
    TArray<FSFBuildingDestructionStage> DamageStages;

    /**
     * 체력 0에서 사용하는 최종 파괴 설정.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Destruction",
        meta = (AllowPrivateAccess = "true"))
    FSFBuildingDestructionStage FinalStage;

	
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
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Destruction",
		meta = (
			ClampMin = "0.01",
			ClampMax = "0.1",
			AllowPrivateAccess = "true"))
	float StrainHitInterval = 0.03f;
	
    UPROPERTY(Transient)
    bool bInitialized = false;

    UPROPERTY(Transient)
    bool bFinalDestructionApplied = false;
	
	
	FTimerHandle StrainHitTimerHandle;
	FTimerHandle ImpulseTimerHandle;
};
