#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayVFXComponent.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UStaticMeshComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STRIKEFORGE_API UPlayVFXComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayVFXComponent();
	
	void PlayConstructionEffect(
			UNiagaraSystem* NiagaraSystem,
			UStaticMeshComponent* TargetMesh);
	
	void StopConstructionEffect();
protected:
	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason) override;
private:
	void DestroyCurrentEffect();
	
private:
	UPROPERTY()
	TObjectPtr<UNiagaraComponent> CurrentNiagaraComponent = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Components|Location", meta=(AllowPrivateAccess = true))
	float AdjustZLocationValue = 15.f;
};
