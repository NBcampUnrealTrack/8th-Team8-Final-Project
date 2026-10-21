// SFGameState.cpp
#include "SFGameState.h"
#include "Game/GameMode/SFGameMode.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "Logging/SFLog.h"
#include "Buildings/Building/HouseBuilding/Nexus/SFNexusBuilding.h"
#include "Datas/Building/SFBuildingCatalog.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "EngineUtils.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "Datas/Building/SFBuildingDefinition.h"
#include "Interfaces/Units/Targetable.h"

ASFGameState::ASFGameState()
{
	// 상태 초기값
	StageState = ESFStageState::Initializing;
	StageResult = ESFStageResult::Undetermined;
}

bool ASFGameState::InitializeStage(FName StageRowName, 
	const TArray<FSFStageBuildingRule>& InBuildingRules
)
{
	if (StageRowName.IsNone())
	{
		SF_LOG(Warning,	"[SFGameState] "
						"InitializeStage failed : Stage Row Name is None.");

		return false;
	}

	if (StageState != ESFStageState::LoadingRaceData)
	{
		SF_LOG(Warning,
			"[SFGameState] InitializeStage failed: "
			"Stage is not LoadingRaceData. State=%s",
			*UEnum::GetValueAsString(StageState));

		return false;
	}

	CurrentStageRowName = StageRowName;
	const USFGameInstance* Instance = GetGameInstance<USFGameInstance>();
	bClearedStageReplay = GetNetMode() == NM_Standalone && Instance &&
		Instance->GetStageProgressState(StageRowName) == ESFStageProgressState::Cleared;
	CurrentStageBuildingRules =	InBuildingRules;
	bBuildingRestrictionsReleasedAfterTutorial = false;
	CompletedBuildingUnlocksByTeam.Reset();

	SF_LOG(Log, "[SFGameState] Stage initialized : %s | BuildingRules: %d",
		*CurrentStageRowName.ToString(), CurrentStageBuildingRules.Num()
	);

	return true;
}

bool ASFGameState::BeginRaceDataLoading()
{
	if (StageState != ESFStageState::Initializing)
	{
		SF_LOG(Warning, "[SFGameState] Race 데이터 로딩을 시작할 수 없는 상태입니다. State=%s",
			*UEnum::GetValueAsString(StageState));

		return false;
	}

	StageState = ESFStageState::LoadingRaceData;

	SF_LOG(Log, "[SFGameState] Race 데이터 로딩을 시작합니다.");

	OnStageStateChanged.Broadcast(StageState);
	return true;
}

bool ASFGameState::BeginInitialBuildingAssetsLoading()
{
	if (StageState != ESFStageState::LoadingRaceData)
	{
		SF_LOG(
			Warning,
			"[SFGameState] 초기 건물 에셋 로딩을 시작할 수 없는 상태입니다. State=%s",
			*UEnum::GetValueAsString(StageState));

		return false;
	}

	StageState = ESFStageState::LoadingInitialBuildingAssets;

	SF_LOG(Log, "[SFGameState] 초기 건물 에셋 로딩을 시작합니다.");

	OnStageStateChanged.Broadcast(StageState);
	return true;
}

bool ASFGameState::StartStage()
{
	// 스테이지 시작 부분
	if (StageState != ESFStageState::ReadyToStart)
	{
		return false;
	}

	StageState = ESFStageState::Playing;

	SF_LOG(Log, "[SFGameState] Stage started.");

	OnStageStateChanged.Broadcast(StageState);

	return true;
}

bool ASFGameState::FinishStage(ESFStageResult NewStageResult)
{
	if (StageState != ESFStageState::Playing)
	{
		SF_LOG(Warning, "[SFGameState] FinishStage failed: Stage is not Playing.");

		return false;
	}

	if (NewStageResult == ESFStageResult::Undetermined)
	{
		SF_LOG(Warning, "[SFGameState] FinishStage failed: Result is Undetermined.");

		return false;
	}

	/*
	*	1. 이벤트 보내기 전에 결과와 상태를 모두 설정
	*	2. 받은 객체가 즉시 GameState를 조회시
	*	3. 완성된 종료 상태를 확인
	*/
	StageResult = NewStageResult;
	StageState = ESFStageState::Finished;

	SF_LOG(Log, "[SFGameState] Stage finished. Result: %s", *UEnum::GetValueAsString(StageResult));

	OnStageResultChanged.Broadcast(StageResult);
	OnStageStateChanged.Broadcast(StageState);

	return true;
}

