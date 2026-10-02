#include "SFMiningBuilding.h"
#include "ActorComponent/Mining/MiningComponent.h"
#include "ActorComponent/UI/SFOverheadUIComponent.h"
#include "Game/GameState/SFGameState.h"
#include "ActorComponent/UI/SFOverheadStatusComponent.h"
#include "Datas/Building/BuildingDefinition/SFMiningBuildingDefinition.h"
#include "Game/Subsystem/SFBuildingGridSubsystem.h"
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
	
	MiningComponent->OnResourceExtracted.AddUniqueDynamic(this, &ThisClass::OnResourceExtractedReceived);
	MiningComponent->OnResourceDepleted.AddUniqueDynamic(this, &ThisClass::OnResourceDepletedReceived);
	// 진행도는 건물을 거치지 않고 상태 컴포넌트가 직접 받는다.
	MiningComponent->OnExtractProgressRatioChanged.AddUniqueDynamic(
		StatusComponent, &USFOverheadStatusComponent::HandleExtractProgressRatio);
}

void ASFMiningBuilding::UnBindDelegateFunction()
{
	MiningComponent->OnResourceExtracted.RemoveDynamic(this, &ThisClass::OnResourceExtractedReceived);
	MiningComponent->OnResourceDepleted.RemoveDynamic(this, &ThisClass::OnResourceDepletedReceived);
	MiningComponent->OnExtractProgressRatioChanged.RemoveDynamic(
		StatusComponent, &USFOverheadStatusComponent::HandleExtractProgressRatio);
	
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

	ConfiguredResourceSearchGridSize = Definition->ResourceSearchGridSize;

	const FIntPoint BuildingGridSize = GetGridSize();
	const bool bIsValidSearchGrid =
		USFBuildingGridSubsystem::IsCenteredSubGridSizeValid(
			BuildingGridSize,
			ConfiguredResourceSearchGridSize);

	if (!bIsValidSearchGrid)
	{
		SF_LOG(
			Error,
			"[%s] ResourceSearchGridSize가 건물 중심에 배치될 수 없습니다. Building=%s, Search=%s",
			*GetName(),
			*BuildingGridSize.ToString(),
			*ConfiguredResourceSearchGridSize.ToString());
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
	// 색 규칙은 SFOverheadUIColors 한 곳에 있다. 획득 배지도 같은 함수를 쓴다.
	StatusComponent->SetProgressColor(SFOverheadUIColors::FromResourceType(InResourceType));
}

bool ASFMiningBuilding::SetMiningTargets(const TArray<FSFMiningCellTarget>& InTargets)
{
	if (!IsValid(MiningComponent)) return false;

	const bool bHasUsableTarget =
		MiningComponent->SetMiningTargets(InTargets);

	RefreshMiningState();

	return bHasUsableTarget;
}

void ASFMiningBuilding::ClearMiningTargets()
{
	if (!IsValid(MiningComponent))
	{
		return;
	}

	MiningComponent->ClearMiningTargets();
	RefreshMiningState();
}

FIntPoint ASFMiningBuilding::GetResourceSearchGridSize() const
{
	return ConfiguredResourceSearchGridSize;
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

	/*
	 * 티어가 활성화되어 있고,
	 * 실제로 사용할 수 있는 0번 채굴 대상을
	 * 준비할 수 있어야 채굴을 시작합니다.
	 */
	const bool bMiningStarted =
		IsTierActivated() && MiningComponent->TryStartMining();

	if (!bMiningStarted)
	{
		MiningComponent->StopMining();
		ApplyIncomeRate(0.f);

		OverheadUI->SetElementVisible(
			ESFOverheadElement::Progress,false);

		return;
	}

	ApplyIncomeRate(MiningComponent->GetIncomeRatePerSeconds());

	OverheadUI->SetElementVisible(
		ESFOverheadElement::Progress,IsOwnedByLocalPlayer());
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

void ASFMiningBuilding::SetProductionSpeedModifier(
	FName ModifierId,
	float InMultiplier)
{
	if (!IsValid(MiningComponent))
	{
		return;
	}

	MiningComponent->SetWorkRateModifier(ModifierId, InMultiplier);
	RefreshMiningState();
}

void ASFMiningBuilding::RemoveProductionSpeedModifier(FName ModifierId)
{
	if (!IsValid(MiningComponent))
	{
		return;
	}

	MiningComponent->RemoveWorkRateModifier(ModifierId);
	RefreshMiningState();
}

// delegates
void ASFMiningBuilding::OnResourceExtractedReceived(EResourceType InType, int32 InAmount)
{
	ASFGameState* SFGameState = GetWorld()->GetGameState<ASFGameState>();
	if (!IsValid(SFGameState)) return;
	
	SFGameState->AddResource(GetTeamIndex(), InType, InAmount);

	/*
	 * 획득은 상태가 아니라 사건이다.
	 * 1초 뒤에 물어보면 보여줄 게 없으므로 상태 컴포넌트를 거치지 않고 바로 전달한다.
	 *
	 * 자원은 팀과 무관하게 쌓이지만 배지는 내 것만 띄운다.
	 * 남의 수입은 보여줄 이유가 없고, 시야에 들어온 적 광산의 채굴량이 그대로 노출된다.
	 */
	if (!IsOwnedByLocalPlayer())	return;

	OverheadUI->PlayResourceGain(InType, InAmount);
}

void ASFMiningBuilding::OnResourceDepletedReceived()
{
	RefreshMiningState();
}
