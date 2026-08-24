#include "SFMiningBuilding.h"
#include "ActorComponent/Mining/MiningComponent.h"
#include "Game/GameState/SFGameState.h"
#include "Buildings/Resource/ResourceBase.h"
#include "Datas/Building/BuildingDefinition/SFMiningBuildingDefinition.h"
#include "Logging/SFLog.h"

ASFMiningBuilding::ASFMiningBuilding()
{
	PrimaryActorTick.bCanEverTick = false;
	
	MiningComponent = CreateDefaultSubobject<UMiningComponent>("MiningComponent");
}

void ASFMiningBuilding::BeginPlay()
{
	Super::BeginPlay();
	
	BindDelegateFunction();
}

void ASFMiningBuilding::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EndPlayReason != EEndPlayReason::LevelTransition)
	{
		ApplyIncomeRate(0.f);
	}
	
	UnBindDelegateFunction();
	
	Super::EndPlay(EndPlayReason);
}

void ASFMiningBuilding::BindDelegateFunction()
{
	//need to connect widgetcomponent
	//MiningComponent->OnExtractProgressRatioChanged
	
	MiningComponent->OnResourceExtracted.AddUniqueDynamic(this, &ThisClass::OnResourceExtractedReceived);
	MiningComponent->OnResourceDepleted.AddUniqueDynamic(this, &ThisClass::OnResourceDepletedReceived);
}

void ASFMiningBuilding::UnBindDelegateFunction()
{
	MiningComponent->OnResourceExtracted.RemoveDynamic(this, &ThisClass::OnResourceExtractedReceived);
	MiningComponent->OnResourceDepleted.RemoveDynamic(this, &ThisClass::OnResourceDepletedReceived);
}

FSFSelectableInfo ASFMiningBuilding::GetSelectableInfo_Implementation()
{
	FSFSelectableInfo Info = Super::GetSelectableInfo_Implementation();
	Info.SelectableType = ESFSelectableType::NormalBuilding;
	return Info;
}

void ASFMiningBuilding::OnBuildingDefinitionLoaded()
{
	Super::OnBuildingDefinitionLoaded();
	
	const USFMiningBuildingDefinition* Definition =	Cast<USFMiningBuildingDefinition>(GetBuildingDefinition());
	if (!IsValid(Definition))
	{
		SF_LOG(
			Error,
			"[%s] MiningBuildingDefinition이 아닙니다. Definition=%s",
			*GetName(),
			*GetNameSafe(GetBuildingDefinition()));

		return;
	}

	MiningComponent->SetMineResourceType(Definition->CanDigResourceType);
}

void ASFMiningBuilding::OnTierActivated()
{
	Super::OnTierActivated();
	
	const USFMiningBuildingDefinition* Definition =	Cast<USFMiningBuildingDefinition>(GetBuildingDefinition());
	if (!IsValid(Definition))
	{
		SF_LOG(
			Error,
			"[%s] MiningBuildingDefinition이 아닙니다.",
			*GetName());

		return;
	}

	const FSFMiningBuildingTierData* ActivatedTier = Definition->FindMiningTier(GetCurrentTier());
	if (!ActivatedTier)
	{
		SF_LOG(
			Error,
			"[%s] Mining Tier가 없습니다. Tier=%d",
			*GetName(),
			GetCurrentTier());

		return;
	}

	MiningComponent->InitMineProperties(ActivatedTier->ExtractBaseWorkAmount, ActivatedTier->ExtractAmount);
	MiningComponent->StartMining();
	ApplyIncomeRate(MiningComponent->GetIncomeRatePerSeconds());
}

void ASFMiningBuilding::OnTierDeactivated()
{
	Super::OnTierDeactivated();
	
	MiningComponent->StopMining();
	
	ApplyIncomeRate(0.f);
}

bool ASFMiningBuilding::TryLinkResource(AResourceBase* InResourceRef)
{
	ensureMsgf(MiningComponent, TEXT("MineBuilding-TryLinkResource::InValid MiningComponent"));
	return MiningComponent->TryLinkResource(InResourceRef);
}

// update gamestate UI
void ASFMiningBuilding::ApplyIncomeRate(float NewRate)
{
	if (FMath::IsNearlyEqual(CurrentIncomeRatePerSecond, NewRate)) return;

	if (ASFGameState* GS = GetWorld()->GetGameState<ASFGameState>())
	{
		GS->AddResourceIncomeRate(GetTeamIndex(), GetMineResourceType(), NewRate - CurrentIncomeRatePerSecond);
	}
	CurrentIncomeRatePerSecond = NewRate; 
}

//getter
EResourceType ASFMiningBuilding::GetMineResourceType() const
{
	ensureMsgf(MiningComponent, TEXT("MineBuilding-GetMineResourceType::InValid MiningComponent"));
	return MiningComponent->GetMineResourceType();
}

int32 ASFMiningBuilding::GetExtractAmount() const
{
	ensureMsgf(MiningComponent, TEXT("MineBuilding-GetExtractAmount::InValid MiningComponent"));
	return MiningComponent->GetExtractAmount();
}


// delegates
void ASFMiningBuilding::OnResourceExtractedReceived(EResourceType InType, int32 InAmount)
{
	ASFGameState* SFGameState = GetWorld()->GetGameState<ASFGameState>();
	if (!IsValid(SFGameState)) return;
	
	SFGameState->AddResource(GetTeamIndex(), InType, InAmount);
}

void ASFMiningBuilding::OnResourceDepletedReceived()
{
	ApplyIncomeRate(0.f);
}