FName ASFGameState::GetCurrentStageRowName() const
{
	return CurrentStageRowName;
}

ESFStageState ASFGameState::GetStageState() const
{
	return StageState;
}

ESFStageResult ASFGameState::GetStageResult() const
{
	return StageResult;
}

bool ASFGameState::IsStagePlaying() const
{
	return StageState == ESFStageState::Playing;
}

bool ASFGameState::IsStageFinished() const
{
	return StageState == ESFStageState::Finished;
}

//기존 Delegate 시그니처를 그대로 활용


// 26.08.18
bool ASFGameState::RegisterNexus(int32 TeamIndex, ASFNexusBuilding* Nexus)
{
	if (!IsValid(Nexus))
	{
		SF_LOG(Warning, "[SFGameState] RegisterNexus failed: Nexus is invalid.");
		return false;
	}

	USFTeamState* Team = FindOrAddTeam(TeamIndex);
	if (!Team)
	{
		SF_LOG(Warning, "[SFGameState] RegisterNexus failed: TeamIndex %d is invalid.", TeamIndex);
		return false;
	}

	if (ASFNexusBuilding* Existing = Team->GetNexus())
	{
		if (Existing != Nexus)
		{
			SF_LOG(Warning, "[SFGameState] Team %d 의 넥서스가 교체됩니다. Old: %s, New: %s",
				TeamIndex, *GetNameSafe(Existing), *GetNameSafe(Nexus));
		}
	}

	Team->SetNexus(Nexus);
	return true;
}

bool ASFGameState::UnregisterNexus(int32 TeamIndex, ASFNexusBuilding* Nexus)
{
	USFTeamState* Team = GetTeamState(TeamIndex);
	if (!Team || !Team->IsNexus(Nexus)) return false;

	// 팀 항목 자체는 남긴다 — 자원·인구가 들어있다
	Team->SetNexus(nullptr);
	return true;
}

bool ASFGameState::UnregisterPlayer(int32 TeamIndex, ASFInGamePlayerController* InPlayer)
{
	USFTeamState* Team = GetTeamState(TeamIndex);

	if (!IsValid(Team) || Team->GetPlayer() != InPlayer)
	{
		return false;
	}

	Team->SetPlayer(nullptr);
	return true;
}

ASFNexusBuilding* ASFGameState::GetNexus(int32 TeamIndex) const
{
	const USFTeamState* Team = GetTeamState(TeamIndex);
	return Team ? Team->GetNexus() : nullptr;
}

ASFInGamePlayerController* ASFGameState::GetPlayer(int32 TeamIndex) const
{
	const USFTeamState* Team = GetTeamState(TeamIndex);
	return Team ? Team->GetPlayer() : nullptr;
}

USFTeamState* ASFGameState::GetTeamState(int32 TeamIndex) const
{
	const TObjectPtr<USFTeamState>* Found = TeamStates.Find(TeamIndex);
	return Found ? Found->Get() : nullptr;
}

int32 ASFGameState::GetNexusTier(int32 TeamIndex) const
{
	const ASFNexusBuilding* Nexus = GetNexus(TeamIndex);
	if (!Nexus)
	{
		const ASFGameMode* StageMode = GetWorld()->GetAuthGameMode<ASFGameMode>();
		if (StageMode && StageMode->AllowsStageWithoutInitialBuildings() &&
			TeamIndex == StageMode->GetPlayerTeamIndex())
		{
			// Unit-only survival stages retain basic construction (display tier 1).
			return 0;
		}

		SF_LOG(Error, "[SFGameState] Team %d 에 넥서스가 등록되어 있지 않습니다. "
			"GameMode 의 PlayerTeamIndex 와 레벨 넥서스의 팀이 일치하는지 확인하세요.", TeamIndex);
		return INDEX_NONE;
	}
	return Nexus->GetCurrentTier();
}

