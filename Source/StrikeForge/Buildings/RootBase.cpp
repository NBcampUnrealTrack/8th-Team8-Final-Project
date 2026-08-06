#include "RootBase.h"
#include "Components/StaticMeshComponent.h"

ARootBase::ARootBase()
{
	PrimaryActorTick.bCanEverTick = false;
	
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("StaticMesh");
	StaticMeshComponent->SetupAttachment(GetRootComponent());
}
