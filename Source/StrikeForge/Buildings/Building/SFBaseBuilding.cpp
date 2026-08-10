#include "SFBaseBuilding.h"
#include "ActorComponent/HP/HealthComponent.h"
#include "ActorComponent/Progress/ProgressComponent.h"
#include "Datas/Building/BuildingAppearance.h"
#include "Game/Subsystem/SFDataTableManager.h"
#include "Kismet/GameplayStatics.h"

ASFBaseBuilding::ASFBaseBuilding()
{
	PrimaryActorTick.bCanEverTick = false;
	
	HealthComponent = CreateDefaultSubobject<UHealthComponent>("HealthComponent");
	ProgressComponent = CreateDefaultSubobject<UProgressComponent>("ProgressComponent");
}

void ASFBaseBuilding::InitializeBuildingData()
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

void ASFBaseBuilding::TakeDamaged_Implementation(float InDamage)
{
	HealthComponent->ApplyDamage(InDamage);
}

void ASFBaseBuilding::OnTakeAttacking(float InBeforeHP, float InCurrentHP, float InMaxHP)
{
	UE_LOG(LogTemp, Warning, TEXT("ABuildingBase::OnTakeAttacking"));
	
	// play taken dmg montage
	// play sound
}

void ASFBaseBuilding::HandleHealthZero()
{
	UE_LOG(LogTemp, Warning, TEXT("ABuildingBase::OnDeath"));
	
	// play sound
	// destroy mesh
	// collision unenable
	// stop progress
	
	// do not destroy()
	
	// virtual function
	OnBuildingDestroyed();
}

void ASFBaseBuilding::LoadTierRatioList(int32 InCurrentTier)
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

void ASFBaseBuilding::EnterPreviewState()
{
	//SetBuildingState(EBuildingState::Preview);

	ApplyPreviewMesh();
	ApplyPreviewMaterial(true);
}

void ASFBaseBuilding::BeginConstruction()
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

void ASFBaseBuilding::EnterCompletedState()
{ // set last mesh completed
	//SetBuildingState(EBuildingState::Completed);
	//ProgressComponent->StopProgress();
	StaticMeshComponent->SetMaterial(0, nullptr);
	if (!CurrentTierMeshes.IsEmpty())
	{
		SetMesh(CurrentTierMeshes.Last());
	}
}

void ASFBaseBuilding::ApplyPreviewMesh()
{
	UStaticMesh* InMesh = CurrentTierMeshes[CurrentTierMeshes.Num() - 1];
	StaticMeshComponent->SetStaticMesh(InMesh);
}

void ASFBaseBuilding::ApplyPreviewMaterial(uint8 bIsPlaced)
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

void ASFBaseBuilding::BindFunctionWithComponent()
{
	if (false == ProgressComponent->OnChangedRatio.IsAlreadyBound(this, &ThisClass::ChangeMeshByRatio))
	{
		ProgressComponent->OnChangedRatio.AddDynamic(this, &ThisClass::ChangeMeshByRatio);
	}
	if (false == ProgressComponent->OnCompletedBuilding.IsAlreadyBound(this, &ThisClass::HandleCompleteConstruction))
	{
		ProgressComponent->OnCompletedBuilding.AddDynamic(this, &ThisClass::HandleCompleteConstruction);
	}
	
	if (false == HealthComponent->OnHealthValueChanged.IsAlreadyBound(this, &ThisClass::OnTakeAttacking))
	{
		HealthComponent->OnHealthValueChanged.AddDynamic(this, &ThisClass::OnTakeAttacking);
	}
	if (false == HealthComponent->OnHealthIsZero.IsAlreadyBound(this, &ThisClass::HandleHealthZero))
	{
		HealthComponent->OnHealthIsZero.AddDynamic(this, &ThisClass::HandleHealthZero);
	}
	
}

void ASFBaseBuilding::UnBindFunctionWithComponent()
{
	if (true == ProgressComponent->OnChangedRatio.IsAlreadyBound(this, &ThisClass::ChangeMeshByRatio))
	{
		ProgressComponent->OnChangedRatio.RemoveDynamic(this, &ThisClass::ChangeMeshByRatio);
	}
	if (true == ProgressComponent->OnCompletedBuilding.IsAlreadyBound(this, &ThisClass::HandleCompleteConstruction))
	{
		ProgressComponent->OnCompletedBuilding.RemoveDynamic(this, &ThisClass::HandleCompleteConstruction);
	}
	
	if (true == HealthComponent->OnHealthValueChanged.IsAlreadyBound(this, &ThisClass::OnTakeAttacking))
	{
		HealthComponent->OnHealthValueChanged.RemoveDynamic(this, &ThisClass::OnTakeAttacking);
	}
	if (true == HealthComponent->OnHealthIsZero.IsAlreadyBound(this, &ThisClass::HandleHealthZero))
	{
		HealthComponent->OnHealthIsZero.RemoveDynamic(this, &ThisClass::HandleHealthZero);
	}
}

void ASFBaseBuilding::ChangeMeshByRatio(int32 InCurrentRatioIndex)
{
	SetMesh(CurrentTierMeshes[InCurrentRatioIndex]);
}

void ASFBaseBuilding::PlayBuildingEffect()
{
	UGameplayStatics::PlaySoundAtLocation(
		GetWorld(),
		BuildingCompleteSound,
		GetActorLocation(),
		FRotator::ZeroRotator);

}

void ASFBaseBuilding::SetBuildingState(const EBuildingState InState)
{
	if (CurrentBuildingState == InState)
	{
		return;
	}

	CurrentBuildingState = InState;
	ApplyBuildingState(InState);
}

void ASFBaseBuilding::ApplyBuildingState(const EBuildingState InState)
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

void ASFBaseBuilding::LoadTierMeshList(int32 InCurrentTier)
{
	// load > will change assetmanager
	CurrentTierMeshes.Empty();
	for ( auto Key : CurrentTierRatios)
	{
		UStaticMesh* LoadMesh = AllTierMeshesSoftArray.TierBuildingState[InCurrentTier].Ratio_Mesh[Key].LoadSynchronous();
		CurrentTierMeshes.Add(LoadMesh);
	}
}

void ASFBaseBuilding::InitInfo(const FBuildingAppearance* InData)
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

void ASFBaseBuilding::UpgradeBuilding(int32 UpgradeValue)
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

void ASFBaseBuilding::SetMesh(UStaticMesh* InMesh)
{
	if (!InMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("ABuildingBase::SetMesh: InMesh is None"));
		return;
	}
	StaticMeshComponent->SetStaticMesh(InMesh);
}

void ASFBaseBuilding::HandleCompleteConstruction()
{
	SetBuildingState(EBuildingState::Completed);
	PlayBuildingEffect();
	// product begin in childclass
	OnConstructionFinished();
}

void ASFBaseBuilding::BeginPlay()
{
	Super::BeginPlay();
	
	BindFunctionWithComponent(); // bind progress
	
	InitializeBuildingData();
}

void ASFBaseBuilding::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnBindFunctionWithComponent();
	Super::EndPlay(EndPlayReason);
	
}