void ASFGameState::SetStageLoadingProgress(float InProgress)
{
	StageLoadingProgress = FMath::Clamp(InProgress, 0.f, 1.f);

	OnStageLoadingProgressChanged.Broadcast(StageLoadingProgress);
}

bool ASFGameState::FailStageInitialization()
{
	// 중복 실패 요청은 이미 처리된 것으로 간주한다.
	if (StageState == ESFStageState::InitializationFailed)
	{
		return true;
	}

	switch (StageState)
	{
	case ESFStageState::Initializing:
	case ESFStageState::LoadingRaceData:
	case ESFStageState::LoadingInitialBuildingAssets:
	case ESFStageState::ReadyToStart:
		// ReadyTo가 되면 아래 실행하지않고 조용히 나감
		break;

	default:
		SF_LOG(
			Warning,
			"[SFGameState] 초기화 실패 상태로 전환할 수 없습니다. State=%s",
			*UEnum::GetValueAsString(StageState));

		return false;
	}

	StageState = ESFStageState::InitializationFailed;

	SF_LOG(Error, "[SFGameState] Stage initialization failed.");

	OnStageStateChanged.Broadcast(StageState);
	return true;
}

bool ASFGameState::MarkStageReadyToStart()
{
	if (StageState != ESFStageState::LoadingInitialBuildingAssets)
	{
		SF_LOG(Warning,
			"[SFGameState] ReadyToStart로 전환할 수 없는 상태입니다. State=%s",
			*UEnum::GetValueAsString(StageState));

		return false;
	}

	StageState = ESFStageState::ReadyToStart;

	SF_LOG(Log, "[SFGameState] 스테이지가 시작 준비 상태가 되었습니다.");

	OnStageStateChanged.Broadcast(StageState);
	return true;
}

USFTeamState* ASFGameState::FindOrAddTeam(int32 TeamIndex)
{
	if (TeamIndex == Targetable::NeutralTeam) return nullptr;

	if (TObjectPtr<USFTeamState>* Found = TeamStates.Find(TeamIndex))
	{
		return *Found;
	}

	USFTeamState* NewTeam = NewObject<USFTeamState>(this);
	TeamStates.Add(TeamIndex, NewTeam);
	return NewTeam;
}

// 여기까지 추가분

// 판단로직

bool ASFGameState::CanAffordResource(int32 TeamIndex, const FResources& InResource) const
{
	const USFTeamState* Team = GetTeamState(TeamIndex);
	return Team ? Team->CanAffordResource(InResource) : false;
}

bool ASFGameState::CanAffordPopulation(int32 TeamIndex, int32 InPopulationCost) const
{	// 음수도 가능하게 해서 유닛이 죽엇을때 감소되게함
	const USFTeamState* Team = GetTeamState(TeamIndex);
	return Team ? Team->CanAffordPopulation(InPopulationCost) : false;
}

// add

void ASFGameState::AddResource(int32 TeamIndex, EResourceType InResourceType, int32 InResourceAmount)
{
	if (USFTeamState* Team = FindOrAddTeam(TeamIndex))
		Team->AddResource(InResourceType, InResourceAmount);
}
void ASFGameState::AddResourceIncomeRate(int32 TeamIndex, EResourceType InResourceType, float InIncomeRate)
{
	if (USFTeamState* Team = FindOrAddTeam(TeamIndex)) Team->AddResourceIncomeRate(InResourceType, InIncomeRate);
}
void ASFGameState::AddPopulation(int32 TeamIndex, int32 InMaximumPopulation)
{
if (USFTeamState* Team = FindOrAddTeam(TeamIndex)) Team->AddPopulation(InMaximumPopulation);
}

void ASFGameState::AddMaxPopulation(int32 TeamIndex, int32 InMaximumPopulation)
{	// 최대인구 증가
	if (USFTeamState* Team = FindOrAddTeam(TeamIndex))
		Team->AddMaxPopulation(InMaximumPopulation);
}

// Minus

bool ASFGameState::TrySpendCost(int32 TeamIndex, const FResources& InResources)
{	// 판단 + 소비까지
	USFTeamState* Team = FindOrAddTeam(TeamIndex);
	return Team ? Team->TrySpendCost(InResources) : false;
}

