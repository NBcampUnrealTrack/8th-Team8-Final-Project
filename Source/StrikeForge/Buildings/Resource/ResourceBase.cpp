#include "ResourceBase.h"

#include "Buildings/Building/MiningBuilding/SFMiningBuilding.h"
#include "Components/BoxComponent.h"

AResourceBase::AResourceBase()
{
	PrimaryActorTick.bCanEverTick = false;
	
	SceneComponent = CreateDefaultSubobject<USceneComponent>("SceneComponent");
	SetRootComponent(SceneComponent);
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("StaticMeshComponent");
	StaticMeshComponent->SetupAttachment(GetRootComponent());
	StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	BoxComponent = CreateDefaultSubobject<UBoxComponent>("BoxComponent");
	BoxComponent->SetupAttachment(GetRootComponent());

	BoxComponent->SetVisibility(true);
	BoxComponent->SetLineThickness(2.f);
	BoxComponent->SetHiddenInGame(false);
	
	BoxComponent->SetCollisionProfileName("SpawnedBuilding");
}

EResourceType AResourceBase::GetResourceType() const
{
	return ResourceType;
}
