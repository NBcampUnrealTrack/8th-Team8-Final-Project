#include "BuildingBase.h"

#include "ActorComponent/HP/HealthComponent.h"
#include "ActorComponent/Progress/ProgressComponent.h"
#include "Datas/Building/BuildingAppearance.h"
#include "Game/Subsystem/SFDataTableManager.h"
#include "Misc/MapErrors.h"

ABuildingBase::ABuildingBase()
{
	PrimaryActorTick.bCanEverTick = false;
	StaticMesh = CreateDefaultSubobject<UStaticMeshComponent>("StaticMesh");
	StaticMesh->SetupAttachment(GetRootComponent());
	
	HealthComponent = CreateDefaultSubobject<UHealthComponent>("HealthComponent");
	ProgressComponent = CreateDefaultSubobject<UProgressComponent>("ProgressComponent");
}

void ABuildingBase::GetTierMeshList(int32 InCurrentTier)
{
	
}

void ABuildingBase::BeginPlay()
{
	Super::BeginPlay();
	
	USFDataTableManager* Subsystem = GetGameInstance()->GetSubsystem<USFDataTableManager>();
	if (!IsValid(Subsystem)) return;
	
	const FBuildingAppearance* Row = Subsystem->GetDataRow<FBuildingAppearance>(EDataTableType::Building, "Barrack");
	UE_LOG(LogTemp, Warning, TEXT("Building Name : %s"), *Row->DisplayName.ToString());
}

