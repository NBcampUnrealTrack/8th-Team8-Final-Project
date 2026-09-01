#include "PlayVFXComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/StaticMeshComponent.h"

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
	if (!IsValid(NiagaraSystem) ||
		!IsValid(TargetMesh))
	{
		return;
	}

	// staticmeshcomponent의 scale값이 10, 10, 10일떄
	// niagara 의 scale크기가 4, 4, 4일때 얼추 맞음
	
	// 실제 화면에 표시되는 월드 스케일
	const FVector MeshScale = TargetMesh->GetComponentScale().GetAbs();

	// 현재 건물 스케일이 균등하다는 전제
	const float ScaleValue = MeshScale.GetMax();

	const FVector TransformScale = MeshScale * 0.5f;

	// 스케일이 1보다 작아도 이펙트가 사라지지 않도록 최소 1 보장
	const float Multiplier = FMath::Max(
		1.f,
		FMath::FloorToFloat(FMath::Sqrt(ScaleValue)));

	UNiagaraComponent* NiagaraComponent =
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				this,
				NiagaraSystem,
				TargetMesh->Bounds.Origin,
				TargetMesh->GetComponentRotation(),
				FVector::OneVector, // User.TransformScale과 이중 적용 방지
				true,
				false);            // 파라미터 설정 후 활성화

	if (!IsValid(NiagaraComponent))
	{
		return;
	}

	NiagaraComponent->SetVariableVec3(
		TEXT("User.TransformScale"),
		TransformScale);

	NiagaraComponent->SetVariableInt(
		TEXT("User.CountMultiplier"),
		Multiplier);

	NiagaraComponent->SetVariableFloat(
		TEXT("User.SizeMultiplier"),
		Multiplier);

	NiagaraComponent->Activate(true);
}