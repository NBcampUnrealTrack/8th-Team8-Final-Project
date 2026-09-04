#include "ResourceBase.h"
#include "Components/BoxComponent.h"
#include "Game/Subsystem/SFBuildingGridSubsystem.h"
#include "Logging/SFLog.h"

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

int32 AResourceBase::Extract(int32 InDigAmount)
{
	if (InDigAmount <= 0 || ResourceDeposit <= 0) return 0; 
	
	const int32 ExtractedAmount = FMath::Min(InDigAmount, ResourceDeposit);
	
	ResourceDeposit -= ExtractedAmount;
	if (ResourceDeposit <= 0)
	{
		SF_LOG(Log, "%s :: 고갈", *UEnum::GetValueAsString(ResourceType));
		OnDepleted.Broadcast(this);
	}
	return ExtractedAmount;
}

void AResourceBase::BeginPlay()
{
	Super::BeginPlay();
	
	USFBuildingGridSubsystem* GridSubsystem = GetWorld()->GetSubsystem<USFBuildingGridSubsystem>();
	if (!IsValid(GridSubsystem)) return;
	
	RegisteredResourceGridInfo = GridSubsystem->WorldCenterToGridInfo(
		GetActorLocation(),
		ResourceGridSize);
	
	const FVector WorldCenterVector = GridSubsystem->GridInfoToWorldCenter(RegisteredResourceGridInfo);
	SetActorLocation(WorldCenterVector);
	
	bResourceGridRegistered  = 
	GridSubsystem->TryRegisterResourceGrid(this, RegisteredResourceGridInfo);
	
	if (!bResourceGridRegistered )
	{
		SF_LOG(Warning, "Resource: %s Failed to Register GridSystem", *GetName());
		Destroy();
	}
}

void AResourceBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bResourceGridRegistered )
	{
		if (USFBuildingGridSubsystem* GridSubsystem = GetWorld()->GetSubsystem<USFBuildingGridSubsystem>())
		{
			GridSubsystem->UnregisterResourceGrid(this, RegisteredResourceGridInfo);
		}
		
		bResourceGridRegistered  = false;
	}
	
	Super::EndPlay(EndPlayReason);
}
