#pragma once
#include "CoreMinimal.h"
class UNiagaraComponent;
class UNiagaraSystem;
class UMaterialInstanceDynamic;
namespace SFCommandFeedback
{
FVector GroundLocation(AActor* Target);
UNiagaraComponent* Spawn(UObject* WorldContext, UNiagaraSystem* System,
    const FVector& Location, const FVector& Scale, const FLinearColor& Color,
    bool bAutoDestroy, TArray<TObjectPtr<UMaterialInstanceDynamic>>* OutMaterials = nullptr);
}

