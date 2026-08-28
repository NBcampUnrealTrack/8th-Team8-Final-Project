#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SFBuildingDestructionActor.generated.h"

USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFBuildingDestructionStage
{
	GENERATED_BODY()

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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction",
		meta = (ClampMin = "1.0"))
	float Radius = 350.f;

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

	/**
     * GC 에셋과 월드 Transform을 적용한다.
     *
     * InVisualTransform은 BaseBuilding의 ActorTransform이 아니라
     * StaticMeshComponent의 ComponentTransform을 넘겨야 한다.
     */
    UFUNCTION(BlueprintCallable, Category = "SF|Building|Destruction")
    bool InitializeDestruction(
        const UGeometryCollection* InCollection,
        const FTransform& InVisualTransform);

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

    UFUNCTION(BlueprintCallable, Category = "SF|Building|Destruction")
    void RemoveAfter(float InSeconds);

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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Destruction",
        meta = (ClampMin = "0.1", AllowPrivateAccess = "true"))
    float DebrisLifetime = 3.f;

    UPROPERTY(Transient)
    int32 AppliedStageIndex = INDEX_NONE;

    UPROPERTY(Transient)
    bool bInitialized = false;

    UPROPERTY(Transient)
    bool bFinalDestructionApplied = false;

    FVector PendingImpulseOrigin = FVector::ZeroVector;
    float PendingImpulseRadius = 0.f;
    float PendingImpulseStrength = 0.f;

    FTimerHandle ImpulseTimerHandle;
};
