#include "SFRootBase.h"
#include "Components/StaticMeshComponent.h"

ASFRootBase::ASFRootBase(const FObjectInitializer& ObjectInitializer)
     : Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("StaticMesh");
	//StaticMeshComponent->SetupAttachment(GetRootComponent());
	SetRootComponent(StaticMeshComponent);
}

FSFSelectableInfo ASFRootBase::GetSelectableInfo_Implementation()
{
	//return ISFSelectable::GetSelectableInfo_Implementation();
	FSFSelectableInfo Info;
	Info.DataRowName = RowName;
	return Info;
}
