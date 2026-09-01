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
	Production // 추후 확장 예정
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
 * 소유 건물과 독립적으로 재생되는 일회성 이펙트.
 * 재생 완료 후 AutoDestroy되며 ActiveEffects에는 저장하지 않는다.
 */
	void PlayOneShotAtLocation(
		UNiagaraSystem* NiagaraSystem,
		const FVector& Location,
		const FRotator& Rotation) const;
	
	
	void StopConstructionEffect();
protected:
	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason) override;
private:
	/** 해당 채널의 이펙트를 정지하거나 즉시 제거한다. */
	void StopManagedEffect(
		ESFBuildingVFXChannel Channel,
		bool bImmediate);

	/** 건물이 제거될 때 관리 중인 모든 이펙트를 제거한다. */
	void DestroyAllManagedEffects();
	
private:
	UPROPERTY(Transient)
	TMap<
		ESFBuildingVFXChannel,
		TObjectPtr<UNiagaraComponent>>
	ActiveEffects;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "VFX|Construction",
		meta = (AllowPrivateAccess = "true"))
	float AdjustZLocationValue = 15.f;
};
