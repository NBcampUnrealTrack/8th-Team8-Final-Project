#include "SFMiningBuilding.h"
#include "ActorComponent/Mining/MiningComponent.h"
#include "Game/GameState/SFGameState.h"
#include "Buildings/Resource/ResourceBase.h"
#include "Buildings/UI/SFBuildingStatWidget.h"
#include "Datas/Building/BuildingDefinition/SFMiningBuildingDefinition.h"
#include "Logging/SFLog.h"

ASFMiningBuilding::ASFMiningBuilding()
{
	MiningComponent = CreateDefaultSubobject<UMiningComponent>("MiningComponent");
}

void ASFMiningBuilding::BeginPlay()
{
	Super::BeginPlay();
	
	if (IsActorBeingDestroyed())
	{
		return;
	}
	
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
	MiningComponent->OnExtractProgressRatioChanged.AddUniqueDynamic(this, &ThisClass::HandleExtractProgressChanged);
}

void ASFMiningBuilding::UnBindDelegateFunction()
{
	MiningComponent->OnResourceExtracted.RemoveDynamic(this, &ThisClass::OnResourceExtractedReceived);
	MiningComponent->OnResourceDepleted.RemoveDynamic(this, &ThisClass::OnResourceDepletedReceived);
	MiningComponent->OnExtractProgressRatioChanged.RemoveDynamic(this, &ThisClass::HandleExtractProgressChanged);
	
}

FSFSelectableInfo ASFMiningBuilding::GetSelectableInfo_Implementation()
{
	FSFSelectableInfo Info = Super::GetSelectableInfo_Implementation();
	SF_LOG(Warning, "MiningBuilding Selected.");
	Info.SelectableType = ESFSelectableType::ResourceBuilding;
	return Info;
}

const FMinableData ASFMiningBuilding::GetMinableData()
{
	FMinableData Data;
	
	Data.ExtractAmount = GetExtractAmount();
	Data.ResourceLeftAmount = GetResourceAmount();
	Data.GetMiningProgressRatio = GetMiningProgressRatio();
	Data.ResourceType = GetMineResourceType();
	Data.UpgradeCost = GetUpgradeCost();
	Data.MiningComponent = MiningComponent;
	
	return Data;
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
	
	SetProgressColorByResourceType(GetMineResourceType());
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
	
	RefreshMiningState();
}

void ASFMiningBuilding::OnTierDeactivated()
{
	Super::OnTierDeactivated();
	
	RefreshMiningState();
}

void ASFMiningBuilding::SetProgressColorByResourceType(EResourceType InResourceType)
{
	switch (InResourceType)
	{
	case EResourceType::Ether:
		{
			USFBuildingStatWidget* StatHUD = GetStatWidget();
			if (!IsValid(StatHUD)) return;
	
			StatHUD->SetProgressBarColor(FLinearColor::Blue);;
		}
	case EResourceType::Mineral:
		{
			USFBuildingStatWidget* StatHUD = GetStatWidget();
			if (!IsValid(StatHUD)) return;
	
			StatHUD->SetProgressBarColor(FLinearColor::Yellow);
			break;
		}
	default:
		{
			break;
		}
	}
}

bool ASFMiningBuilding::CanAcceptResource(const AResourceBase* InResource) const
{
	return IsValid(MiningComponent) &&
		IsValid(InResource) &&
		!InResource->IsDepleted() &&
		MiningComponent->GetMineResourceType() ==
			InResource->GetResourceType();
}

bool ASFMiningBuilding::TryLinkResource(AResourceBase* InResourceRef)
{
	if (!CanAcceptResource(InResourceRef))
	{
		return false;
	}
	
	const bool bLinked =
		MiningComponent->TryLinkResource(InResourceRef);

	if (bLinked)
	{
		RefreshMiningState();
	}

	return bLinked;
}

void ASFMiningBuilding::UnlinkResource(const AResourceBase* InResource)
{
	if (!IsValid(MiningComponent) || !MiningComponent->IsLinkedResource(InResource))
	{
		return;
	}

	MiningComponent->UnlinkResource();
	RefreshMiningState();
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

void ASFMiningBuilding::RefreshMiningState()
{
	if (!IsValid(MiningComponent)) return;
	
	const bool bCanMine =
		IsTierActivated() &&
		MiningComponent->HasUsableLinkedResource();

	if (!bCanMine)
	{
		MiningComponent->StopMining();
		ApplyIncomeRate(0.f);
		
		// progressbar는 종료하고 다른 UI를 보여한다
		if (USFBuildingStatWidget* StatHUD = GetStatWidget())
		{
			StatHUD->SetProgressVisible(false);
		}
		// TODO:: Alert UI Needed
		
		return;
	}

	// 광질 시작
	MiningComponent->StartMining();

	ApplyIncomeRate(
		MiningComponent->GetIncomeRatePerSeconds());
	
	// 광질이 시작되면 진행도는 보여야한다.
	if (USFBuildingStatWidget* StatHUD = GetStatWidget())
	{
		StatHUD->SetProgressRatio(MiningComponent->GetExtractProgressRatio());
		StatHUD->SetProgressVisible(true);
	}
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

int32 ASFMiningBuilding::GetResourceAmount() const
{
	ensureMsgf(MiningComponent, TEXT("MineBuilding-GetResourceAmount::InValid MiningComponent"));
	return MiningComponent->GetRemainingDeposit();
}

float ASFMiningBuilding::GetMiningProgressRatio() const
{
	ensureMsgf(MiningComponent, TEXT("MineBuilding-GetMiningProgressRatio::InValid MiningComponent"));
	return MiningComponent->GetExtractProgressRatio();
}

// delegates
void ASFMiningBuilding::OnResourceExtractedReceived(EResourceType InType, int32 InAmount)
{
	ASFGameState* SFGameState = GetWorld()->GetGameState<ASFGameState>();
	if (!IsValid(SFGameState)) return;
	
	SFGameState->AddResource(GetTeamIndex(), InType, InAmount);
}

void ASFMiningBuilding::HandleExtractProgressChanged(EResourceType InType, float InRatio)
{
	if (USFBuildingStatWidget* StatHUD = GetStatWidget())
	{
		StatHUD->SetProgressRatio(InRatio);
	}
}

void ASFMiningBuilding::OnResourceDepletedReceived()
{
	RefreshMiningState();
}
