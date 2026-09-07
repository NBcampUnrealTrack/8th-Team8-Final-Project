// SFGameMode.cpp
#include "SFGameMode.h"
#include "Logging/SFLog.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "Datas/Building/SFBuildingCatalog.h"
#include "Game/Subsystem/SFDataTableManager.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "EngineUtils.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "Buildings/Building/HouseBuilding/Nexus/SFNexusBuilding.h"
#include "Datas/Building/SFBuildingDefinition.h"
#include "Game/Spawn/SFBuildingSpawnAnchor.h"
#include "Game/Spawn/SFBuildingSpawnLibrary.h"

#include "Game/Subsystem/SFBuildingAssetSubsystem.h"

ASFGameMode::ASFGameMode()
{
	// GameMode가 사용할 GameState 클래스 지정
	// World Setting에서 별도로 잘못 설정하는 상황 방지
	GameStateClass = ASFGameState::StaticClass();
}

void ASFGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	
	ASFInGamePlayerController* SFPC = Cast<ASFInGamePlayerController>(NewPlayer);
	if (!IsValid(SFPC))	return;

	const int32 AssignedTeamIndex =	PlayerTeamIndex;
	// Controller 자신에게 팀 번호 저장
	SFPC->SetMyTeamIndex(AssignedTeamIndex);
	
	ASFGameState* CurrentGameState = GetGameState<ASFGameState>();
	if (!IsValid(CurrentGameState))	return;

	// 이미 생성된 TeamState에 Controller 연결
	if (!CurrentGameState->RegisterPlayer(AssignedTeamIndex, SFPC))
	{
		SF_LOG(
			Error,
			"[SFGameMode] Player 팀 등록 실패. "
			"Team=%d, Player=%s",
			AssignedTeamIndex,
			*GetNameSafe(SFPC));
	}
}

void ASFGameMode::Logout(AController* Exiting)
{
	// 싱글이라 할필요가없긴함. 나가면 바로 종료라서
	// 애초에 컨트롤러가 나가면 Team정보의 Map 정보도 다 지워져야함
	ASFInGamePlayerController* SFPC = Cast<ASFInGamePlayerController>(Exiting);
	if (IsValid(SFPC))
	{
		if (ASFGameState* CurrentGameState = GetGameState<ASFGameState>())
		{
			CurrentGameState->UnregisterPlayer(SFPC->GetMyTeamIndex(), SFPC);
		}
	}
	
	Super::Logout(Exiting);
}

void ASFGameMode::InitGameState()
{
	Super::InitGameState();
	
	SFGameState =
		GetGameState<ASFGameState>();

	if (!IsValid(SFGameState))
	{
		SF_LOG(
			Error,
			"[SFGameMode] SFGameState를 찾을 수 없습니다.");

		return;
	}

	/*
	 * 비동기 로딩 중 PostLogin이 먼저 호출돼도
	 * 플레이어를 연결할 수 있도록 빈 TeamState를 준비한다.
	 */
	if (!IsValid(
		SFGameState->FindOrAddTeam(
			PlayerTeamIndex)))
	{
		SF_LOG(
			Error,
			"[SFGameMode] 플레이어 TeamState 생성 실패.");

		return;
	}
}

void ASFGameMode::BeginPlay()
{
	Super::BeginPlay();
	
	BeginStageBootstrap();
}

