#include "PlayVFXComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/StaticMeshComponent.h"
#include "Logging/SFLog.h"

namespace SFConstructionVFX
{
	const FName BuildingScale(
		TEXT("User.BuildingScale"));
}

UPlayVFXComponent::UPlayVFXComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	
}

void UPlayVFXComponent::PlayConstructionEffect(
	UNiagaraSystem* NiagaraSystem,
	UStaticMeshComponent* TargetMesh)
{
	if (!IsValid(TargetMesh))
	{
		SF_LOG(
			Error,
			"PlayConstructionEffect: TargetMesh가 유효하지 않습니다.");
		return;
	}

	if (!IsValid(NiagaraSystem))
	{
		return;
	}

	// 기존 건설 이펙트만 제거한다.
	// 생산 등 다른 채널에는 영향을 주지 않는다.
	StopManagedEffect(
		ESFBuildingVFXChannel::Construction,
		true);

	FVector SpawnLocation =
		TargetMesh->Bounds.Origin;

	SpawnLocation.Z +=
		AdjustZLocationValue;

	const FVector MeshScale =
		TargetMesh->GetRelativeScale3D();
	const FRotator MeshRotation =
		TargetMesh->GetComponentRotation();
	
	UNiagaraComponent* NewEffect =
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			NiagaraSystem,
			SpawnLocation,
			MeshRotation,
			FVector::OneVector,
			true,  // 재생 완료 시 자동 제거
			false  // 파라미터 설정 후 활성화
		);

	if (!IsValid(NewEffect))
	{
		return;
	}

	ActiveEffects.Add(
		ESFBuildingVFXChannel::Construction,
		NewEffect);

	NewEffect->SetVariableVec3(
		SFConstructionVFX::BuildingScale,
		MeshScale);

	NewEffect->Activate(true);
}

void UPlayVFXComponent::PlayOneShotAtLocation(UNiagaraSystem* NiagaraSystem, const FVector& Location,
	const FRotator& Rotation) const
{
	if (!IsValid(NiagaraSystem))
	{
		return;
	}

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this,
		NiagaraSystem,
		Location,
		Rotation,
		FVector::OneVector,
		true,  // 재생 완료 후 자동 제거
		true); // 즉시 재생
}

void UPlayVFXComponent::StopConstructionEffect()
{
	StopManagedEffect(
		ESFBuildingVFXChannel::Construction,
		false);
}

void UPlayVFXComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyAllManagedEffects();

	Super::EndPlay(EndPlayReason);
}

void UPlayVFXComponent::StopManagedEffect(ESFBuildingVFXChannel Channel, bool bImmediate)
{
	TObjectPtr<UNiagaraComponent>* FoundEffect =
		ActiveEffects.Find(Channel);

	if (!FoundEffect)
	{
		return;
	}

	if (!IsValid(*FoundEffect))
	{
		ActiveEffects.Remove(Channel);
		return;
	}

	if (bImmediate)
	{
		(*FoundEffect)->DestroyComponent();
		ActiveEffects.Remove(Channel);
		return;
	}

	// 방출을 정지하고 남은 파티클이 끝나면 AutoDestroy
	(*FoundEffect)->Deactivate();
}

void UPlayVFXComponent::DestroyAllManagedEffects()
{
	for (TPair<ESFBuildingVFXChannel, TObjectPtr<UNiagaraComponent>>& Pair: ActiveEffects)
	{
		if (IsValid(Pair.Value))
		{
			Pair.Value->DestroyComponent();
		}
	}

	ActiveEffects.Empty();
}