void ASFGameState::RefundCost(int32 TeamIndex, const FResources& InResources)
{
	if (USFTeamState* Team = FindOrAddTeam(TeamIndex))
		Team->RefundCost(InResources);
}

// getter setter

int32 ASFGameState::GetMineral(int32 TeamIndex) const
{
	const USFTeamState* Team = GetTeamState(TeamIndex);
	return Team ? Team->GetMineral() : 0;
}

int32 ASFGameState::GetEther(int32 TeamIndex) const
{
	const USFTeamState* Team = GetTeamState(TeamIndex);
	return Team ? Team->GetEther() : 0;
}

float ASFGameState::GetMineralIncomeRate(int32 TeamIndex) const
{
	const USFTeamState* Team = GetTeamState(TeamIndex);
	return Team ? Team->GetMineralIncomeRate() : 0.f;
}

float ASFGameState::GetEtherIncomeRate(int32 TeamIndex) const
{
	const USFTeamState* Team = GetTeamState(TeamIndex);
	return Team ? Team->GetEtherIncomeRate() : 0.f;
}

int32 ASFGameState::GetCurrentPopulation(int32 TeamIndex) const
{
	const USFTeamState* Team = GetTeamState(TeamIndex);
	return Team ? Team->GetCurrentPopulation() : 0;
}

int32 ASFGameState::GetMaximumPopulation(int32 TeamIndex) const
{
	const USFTeamState* Team = GetTeamState(TeamIndex);
	return Team ? Team->GetMaximumPopulation() : 0;
}

bool ASFGameState::RegisterTeam(int32 TeamIndex, ESFRace InRace, USFBuildingCatalog* InBuildingCatalog)
{
	if (!IsValid(InBuildingCatalog))
	{
		SF_LOG(Error,
			"[SFGameState] Team %d에 설정할 "
			"BuildingCatalog가 유효하지 않습니다.",
			TeamIndex);

		return false;
	}

	USFTeamState* Team = FindOrAddTeam(TeamIndex);

	if (!IsValid(Team))
	{
		SF_LOG(
			Error,
			"[SFGameState] TeamState를 준비할 수 없습니다. "
			"Team=%d",
			TeamIndex);

		return false;
	}

	return Team->InitializeTeamData(InRace, InBuildingCatalog);
}

bool ASFGameState::RegisterPlayer(int32 TeamIndex, ASFInGamePlayerController* InPlayer)
{
	if (!IsValid(InPlayer))
	{
		return false;
	}
	/*
	 * FindOrAddTeam을 사용하지 않는다.
	 * Controller가 들어오는 시점에는 InitGameState에서
	 * 팀이 이미 생성되어 있어야 한다.
	 */
	USFTeamState* Team = GetTeamState(TeamIndex);

	if (!IsValid(Team))
	{
		SF_LOG(
			Error,
			"[SFGameState] Player를 연결할 팀이 없습니다. "
			"Team=%d, Player=%s",
			TeamIndex,
			*GetNameSafe(InPlayer));

		return false;
	}

	if (ASFInGamePlayerController* Existing = Team->GetPlayer())
	{
		if (Existing != InPlayer)
		{
			SF_LOG(
				Warning,
				"[SFGameState] Team %d의 Player가 교체됩니다. "
				"Old=%s, New=%s",
				TeamIndex,
				*GetNameSafe(Existing),
				*GetNameSafe(InPlayer));
		}
	}

	Team->SetPlayer(InPlayer);
	return true;
}

USFBuildingCatalog* ASFGameState::GetTeamBuildingCatalog(int32 TeamIndex) const
{
	const USFTeamState* Team = GetTeamState(TeamIndex);

	return IsValid(Team)?
			Team->GetBuildingCatalog(): nullptr;
}

bool ASFGameState::IsBuildingAllowedInCurrentStage(
	FName InBuildingId
) const
{
	if (InBuildingId.IsNone())
	{
		return false;
	}

	if (bBuildingRestrictionsReleasedAfterTutorial)
	{
		return true;
	}

	for (const FSFStageBuildingRule& Rule
		: CurrentStageBuildingRules)
	{
		if (Rule.BuildingId == InBuildingId)
		{
			return Rule.bCanBuild;
		}
	}

	// 규칙에 없는 건물은 기본적으로 허용
	return true;
}