bool ASFGameMode::SpawnStageBuildingsFromAnchors()
{
	UWorld* World = GetWorld();

	if (!IsValid(World) ||
		!IsValid(SFGameState))
	{
		return false;
	}
	
	TArray<ASFBuildingSpawnAnchor*> Anchors;

	for (TActorIterator<ASFBuildingSpawnAnchor> It(World); It; ++It)
	{
		ASFBuildingSpawnAnchor* Anchor = *It;

		if (IsValid(Anchor))
		{
			Anchors.Add(Anchor);
		}
	}

	if (Anchors.IsEmpty())
	{
		SF_LOG(
			Error,
			"[SFGameMode] 레벨에 BuildingSpawnAnchor가 없습니다.");

		return false;
	}

	// 생성 전에 데이터 오류를 먼저 확인한다.
	TSet<int32> NexusTeams;

	for (const ASFBuildingSpawnAnchor* Anchor : Anchors)
	{
		if (Anchor->GetBuildingType() ==
			ESFBuildingType::None)
		{
			SF_LOG(
				Error,
				"[SFGameMode] BuildingType이 None인 Anchor가 있습니다. "
				"Anchor=%s",
				*GetNameSafe(Anchor));

			return false;
		}

		if (Anchor->GetBuildingType() == ESFBuildingType::Nexus)
		{
			const int32 TeamIndex =	Anchor->GetTeamIndex();

			if (NexusTeams.Contains(TeamIndex))
			{
				SF_LOG(
					Error,
					"[SFGameMode] Team %d의 Nexus Anchor가 중복됐습니다.",
					TeamIndex);

				return false;
			}

			NexusTeams.Add(TeamIndex);
		}
	}

	// Nexus를 우선 생성한다.
	for (const ASFBuildingSpawnAnchor* Anchor : Anchors)
	{
		if (Anchor->GetBuildingType() != ESFBuildingType::Nexus)
		{
			continue;
		}

		if (!SpawnBuildingAtAnchor(Anchor))
		{
			return false;
		}
	}

	// House, Mine, Production 등을 생성한다.
	for (const ASFBuildingSpawnAnchor* Anchor : Anchors)
	{
		if (Anchor->GetBuildingType() == ESFBuildingType::Nexus)
		{
			continue;
		}

		if (!SpawnBuildingAtAnchor(Anchor))
		{
			return false;
		}
	}
	
	// 완료하면 destory한다
	for (ASFBuildingSpawnAnchor* Anchor : Anchors)
	{
		Anchor->Destroy();
	}
	return true;
}

bool ASFGameMode::SpawnBuildingAtAnchor(const ASFBuildingSpawnAnchor* InAnchor)
{
	if (!IsValid(InAnchor) || !IsValid(SFGameState)) return false;

	const int32 TeamIndex = InAnchor->GetTeamIndex();
	const ESFBuildingType BuildingType = InAnchor->GetBuildingType();
	
	if (BuildingType == ESFBuildingType::None)
	{
		SF_LOG(Error, "[SFGameMode] SpawnAnchor의 BuildingType이 None입니다. " "Anchor=%s",*GetNameSafe(InAnchor));
		return false;
	}
	
	USFBuildingCatalog* Catalog = SFGameState->GetTeamBuildingCatalog(TeamIndex);
	if (!IsValid(Catalog))
	{
		SF_LOG(Error,"[SFGameMode] Team %d의 BuildingCatalog가 없습니다. "	"Anchor=%s",TeamIndex,*GetNameSafe(InAnchor));
		return false;
	}
	
	USFBuildingDefinition* Definition =	Catalog->FindLoadedDefinitionByType(BuildingType);
	if (!IsValid(Definition))
	{
		SF_LOG(Error,"[SFGameMode] BuildingType에 맞는 Definition이 없습니다. "
			"Team=%d, Type=%s, Catalog=%s",
			TeamIndex,
			*UEnum::GetValueAsString(BuildingType),
			*GetNameSafe(Catalog));

		return false;
	}
	
	FTransform SpawnTransform = InAnchor->GetActorTransform();
	/*
	 * Anchor Scale이 GridSize와 실제 메시 크기를 어긋나게 하지 않도록
	 * 런타임 건물은 항상 원래 Scale로 생성한다.
	 */
	SpawnTransform.SetScale3D(FVector::OneVector);
	
	ASFBaseBuilding* SpawnedBuilding =
		USFBuildingSpawnLibrary::
			SpawnBuildingFromDefinition(
				this,
				Definition,
				TeamIndex,
				SpawnTransform,
				ESFBuildingSpawnMode::ViewPortPlacedMode);
	if (!IsValid(SpawnedBuilding) ||
		SpawnedBuilding->IsActorBeingDestroyed())
	{
		SF_LOG(
			Error,
			"[SFGameMode] Anchor 건물 생성 실패. "
			"Anchor=%s, Team=%d, Type=%s",
			*GetNameSafe(InAnchor),
			TeamIndex,
			*UEnum::GetValueAsString(BuildingType));

		return false;
	}
	if (BuildingType == ESFBuildingType::Nexus &&
	!IsValid(SFGameState->GetNexus(TeamIndex)))
	{
		SF_LOG(
			Error,
			"[SFGameMode] 생성된 Nexus가 GameState에 등록되지 않았습니다. "
			"Team=%d, Definition=%s",
			TeamIndex,
			*GetNameSafe(Definition));

		SpawnedBuilding->Destroy();
		return false;
	}
	
	
	return true;
}

