#include "SFRootBase.h"
#include "Components/StaticMeshComponent.h"

ASFRootBase::ASFRootBase()
{
	PrimaryActorTick.bCanEverTick = false;
	
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("StaticMesh");
	//StaticMeshComponent->SetupAttachment(GetRootComponent());
	SetRootComponent(StaticMeshComponent);
}
