#include "PlayVFXComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/StaticMeshComponent.h"
#include "Logging/SFLog.h"

namespace SFVFXParameters
{
	const FName BuildingScale(TEXT("User.BuildingScale"));
	// const FName BurnStage(
	// 	TEXT("User.BurnStage"));
	// const FName SpawnBoxSize(
	// 	TEXT("User.SpawnBoxSize"));
}

UPlayVFXComponent::UPlayVFXComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	
}

void UPlayVFXComponent::PlayConstructionEffect(
	UNiagaraSystem* NiagaraSystem,
	UStaticMeshComponent* TargetMesh)
{
	if (!IsValid(NiagaraSystem) ||
		!IsValid(TargetMesh))
	{
		return;
	}

	FVector SpawnLocation = TargetMesh->Bounds.Origin;
	SpawnLocation.Z += AdjustZLocationValue;

	UNiagaraComponent* NewEffect =
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			NiagaraSystem,
			SpawnLocation,
			TargetMesh->GetComponentRotation(),
			FVector::OneVector,
			true,   // 완료 후 자동 제거
			false); // 파라미터 설정 후 활성화

	if (!IsValid(NewEffect))
	{
		return;
	}

	NewEffect->SetVariableVec3(SFVFXParameters::BuildingScale,
		TargetMesh->GetRelativeScale3D());

	// 새 효과 생성에 성공한 경우에만 기존 효과를 교체한다.
	StopManagedEffect(ESFBuildingVFXChannel::Construction,true);

	ActiveEffects.Add(ESFBuildingVFXChannel::Construction,NewEffect);

	NewEffect->Activate(true);
}

// void UPlayVFXComponent::PlayBurningEffect(UNiagaraSystem* NiagaraSystem, const FTransform& SpawnTransform,
// 	const FVector& SpawnBoxSize, int32 BurnStage)
// {
// 	if (!IsValid(NiagaraSystem) ||
// 	   BurnStage <= 0 ||
// 	   SpawnBoxSize.X <= 0.f ||
// 	   SpawnBoxSize.Y <= 0.f)
// 	{
// 		return;
// 	}
//
// 	UNiagaraComponent* NewEffect =
// 		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
// 			this,
// 			NiagaraSystem,
// 			SpawnTransform.GetLocation(),
// 			SpawnTransform.Rotator(),
// 			FVector::OneVector,
// 			true,   // 완료 후 자동 제거
// 			false); // 파라미터 설정 후 활성화
//
// 	if (!IsValid(NewEffect))
// 	{
// 		return;
// 	}
//
// 	NewEffect->SetVariableInt(SFVFXParameters::BurnStage, BurnStage);
//
// 	NewEffect->SetVariableVec3(SFVFXParameters::SpawnBoxSize, SpawnBoxSize);
//
// 	ActiveBurningEffects.Add(NewEffect);
//
// 	NewEffect->Activate(true);
// }
//
// void UPlayVFXComponent::StopBurningEffects(bool bImmediate)
// {
// 	for (TObjectPtr<UNiagaraComponent>& Effect : ActiveBurningEffects)
// 	{
// 		UNiagaraComponent* EffectComponent = Effect.Get();
//
// 		if (!IsValid(EffectComponent))
// 		{
// 			continue;
// 		}
//
// 		if (bImmediate || !EffectComponent->IsActive())
// 		{
// 			EffectComponent->DestroyComponent();
// 		}
// 		else
// 		{
// 			EffectComponent->Deactivate();
// 		}
// 	}
//
// 	ActiveBurningEffects.Reset();
// }

void UPlayVFXComponent::PlayDestroyNiagaraEffect(
	UNiagaraSystem* NiagaraSystem,
	const FTransform& Transform,
	const FVector& MeshCompScale) const
{
	if (!IsValid(NiagaraSystem))
	{
		return;
	}

	SF_LOG(Warning, "DestroyEffect Location : %s", *Transform.GetLocation().ToString());
	SF_LOG(Warning, "MeshCompScale : %s", *MeshCompScale.ToString());
	
	UNiagaraComponent* SpawnedNiagaraEffect = 
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this,
		NiagaraSystem,
		Transform.GetLocation(),
		Transform.Rotator(),
		FVector::OneVector,
		true,  // 재생 완료 후 자동 제거
		false); // 즉시 재생
	
	SpawnedNiagaraEffect->SetVariableVec3(
		SFVFXParameters::BuildingScale, MeshCompScale);
	
	SpawnedNiagaraEffect->Activate(true);
	
}

void UPlayVFXComponent::StopConstructionEffect()
{
	StopManagedEffect(
		ESFBuildingVFXChannel::Construction,
		false);
}

void UPlayVFXComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TArray<ESFBuildingVFXChannel> Channels;
	ActiveEffects.GetKeys(Channels);

	const bool bImmediate =
		EndPlayReason != EEndPlayReason::Destroyed;

	for (ESFBuildingVFXChannel Channel : Channels)
	{
		StopManagedEffect(Channel, bImmediate);
	}

	//StopBurningEffects(bImmediate);

	Super::EndPlay(EndPlayReason);
}

void UPlayVFXComponent::StopManagedEffect(ESFBuildingVFXChannel Channel, bool bImmediate)
{
	TObjectPtr<UNiagaraComponent> Effect = nullptr;

	// 종료 전에 관리 목록에서 제거한다.
	// 자연 종료 중인 효과가 EndPlay에서 다시 즉시 삭제되는 것을 방지한다.
	if (!ActiveEffects.RemoveAndCopyValue(Channel, Effect))
	{
		return;
	}

	UNiagaraComponent* EffectComponent = Effect.Get();

	if (!IsValid(EffectComponent))
	{
		return;
	}

	if (bImmediate || !EffectComponent->IsActive())
	{
		EffectComponent->DestroyComponent();
		return;
	}

	// 새로운 방출을 막고 기존 파티클은 마무리한다.
	EffectComponent->Deactivate();
}