void ASFGameMode::BeginStageBootstrap()
{
	if (StageBootstrapState !=
		ESFStageBootstrapState::None)
	{
		return;
	}

	USFGameInstance* SFGameInstance =
		Cast<USFGameInstance>(
			GetGameInstance());

	if (!IsValid(SFGameInstance) ||
		!IsValid(SFGameState))
	{
		FailStageBootstrap();
		return;
	}

	BuildingAssetSubsystem =
		SFGameInstance
		->GetSubsystem<USFBuildingAssetSubsystem>();

	if (!IsValid(BuildingAssetSubsystem))
	{
		FailStageBootstrap();
		return;
	}

	BootstrapStageRowName =
		SFGameInstance
		->GetSelectedStageRowName();

	const ESFRace PlayerRace =
		SFGameInstance->GetSelectedRace();

	if (BootstrapStageRowName.IsNone() ||
		PlayerRace == ESFRace::None)
	{
		FailStageBootstrap();
		return;
	}

	USFDataTableManager* DataManager =
		SFGameInstance
		->GetSubsystem<USFDataTableManager>();

	if (!IsValid(DataManager))
	{
		FailStageBootstrap();
		return;
	}

	const FSFStageData* StageData =
		DataManager->GetDataRow<FSFStageData>(
			EDataTableType::Stage,
			BootstrapStageRowName,
			TEXT("BeginStageBootstrap"));

	if (!StageData)
	{
		FailStageBootstrap();
		return;
	}

	BootstrapTeamRaces.Reset();
	BootstrapTeamRaces.Add(
		PlayerTeamIndex,
		PlayerRace);

	for (const FSFStageTeamSetup& TeamSetup
		: StageData->TeamSetups)
	{
		if (TeamSetup.TeamIndex == PlayerTeamIndex ||
			TeamSetup.Race == ESFRace::None ||
			BootstrapTeamRaces.Contains(
				TeamSetup.TeamIndex))
		{
			FailStageBootstrap();
			return;
		}

		BootstrapTeamRaces.Add(
			TeamSetup.TeamIndex,
			TeamSetup.Race);
	}

	RequestRequiredRaceData();
}

void ASFGameMode::RequestRequiredRaceData()
{
	if (!IsValid(BuildingAssetSubsystem))
	{
		FailStageBootstrap();
		return;
	}

	TSet<ESFRace> RequiredRaces;

	for (const TPair<int32, ESFRace>& Team
		: BootstrapTeamRaces)
	{
		RequiredRaces.Add(Team.Value);
	}

	if (RequiredRaces.IsEmpty())
	{
		FailStageBootstrap();
		return;
	}

	StageBootstrapState =
		ESFStageBootstrapState::LoadingRaceData;

	PendingRaceRequestCount =
		RequiredRaces.Num();

	for (ESFRace Race : RequiredRaces)
	{
		BuildingAssetSubsystem
			->RequestRaceDataAsync(
				Race,
				FSFOnRaceBuildingDataReady::CreateUObject(
					this,
					&ThisClass::HandleRaceDataReady));
	}
}

void ASFGameMode::HandleRaceDataReady(ESFRace InRace, bool bSuccess)
{
	if (StageBootstrapState !=
		ESFStageBootstrapState::LoadingRaceData)
	{
		return;
	}

	if (!bSuccess)
	{
		SF_LOG(
			Error,
			"[SFGameMode] 종족 데이터 로드 실패. Race=%s",
			*UEnum::GetValueAsString(InRace));

		FailStageBootstrap();
		return;
	}

	--PendingRaceRequestCount;

	if (PendingRaceRequestCount > 0)
	{
		return;
	}

	for (const TPair<int32, ESFRace>& Team
		: BootstrapTeamRaces)
	{
		USFBuildingCatalog* Catalog =
			BuildingAssetSubsystem
			->GetLoadedCatalog(Team.Value);

		if (!IsValid(Catalog) ||
			!SFGameState->RegisterTeam(
				Team.Key,
				Team.Value,
				Catalog))
		{
			FailStageBootstrap();
			return;
		}
	}

	if (!SFGameState->InitializeStage(
		BootstrapStageRowName))
	{
		FailStageBootstrap();
		return;
	}

	RequestAnchorBuildingAssets();
}

