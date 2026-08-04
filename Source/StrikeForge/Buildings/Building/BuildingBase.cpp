#include "BuildingBase.h"

#include "ActorComponent/HP/HealthComponent.h"
#include "ActorComponent/Progress/ProgressComponent.h"
#include "Datas/Building/BuildingAppearance.h"
#include "Game/Subsystem/SFDataTableManager.h"
#include "Iris/ReplicationSystem/RepTag.h"
#include "Kismet/GameplayStatics.h"

ABuildingBase::ABuildingBase()
{
	PrimaryActorTick.bCanEverTick = false;
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("StaticMesh");
	StaticMeshComponent->SetupAttachment(GetRootComponent());
	
	HealthComponent = CreateDefaultSubobject<UHealthComponent>("HealthComponent");
	ProgressComponent = CreateDefaultSubobject<UProgressComponent>("ProgressComponent");
}

void ABuildingBase::GetTierRatioList(int32 InCurrentTier)
{
	int32 CountKeys = AllTierMeshesSoftArray.TierBuildingState[InCurrentTier].Ratio_Mesh.GetKeys(CurrentTierRatios);
	CurrentTierRatios.Sort([](const float A, const float B)
	{
		return A < B;
	});
	
	if (CountKeys <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("ABuildingBase::GetTierRatioList: TMap_ratio is Invalid"));
		return;
	}
	
	ProgressComponent->QuarterofRatios = CurrentTierRatios;
	BuildingCompleteSound = AllTierMeshesSoftArray.TierBuildingState[InCurrentTier].CompleteSoundBase;
}

void ABuildingBase::BindFunction()
{
	if (false == ProgressComponent->OnChangedRatio.IsAlreadyBound(this, &ThisClass::ChangeMeshByRatio))
	{
		ProgressComponent->OnChangedRatio.AddDynamic(this, &ThisClass::ChangeMeshByRatio);
	}
	if (false == ProgressComponent->OnCompletedBuilding.IsAlreadyBound(this, &ThisClass::CompletedBuilding))
	{
		ProgressComponent->OnCompletedBuilding.AddDynamic(this, &ThisClass::CompletedBuilding);
	}
}

void ABuildingBase::UnBindFunction()
{
	if (true == ProgressComponent->OnChangedRatio.IsAlreadyBound(this, &ThisClass::ChangeMeshByRatio))
	{
		ProgressComponent->OnChangedRatio.RemoveDynamic(this, &ThisClass::ChangeMeshByRatio);
	}
	if (true == ProgressComponent->OnCompletedBuilding.IsAlreadyBound(this, &ThisClass::CompletedBuilding))
	{
		ProgressComponent->OnCompletedBuilding.RemoveDynamic(this, &ThisClass::CompletedBuilding);
	}
}

void ABuildingBase::ChangeMeshByRatio(int32 InCurrentRatioIndex)
{
	SetMesh(CurrentTierMeshes[InCurrentRatioIndex]);
}

void ABuildingBase::CompletedBuilding()
{
	SetBuildingState(EBuildingState::Completed);
	ProgressComponent->StopProgress();
	UGameplayStatics::PlaySoundAtLocation(
		GetWorld(),
		BuildingCompleteSound,
		GetActorLocation(),
		FRotator::ZeroRotator);
	
	// initinfo on completed
	
}

void ABuildingBase::SetBuildingState(const EBuildingState InState)
{
	CurrentMode = InState;
	//OnChangedBuildingState.Broadcast(CurrentMode);
}

void ABuildingBase::GetTierMeshList(int32 InCurrentTier)
{
	// load > will change assetmanager
	CurrentTierMeshes.Empty();
	for ( auto Key : CurrentTierRatios)
	{
		UStaticMesh* LoadMesh = AllTierMeshesSoftArray.TierBuildingState[InCurrentTier].Ratio_Mesh[Key].LoadSynchronous();
		CurrentTierMeshes.Add(LoadMesh);
	}
}

void ABuildingBase::InitInfo(const FBuildingAppearance* InData)
{
	if (!InData)
	{
		UE_LOG(LogTemp, Warning, TEXT("InitInfo_SetMeshes::InData: Invalid"));
		return;
	}
	
	AllTierMeshesSoftArray = InData->AllTierBuildingStates;
	SetMesh(InData->PreviewMesh);
	DisplayName = InData->DisplayName;
	//BuildingCompleteSound = InData->AllTierBuildingStates.CompleteSoundBase;
	
	UE_LOG(LogTemp, Warning, TEXT("InitInfo_SetMeshes:: Completed."));
}

void ABuildingBase::UpgradeBuilding(int32 UpgradeValue)
{
	int32 TempTier = CurrentTier + UpgradeValue;
	if(!AllTierMeshesSoftArray.TierBuildingState.IsValidIndex(TempTier))
	{
		UE_LOG(LogTemp, Error, TEXT("ABuildingBase::UpgradeBuilding: Excess Index %d / %d"),
			TempTier, AllTierMeshesSoftArray.TierBuildingState.Num());
		return;
	}
	// if resource is property
	CurrentTier += UpgradeValue;
	
	GetTierRatioList(CurrentTier); // get ratio at datatable
	GetTierMeshList(CurrentTier);
	
	StartBuilding();
}

void ABuildingBase::StartBuilding()
{
	SetBuildingState(EBuildingState::Building);
	
	// GetTierRatioList(CurrentTier); // get ratio at datatable
	// GetTierMeshList(CurrentTier);
	
	ProgressComponent->StartProgress();
	
	// upgrade tier where do i need to initial function ( ratio, mesh )
	
}

void ABuildingBase::SetMesh(UStaticMesh* InMesh)
{
	if (!InMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("ABuildingBase::SetMesh: InMesh is None"));
		return;
	}
	StaticMeshComponent->SetStaticMesh(InMesh);
}

void ABuildingBase::BeginPlay()
{
	Super::BeginPlay();
	
	SetBuildingState(EBuildingState::Placing);
	
	USFDataTableManager* Subsystem = GetGameInstance()->GetSubsystem<USFDataTableManager>();
	if (!IsValid(Subsystem)) return;
	
	const FBuildingAppearance* Row = Subsystem->GetDataRow<FBuildingAppearance>(EDataTableType::Building, RowFName);
	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("Building RowName is INVALID"));
		return;
	}
	
	if (!Row->PreviewMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("PreviewMesh is None"));
		return;
	}
	
	InitInfo(Row); // set preview mesh
	
	GetTierRatioList(CurrentTier); // get ratio at datatable
	GetTierMeshList(CurrentTier);
	
	BindFunction(); // bind progress
}

void ABuildingBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnBindFunction();
	Super::EndPlay(EndPlayReason);
}