void ASFGameState::ReleaseBuildingRestrictionsAfterTutorial()
{
	if (bBuildingRestrictionsReleasedAfterTutorial)
	{
		return;
	}

	bBuildingRestrictionsReleasedAfterTutorial = true;
	OnStageBuildingPermissionsChanged.Broadcast();
}

bool ASFGameState::AreBuildingRequirementsMet(
	int32 TeamIndex,
	FName InBuildingId
) const
{
	if (InBuildingId.IsNone())
	{
		return false;
	}

	const FSFStageBuildingRule* TargetRule =
		nullptr;

	for (
		const FSFStageBuildingRule& Rule :
		CurrentStageBuildingRules
		)
	{
		if (Rule.BuildingId == InBuildingId)
		{
			TargetRule =
				&Rule;

			break;
		}
	}

	// 테크 조건이 등록되지 않은 건물은 기본적으로 해금
	if (!TargetRule)
	{
		return true;
	}

	for (
		const FName& RequiredBuildingId :
		TargetRule->RequiredBuildingIds
		)
	{
		if (!HasCompletedBuilding(
			TeamIndex,
			RequiredBuildingId
		))
		{
			return false;
		}
	}

	return true;
}

void ASFGameState::RecordCompletedBuilding(
	int32 TeamIndex, FName BuildingId, FName SelectionUIRowName) const
{
	if (TeamIndex < 0)
	{
		return;
	}

	TSet<FName>& Unlocks = CompletedBuildingUnlocksByTeam.FindOrAdd(TeamIndex);
	if (!BuildingId.IsNone())
	{
		Unlocks.Add(BuildingId);
	}
	if (!SelectionUIRowName.IsNone())
	{
		Unlocks.Add(SelectionUIRowName);
	}
}

int32 ASFGameState::GetMaxAllowedTierInCurrentStage(
	FName InBuildingId
) const
{
	if (InBuildingId.IsNone())
	{
		return 3;
	}

	for (const FSFStageBuildingRule& Rule
		: CurrentStageBuildingRules)
	{
		if (Rule.BuildingId == InBuildingId)
		{
			return Rule.MaxAllowedTier;
		}
	}

	// 규칙에 없는 건물은 최대 티어 3
	return 3;
}

int32 ASFGameState::GetMaxAllowedTierIndexInCurrentStage(
	FName InBuildingId
) const
{
	return FMath::Max(
		0,
		GetMaxAllowedTierInCurrentStage(InBuildingId) - 1);
}

bool ASFGameState::HasCompletedBuilding(
	int32 TeamIndex,
	FName InBuildingId
) const
{
	if (InBuildingId.IsNone())
	{
		return false;
	}

	if (const TSet<FName>* Unlocks = CompletedBuildingUnlocksByTeam.Find(TeamIndex))
	{
		if (Unlocks->Contains(InBuildingId))
		{
			return true;
		}
	}

	UWorld* World =
		GetWorld();

	if (!IsValid(World))
	{
		return false;
	}

	for (
		TActorIterator<ASFBaseBuilding> It(World);
		It;
		++It
		)
	{
		ASFBaseBuilding* Building =
			*It;

		if (!IsValid(Building))
		{
			continue;
		}

		if (Building->GetTeamIndex() != TeamIndex)
		{
			continue;
		}

		// 건설이 완전히 끝난 건물만 인정
		if (!Building->IsTierActivated())
		{
			continue;
		}

		USFBuildingDefinition* Definition =
			Building->GetBuildingDefinition();

		if (!IsValid(Definition))
		{
			continue;
		}

		const bool bMatchesSelectionRow =
			Definition->SelectionUIRowName ==
			InBuildingId;

		const bool bMatchesBuildingId =
			Definition->BuildingId ==
			InBuildingId;

		if (
			bMatchesSelectionRow ||
			bMatchesBuildingId
			)
		{
			RecordCompletedBuilding(TeamIndex, Definition->BuildingId,
				Definition->SelectionUIRowName);
			return true;
		}
	}

	return false;
}
