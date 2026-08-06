#include "BuildingBase.h"
#include "ActorComponent/HP/HealthComponent.h"
#include "ActorComponent/Progress/ProgressComponent.h"
#include "Datas/Building/BuildingAppearance.h"
#include "Game/Subsystem/SFDataTableManager.h"
#include "Kismet/GameplayStatics.h"

ABuildingBase::ABuildingBase()
{
	PrimaryActorTick.bCanEverTick = false;
	
	HealthComponent = CreateDefaultSubobject<UHealthComponent>("HealthComponent");
	ProgressComponent = CreateDefaultSubobject<UProgressComponent>("ProgressComponent");
}

void ABuildingBase::InitializeBuildingData()
{
	USFDataTableManager* Subsystem = GetGameInstance()->GetSubsystem<USFDataTableManager>();
	if (!IsValid(Subsystem)) return;
	
	const FBuildingAppearance* Row = Subsystem->GetDataRow<FBuildingAppearance>(EDataTableType::Building, RowFName);
	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("Building RowName is INVALID"));
		return;
	}
	
	if (!Row->PreviewMeshMaterial)
	{
		UE_LOG(LogTemp, Error, TEXT("PreviewMeshMaterial is None"));
		return;
	}
	
	InitInfo(Row); 
	LoadTierRatioList(CurrentTier); // get ratio at datatable
	LoadTierMeshList(CurrentTier);
	/////////////////////////////////////////////////////////////////
	///
	// ApplyPreviewMesh(); // set preview mesh
	// ApplyPreviewMaterial(true); // true = green false = red;
}
FName ABuildingBase::OnSelected_Implementation()
{
	UE_LOG(LogTemp, Warning, TEXT("ABuildingBase::OnSelected_Implementation"));
	return RowFName;
}

void ABuildingBase::OnDeSelected_Implementation()
{
	UE_LOG(LogTemp, Warning, TEXT("ABuildingBase::OnDeSelected_Implementation"));
	
}

void ABuildingBase::TakeDamaged_Implementation(float InDamage)
{
	HealthComponent->ApplyDamage(InDamage);
}

void ABuildingBase::OnTakeAttacking(float InBeforeHP, float InCurrentHP, float InMaxHP)
{
	UE_LOG(LogTemp, Warning, TEXT("ABuildingBase::OnTakeAttacking"));
	
	// play taken dmg montage
	// play sound
}

void ABuildingBase::OnDeath()
{
	UE_LOG(LogTemp, Warning, TEXT("ABuildingBase::OnDeath"));
	
	// play death montage
	// play sound
	
	// do not destroy()
}

void ABuildingBase::LoadTierRatioList(int32 InCurrentTier)
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
	
	ProgressComponent->SetQuarterOfRatios(CurrentTierRatios);
	BuildingCompleteSound = AllTierMeshesSoftArray.TierBuildingState[InCurrentTier].CompleteSoundBase;
}

void ABuildingBase::EnterPreviewState()
{
	//SetBuildingState(EBuildingState::Preview);

	ApplyPreviewMesh();
	ApplyPreviewMaterial(true);
}

void ABuildingBase::BeginConstruction()
{
	//SetBuildingState(EBuildingState::UnderConstruction);
	
	LoadTierRatioList(CurrentTier); // get ratio at datatable
	LoadTierMeshList(CurrentTier);
	
	StaticMeshComponent->SetMaterial(0, nullptr);

	if (CurrentTierMeshes.IsValidIndex(0))
	{
		SetMesh(CurrentTierMeshes[0]);
	}

	ProgressComponent->StartProgress();
}

void ABuildingBase::EnterCompletedState()
{ // set last mesh completed
	//SetBuildingState(EBuildingState::Completed);
	//ProgressComponent->StopProgress();
	StaticMeshComponent->SetMaterial(0, nullptr);
	if (!CurrentTierMeshes.IsEmpty())
	{
		SetMesh(CurrentTierMeshes.Last());
	}
}

void ABuildingBase::ApplyPreviewMesh()
{
	UStaticMesh* InMesh = CurrentTierMeshes[CurrentTierMeshes.Num() - 1];
	StaticMeshComponent->SetStaticMesh(InMesh);
}