void ASFGameMode::RequestAnchorBuildingAssets()
{
	UWorld* World = GetWorld();

	if (!IsValid(World) ||
		!IsValid(SFGameState) ||
		!IsValid(BuildingAssetSubsystem))
	{
		FailStageBootstrap();
		return;
	}

	TArray<USFBuildingDefinition*>
		RequiredDefinitions;

	int32 AnchorCount = 0;

	for (TActorIterator<ASFBuildingSpawnAnchor> It(World);
		It;
		++It)
	{
		const ASFBuildingSpawnAnchor* Anchor = *It;

		if (!IsValid(Anchor))
		{
			continue;
		}

		++AnchorCount;

		USFBuildingCatalog* Catalog =
			SFGameState->GetTeamBuildingCatalog(
				Anchor->GetTeamIndex());

		USFBuildingDefinition* Definition =
			IsValid(Catalog)
				? Catalog->FindLoadedDefinitionByType(
					Anchor->GetBuildingType())
				: nullptr;

		if (!IsValid(Definition))
		{
			FailStageBootstrap();
			return;
		}

		RequiredDefinitions.AddUnique(
			Definition);
	}

	if (AnchorCount == 0 ||
		RequiredDefinitions.IsEmpty())
	{
		FailStageBootstrap();
		return;
	}

	StageBootstrapState =
		ESFStageBootstrapState::LoadingAnchorAssets;

	PendingAnchorAssetRequestCount =
		RequiredDefinitions.Num();

	for (USFBuildingDefinition* Definition
		: RequiredDefinitions)
	{
		if (StageBootstrapState !=
			ESFStageBootstrapState::LoadingAnchorAssets)
		{
			return;
		}

		BuildingAssetSubsystem
			->RequestBuildingAssetsAsync(
				Definition,
				0,
				FSFOnBuildingAssetsReady::CreateUObject(
					this,
					&ThisClass::HandleAnchorAssetsReady));
	}
}

void ASFGameMode::HandleAnchorAssetsReady(USFBuildingDefinition* InDefinition, int32 InTierIndex, bool bSuccess)
{
	if (StageBootstrapState !=
		ESFStageBootstrapState::LoadingAnchorAssets)
	{
		return;
	}

	if (!bSuccess ||
		!IsValid(InDefinition) ||
		InTierIndex != 0)
	{
		FailStageBootstrap();
		return;
	}

	--PendingAnchorAssetRequestCount;

	if (PendingAnchorAssetRequestCount == 0)
	{
		CompleteStageBootstrap();
	}
}

void ASFGameMode::CompleteStageBootstrap()
{
	if (!SpawnStageBuildingsFromAnchors())
	{
		FailStageBootstrap();
		return;
	}

	StageBootstrapState =
		ESFStageBootstrapState::Ready;

	if (!StartStage())
	{
		FailStageBootstrap();
		return;
	}

	SF_LOG(
		Log,
		"[SFGameMode] 비동기 스테이지 초기화 완료.");
}

void ASFGameMode::FailStageBootstrap()
{
	if (StageBootstrapState ==
		ESFStageBootstrapState::Failed)
	{
		return;
	}

	StageBootstrapState =
		ESFStageBootstrapState::Failed;

	SF_LOG(
		Error,
		"[SFGameMode] 비동기 스테이지 초기화 실패.");
}

