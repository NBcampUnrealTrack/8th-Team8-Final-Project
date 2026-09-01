#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayVFXComponent.generated.h"

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

};