void ABuildingBase::ApplyPreviewMaterial(uint8 bIsPlaced)
{
	if (bIsPlaced)
	{
		OverlayMID = UMaterialInstanceDynamic::Create(BuildingPreviewMaterialInstance, this);
		OverlayMID->SetVectorParameterValue("PreviewColor", FLinearColor(0.048881,1.000000,0.000000,1.000000));
		StaticMeshComponent->SetMaterial(0, OverlayMID);
	}
	else
	{
		OverlayMID = UMaterialInstanceDynamic::Create(BuildingPreviewMaterialInstance, this);
		OverlayMID->SetVectorParameterValue("PreviewColor", FLinearColor(0.700000,0.000000,0.000000,1.000000));
		StaticMeshComponent->SetMaterial(0, OverlayMID);
	}
}

void ABuildingBase::BindFunction()
{
	if (false == ProgressComponent->OnChangedRatio.IsAlreadyBound(this, &ThisClass::ChangeMeshByRatio))
	{
		ProgressComponent->OnChangedRatio.AddDynamic(this, &ThisClass::ChangeMeshByRatio);
	}
	if (false == ProgressComponent->OnCompletedBuilding.IsAlreadyBound(this, &ThisClass::CompleteConstruction))
	{
		ProgressComponent->OnCompletedBuilding.AddDynamic(this, &ThisClass::CompleteConstruction);
	}
	
	if (false == HealthComponent->OnHealthValueChanged.IsAlreadyBound(this, &ThisClass::OnTakeAttacking))
	{
		HealthComponent->OnHealthValueChanged.AddDynamic(this, &ThisClass::OnTakeAttacking);
	}
	if (false == HealthComponent->OnHealthIsZero.IsAlreadyBound(this, &ThisClass::OnDeath))
	{
		HealthComponent->OnHealthIsZero.AddDynamic(this, &ThisClass::OnDeath);
	}
	
}

void ABuildingBase::UnBindFunction()
{
	if (true == ProgressComponent->OnChangedRatio.IsAlreadyBound(this, &ThisClass::ChangeMeshByRatio))
	{
		ProgressComponent->OnChangedRatio.RemoveDynamic(this, &ThisClass::ChangeMeshByRatio);
	}
	if (true == ProgressComponent->OnCompletedBuilding.IsAlreadyBound(this, &ThisClass::CompleteConstruction))
	{
		ProgressComponent->OnCompletedBuilding.RemoveDynamic(this, &ThisClass::CompleteConstruction);
	}
	
	if (true == HealthComponent->OnHealthValueChanged.IsAlreadyBound(this, &ThisClass::OnTakeAttacking))
	{
		HealthComponent->OnHealthValueChanged.RemoveDynamic(this, &ThisClass::OnTakeAttacking);
	}
	if (true == HealthComponent->OnHealthIsZero.IsAlreadyBound(this, &ThisClass::OnDeath))
	{
		HealthComponent->OnHealthIsZero.RemoveDynamic(this, &ThisClass::OnDeath);
	}
}

void ABuildingBase::ChangeMeshByRatio(int32 InCurrentRatioIndex)
{
	SetMesh(CurrentTierMeshes[InCurrentRatioIndex]);
}

void ABuildingBase::PlayBuildingEffect()
{
	UGameplayStatics::PlaySoundAtLocation(
		GetWorld(),
		BuildingCompleteSound,
		GetActorLocation(),
		FRotator::ZeroRotator);

}

void ABuildingBase::SetBuildingState(const EBuildingState InState)
{
	if (CurrentBuildingState == InState)
	{
		return;
	}

	CurrentBuildingState = InState;
	ApplyBuildingState(InState);
}

void ABuildingBase::ApplyBuildingState(const EBuildingState InState)
{
	switch (InState)
	{
	case EBuildingState::Preview:
		EnterPreviewState();
		break;

	case EBuildingState::UnderConstruction:
		BeginConstruction();
		break;

	case EBuildingState::Completed:
		EnterCompletedState();
		break;
		
	default:
		break;
	}
}

void ABuildingBase::LoadTierMeshList(int32 InCurrentTier)
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
	// material apply
	// completedbuiling apply material
	//
	///////
	//SetMesh(InData->PreviewMesh);
	DisplayName = InData->DisplayName;
	BuildingPreviewMaterialInstance = InData->PreviewMeshMaterial;
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
	// if ( resource is property )
	//
	//
	//
	//
	////////////////////////////
	///
	CurrentTier += UpgradeValue;
	SetBuildingState(EBuildingState::UnderConstruction);
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

void ABuildingBase::CompleteConstruction()
{
	SetBuildingState(EBuildingState::Completed);
	PlayBuildingEffect();
}

void ABuildingBase::BeginPlay()
{
	Super::BeginPlay();
	
	BindFunction(); // bind progress
	
	InitializeBuildingData();
}

void ABuildingBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnBindFunction();
	Super::EndPlay(EndPlayReason);
	
}