bool ASFGameMode::StartStage()
{
	if (!IsValid(SFGameState))
	{
		SF_LOG(Error, "[SFGameMode] StartStage failed: GameState is invalid.");

		return false;
	}

	if (!SFGameState->StartStage())
	{
		SF_LOG(Warning, "[SFGameMode] StartStage was rejected by GameState.");

		return false;
	}

	SF_SCREEN_LOG(Log, 5.0f, FColor::Green, "[SFGameMode] Stage started.");
	
	// 플레이어 초기자원 입력 - 디버그용(건물짓기, 유닛뽑기)
	SFGameState->AddResource(PlayerTeamIndex, EResourceType::Mineral, StartingMineral);
	SFGameState->AddResource(PlayerTeamIndex, EResourceType::Ether, StartingEther);
	return true;
}

bool ASFGameMode::ProcessCoreDestroyed(int32 DestroyedTeamIndex)
{
	if (DestroyedTeamIndex == INDEX_NONE)
	{
		SF_LOG(Warning, "[SFGameMode] ProcessCoreDestroyed failed: Destroyed Team Index is invalid.");

		return false;
	}

	SF_LOG(Log, "[SFGameMode] Core destroyed. DestroyedTeam: %d, PlayerTeam: %d", DestroyedTeamIndex, PlayerTeamIndex);

	if (DestroyedTeamIndex == PlayerTeamIndex)
	{
		return ProcessDefeat();
	}

	return ProcessVictory();
	
}

bool ASFGameMode::ProcessVictory()
{
	return FinishStage(ESFStageResult::Victory);
}

bool ASFGameMode::ProcessDefeat()
{
	return FinishStage(ESFStageResult::Defeat);
}

bool ASFGameMode::FinishStage(ESFStageResult NewStageResult)
{
	if (!IsValid(SFGameState))
	{
		SF_LOG(Error, "[SFGameMode] FinishStage failed: GameState is invalid.");

		return false;
	}

	if (!SFGameState->IsStagePlaying())
	{
		SF_LOG(Warning, "[SFGameMode] FinishStage failed: Stage is not playing.");

		return false;
	}

	if (NewStageResult == ESFStageResult::Undetermined)
	{
		SF_LOG(Warning, "[SFGameMode] Undetermined is not a valid final result.");

		return false;
	}

	// 26.08.11 추가
	if (!SFGameState->FinishStage(NewStageResult))
	{
		SF_LOG(Warning, "[SFGameMode] FinishStage was rejected by GameState.");

		return false;
	}

	if (NewStageResult == ESFStageResult::Victory)
	{
		USFGameInstance* SFGameInstance = Cast<USFGameInstance>(GetGameInstance());

		if (!IsValid(SFGameInstance))
		{
			SF_LOG(Warning, "[SFGameMode] Failed to record stage clear: GameInstance is invalid.");
		}
		else
		{
			const FName StageRowName = SFGameInstance->GetSelectedStageRowName();

			if (StageRowName.IsNone())
			{
				SF_LOG(Warning, "[SFGameMode] Failed to record stage clear : Selected Stage Row Name is None.");
			}
			else if (!SFGameInstance->MarkStageCleared(StageRowName))
			{
				SF_LOG(Warning, "[SFGameMode] Failed to record stage clear: %s", *StageRowName.ToString());
			}
		}
	}

	const FColor ResultColor = NewStageResult == ESFStageResult::Victory ? FColor::Green : FColor::Red;

	SF_SCREEN_LOG(Log, 5.0f, ResultColor, "[SFGameMode] Stage finished. Result: %s", *UEnum::GetValueAsString(NewStageResult));

	return true;
}

ASFGameState* ASFGameMode::GetSFGameState() const
{
	return SFGameState;
}

