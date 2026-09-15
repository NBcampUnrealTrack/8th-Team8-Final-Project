#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayVFXComponent.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UStaticMeshComponent;

UENUM()
enum class ESFBuildingVFXChannel : uint8
{
	Construction,
	//Production // 추후 확장 예정
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STRIKEFORGE_API UPlayVFXComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayVFXComponent();
	
	void PlayConstructionEffect(
			UNiagaraSystem* NiagaraSystem,
			UStaticMeshComponent* TargetMesh);
	
	/**
 * 새로운 손상 단계의 불꽃 효과를 생성한다.
 * 이전 단계에서 생성한 불꽃은 변경하지 않는다.
 */
	void PlayBurningEffect(
		UNiagaraSystem* NiagaraSystem,
		const FTransform& SpawnTransform,
		const FVector& SpawnBoxSize,
		int32 BurnStage);

	/** 생성된 모든 불꽃 효과를 종료한다. */
	void StopBurningEffects(bool bImmediate = false);
	
	/**
 * 소유 건물과 독립적으로 재생되는 일회성 이펙트.
 * 재생 완료 후 AutoDestroy되며 ActiveEffects에는 저장하지 않는다.
 */
	void PlayDestroyNiagaraEffect(
		UNiagaraSystem* NiagaraSystem,
		const FTransform& Transform,
		const FVector& MeshCompScale) const;
	
	void StopConstructionEffect();
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	/** 해당 채널의 이펙트를 정지하거나 즉시 제거한다. */
	void StopManagedEffect(
		ESFBuildingVFXChannel Channel,
		bool bImmediate);
	
private:
	UPROPERTY(Transient)
	TMap<
		ESFBuildingVFXChannel,
		TObjectPtr<UNiagaraComponent>>
	ActiveEffects;

	/** 손상 단계별로 생성된 불꽃 효과들. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> ActiveBurningEffects;
	
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "VFX|Construction",
		meta = (AllowPrivateAccess = "true"))
	float AdjustZLocationValue = 15.f;
};
