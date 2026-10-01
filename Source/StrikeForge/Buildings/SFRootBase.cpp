#include "SFRootBase.h"

#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

ASFRootBase::ASFRootBase(const FObjectInitializer& ObjectInitializer)
     : Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	
	SceneComponent = CreateDefaultSubobject<USceneComponent>("SceneComponent");
	SetRootComponent(SceneComponent);
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("StaticMesh");
	StaticMeshComponent->SetupAttachment(GetRootComponent());
	StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// 배치 격자 데칼이 건물 표면에 묻지 않도록 합니다.
	StaticMeshComponent->bReceivesDecals = false;
	
	BoxComponent = CreateDefaultSubobject<UBoxComponent>("BoxComponent");
	BoxComponent->SetupAttachment(GetRootComponent());

	BoxComponent->SetVisibility(false);
	BoxComponent->SetLineThickness(2.f);
	BoxComponent->SetHiddenInGame(true);
	
	BoxComponent->CanCharacterStepUpOn = ECB_No;
	BoxComponent->SetWalkableSlopeOverride(FWalkableSlopeOverride(WalkableSlope_Unwalkable, 0.f));
}

FSFSelectableInfo ASFRootBase::GetSelectableInfo_Implementation()
{
	//return ISFSelectable::GetSelectableInfo_Implementation();
	FSFSelectableInfo Info;
	Info.DataRowName = RowName;
	return Info;
}

int32 ASFRootBase::GetTeamIndex() const
{
	const UCombatTargetComponent* Combat = FindComponentByClass<UCombatTargetComponent>();
	return Combat ? Combat->GetTeamIndex() : Targetable::NeutralTeam;
}