// 26.08.18
// ASFNexusBuilding* ASFGameMode::SpawnNexus(FName NexusRowName, const FTransform& SpawnTransform, int32 TeamIndex)
// {
 //    if (NexusRowName.IsNone())
 //    {
 //        SF_LOG(Warning, "[SFGameMode] SpawnNexus failed: Nexus RowName is None.");
 //
 //        return nullptr;
 //    }
 //
 //    if (TeamIndex == INDEX_NONE)
 //    {
 //        SF_LOG(Warning, "[SFGameMode] SpawnNexus failed: TeamIndex is invalid.");
 //
 //        return nullptr;
 //    }
 //
 //    UGameInstance* GameInstance = GetGameInstance();
 //
 //    if (!IsValid(GameInstance))
 //    {
 //        SF_LOG(Error, "[SFGameMode] SpawnNexus failed: GameInstance is invalid.");
 //
 //        return nullptr;
 //    }
 //
 //    USFDataTableManager* DataManager = GameInstance->GetSubsystem<USFDataTableManager>();
 //
 //    if (!IsValid(DataManager))
 //    {
 //        SF_LOG(Error, "[SFGameMode] SpawnNexus failed: DataTableManager is invalid.");
 //
 //        return nullptr;
 //    }
 //
 //    const FBuildingAppearance* NexusData = DataManager->GetDataRow<FBuildingAppearance>(EDataTableType::Building, NexusRowName, TEXT("SpawnNexus"));
 //
 //    if (NexusData == nullptr)
 //    {
 //        SF_LOG(Warning, "[SFGameMode] SpawnNexus failed: Row was not found. Row: %s", *NexusRowName.ToString());
 //
 //        return nullptr;
 //    }
 //
 //    UClass* NexusClass = NexusData->BuildingClass.LoadSynchronous();
 //
 //    if (!IsValid(NexusClass))
 //    {
 //        SF_LOG(Warning, "[SFGameMode] SpawnNexus failed: BuildingClass is invalid. Row: %s", *NexusRowName.ToString());
 //
 //        return nullptr;
 //    }
 //
 //    if (!NexusClass->IsChildOf(ASFNexusBuilding::StaticClass()))
 //    {
 //        SF_LOG(Warning, "[SFGameMode] SpawnNexus failed: BuildingClass is not an SFNexusBuilding. Class: %s", *GetNameSafe(NexusClass));
 //
 //        return nullptr;
 //    }
 //
 //    UWorld* World = GetWorld();
 //
 //    if (!IsValid(World))
 //    {
 //        SF_LOG(Error, "[SFGameMode] SpawnNexus failed: World is invalid.");
 //
 //        return nullptr;
 //    }
 //
 //    ASFNexusBuilding* SpawnedNexus = World->SpawnActorDeferred<ASFNexusBuilding>(NexusClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
 //
 //    if (!IsValid(SpawnedNexus))
 //    {
 //        SF_LOG(Error, "[SFGameMode] SpawnNexus failed: SpawnActorDeferred failed.");
 //
 //        return nullptr;
 //    }
 //
 //    // BeginPlay 전에 데이터테이블 RowName 설정
 //    SpawnedNexus->SetRowName(NexusRowName);
	// SpawnedNexus->SetSpawnMode(ESFBuildingSpawnMode::ViewPortPlacedMode);
 //    // BeginPlay 전에 전투 팀 설정
 //    UCombatTargetComponent* CombatTarget = SpawnedNexus->FindComponentByClass<UCombatTargetComponent>();
 //
 //    if (!IsValid(CombatTarget))
 //    {
 //        SF_LOG(Error, "[SFGameMode] SpawnNexus failed: CombatTargetComponent is invalid.");
 //
 //        SpawnedNexus->Destroy();
 //
 //        return nullptr;
 //    }
 //
 //    CombatTarget->SetTeamIndex(TeamIndex);
 //
 //    // 여기서 BeginPlay가 실행됨
 //    SpawnedNexus->FinishSpawning(SpawnTransform);
 //
 //    if (!IsValid(SFGameState))
 //    {
 //        SFGameState = GetGameState<ASFGameState>();
 //    }
 //
 //    if (!IsValid(SFGameState))
 //    {
 //        SF_LOG(Error, "[SFGameMode] SpawnNexus failed: SFGameState is invalid.");
 //
 //        SpawnedNexus->Destroy();
 //
 //        return nullptr;
 //    }
 //
 //    if (!SFGameState->RegisterNexus(TeamIndex, SpawnedNexus))
 //    {
 //        SF_LOG(Error, "[SFGameMode] SpawnNexus failed: Nexus registration failed.");
 //
 //        SpawnedNexus->Destroy();
 //
 //        return nullptr;
 //    }
 //
 //    SF_LOG(Log, "[SFGameMode] Nexus spawned. Team: %d, Row: %s, Nexus: %s", TeamIndex, *NexusRowName.ToString(), *GetNameSafe(SpawnedNexus));
 //
 //    return SpawnedNexus;
// }
