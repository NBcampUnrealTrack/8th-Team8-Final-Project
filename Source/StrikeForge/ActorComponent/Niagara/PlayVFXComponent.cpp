#include "PlayVFXComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/StaticMeshComponent.h"
#include "Logging/SFLog.h"

namespace SFConstructionVFX
{
	const FName CountMultiplier(
		TEXT("User.CountMultiplier"));

	const FName TransformScale(
		TEXT("User.TransformScale"));

	const FName SizeMultiplier(
		TEXT("User.SizeMultiplier"));
}

UPlayVFXComponent::UPlayVFXComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	
}

void UPlayVFXComponent::PlayConstructionEffect(
	UNiagaraSystem* NiagaraSystem,
	UStaticMeshComponent* TargetMesh)
{
	if (!IsValid(NiagaraSystem) || !IsValid(TargetMesh))
	{
		SF_LOG(Error, "PlayConstructionEffect:: InValid NiagaraSystem, InValid StaticMeshComponent");
		return;
	}

	if (IsValid(CurrentNiagaraComponent))
	{
		CurrentNiagaraComponent->DestroyComponent();
		CurrentNiagaraComponent = nullptr;
	}
	// staticmeshcomponent의 scale값이 10, 10, 10일떄
	// niagara 의 scale크기가 4, 4, 4일때 얼추 맞음
	
	FVector NiagaraSpawnLocation = TargetMesh->Bounds.Origin;
	NiagaraSpawnLocation.Z += AdjustZLocationValue;
	
	const FVector MeshScale =
			TargetMesh->GetComponentScale().GetAbs();

	// 현재 건물은 균등 스케일이라는 전제
	const float ScaleValue = MeshScale.GetMax();

	const FVector TransformScale =
		MeshScale * 0.5f;

	// 스케일이 1보다 작아도 이펙트가 사라지지 않도록 최소 1 보장
	const float Multiplier = FMath::Max(
		1.f,
		FMath::FloorToFloat(FMath::Sqrt(ScaleValue * 0.5f)));

		CurrentNiagaraComponent =
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				this,
				NiagaraSystem,
				NiagaraSpawnLocation,
				TargetMesh->GetComponentRotation(),
				FVector::OneVector, // User.TransformScale과 이중 적용 방지
				true,
				false);            // 파라미터 설정 후 활성화

	if (!IsValid(CurrentNiagaraComponent))
	{
		return;
	}

	CurrentNiagaraComponent->SetVariableVec3(
		TEXT("User.TransformScale"),
		TransformScale);

	CurrentNiagaraComponent->SetVariableInt(
		TEXT("User.CountMultiplier"),
		Multiplier);

	CurrentNiagaraComponent->SetVariableFloat(
		TEXT("User.SizeMultiplier"),
		Multiplier);

	CurrentNiagaraComponent->Activate(true);
}

void UPlayVFXComponent::StopConstructionEffect()
{
	if (!IsValid(CurrentNiagaraComponent)) return;
	// 방출을 멈추고 남은 파티클이 끝나면 AutoDestroy
	CurrentNiagaraComponent->Deactivate();
}

void UPlayVFXComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 건물이 직접 제거되거나 레벨이 종료되는 경우 즉시 정리
	DestroyCurrentEffect();
	
	Super::EndPlay(EndPlayReason);
}

void UPlayVFXComponent::DestroyCurrentEffect()
{
	if (IsValid(CurrentNiagaraComponent))
	{
		CurrentNiagaraComponent->DestroyComponent();
	}

	CurrentNiagaraComponent = nullptr;
}
