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

	SFGameState = GetGameState<ASFGameState>();

	if (!IsValid(SFGameState))
	{
		return;
	}

	if (!IsValid(SFGameState->FindOrAddTeam(PlayerTeamIndex)))
	{
		return;
	}
}

void ASFGameMode::BeginPlay()
{
	Super::BeginPlay();
	
	// initgamestate에서 플레이어팀등록을 마친뒤 호출
	BeginStageBootstrap();
}

bool ASFGameMode::SpawnStageBuildingsFromAnchors()
{
	UWorld* World = GetWorld();

	if (!IsValid(World) || !IsValid(SFGameState))
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
		if (Anchor->GetBuildingType() == ESFBuildingType::None)
		{
			SF_LOG(Error,
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
				SF_LOG(Error,
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
				ESFBuildingSpawnMode::ViewPortPlacedMode,
				InAnchor->GetInitialTierIndex());
	
	if (!IsValid(SpawnedBuilding) ||
		SpawnedBuilding->IsActorBeingDestroyed())
	{
		SF_LOG(Error,
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
		SF_LOG(Error,
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
	if (!IsValid(SFGameState))
    {
        SF_LOG(Error,
            "[SFGameMode] Stage Bootstrap을 시작할 GameState가 없습니다.");

        return;
    }

    if (SFGameState->GetStageState() != ESFStageState::Initializing)
    {
        return;
    }

    /*
     * 먼저 상태를 변경하여 BeginStageBootstrap 중복 호출을 막는다.
     * 이후 검증에 실패하면 InitializationFailed로 전환한다.
     */
    if (!SFGameState->BeginRaceDataLoading())
    {
        FailStageBootstrap();
        return;
    }

    USFGameInstance* SFGameInstance = Cast<USFGameInstance>(GetGameInstance());
    if (!IsValid(SFGameInstance))
    {
        FailStageBootstrap();
        return;
    }

    // 비동기 에셋 로딩 Subsystem을 캐싱한다.
    BuildingAssetSubsystem = SFGameInstance->GetSubsystem<USFBuildingAssetSubsystem>();
    if (!IsValid(BuildingAssetSubsystem))
    {
        FailStageBootstrap();
        return;
    }

	// StageTable의 RowName
    BootstrapStageRowName = SFGameInstance->GetSelectedStageRowName();
	// 플레이어가 선택한 종족
    const ESFRace PlayerRace = SFGameInstance->GetSelectedRace();

    if (BootstrapStageRowName.IsNone() ||
        PlayerRace == ESFRace::None)
    {
        FailStageBootstrap();
        return;
    }

	// 테이블 매니저
    USFDataTableManager* DataManager = SFGameInstance->GetSubsystem<USFDataTableManager>();
    if (!IsValid(DataManager))
    {
        FailStageBootstrap();
        return;
    }

	// GameInstance와 연계
	// 테이블의 현재 RowName으로 구조체 정보 추출
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
    BootstrapTeamRaces.Add(PlayerTeamIndex, PlayerRace);

	// 테이블의 Enemy Setup 불러와서 등록
    for (const FSFStageTeamSetup& TeamSetup : StageData->TeamSetups)
    {
    	if (TeamSetup.TeamIndex == PlayerTeamIndex ||
			TeamSetup.Race == ESFRace::None ||
			BootstrapTeamRaces.Contains(TeamSetup.TeamIndex))
        {
            FailStageBootstrap();
            return;
        }
		// 적Enemy 팀번호와 종족등록
        BootstrapTeamRaces.Add(TeamSetup.TeamIndex, TeamSetup.Race);
    }

    RequestRequiredRaceData();
}

void ASFGameMode::RequestRequiredRaceData()
{
	if (!IsValid(SFGameState) ||
		!IsValid(BuildingAssetSubsystem))
	{
		FailStageBootstrap();
		return;
	}

	if (SFGameState->GetStageState() != ESFStageState::LoadingRaceData)
	{
		return;
	}

	// 공통 종족 필터
	TSet<ESFRace> RequiredRaces;
	for (const TPair<int32, ESFRace>& Team : BootstrapTeamRaces)
	{
		RequiredRaces.Add(Team.Value);
	}

	if (RequiredRaces.IsEmpty())
	{
		FailStageBootstrap();
		return;
	}
	
	// 현재 게임상태 State = 로딩중...
	// StageBootstrapState = ESFStageBootstrapState::LoadingRaceData;
	// 로딩 갯수
	TotalRaceRequestCount = RequiredRaces.Num();
	// 임시저장 ( 완료될때마다 줄어들거임 )
	PendingRaceRequestCount = TotalRaceRequestCount;
	// 로딩 UI 진행도
	SFGameState->SetStageLoadingProgress(0.f);

	for (ESFRace Race : RequiredRaces)
	{
		// 종족안의 건물정의들 비동기 로딩 (DA -> DA)
		BuildingAssetSubsystem
			->RequestRaceDataAsync(
				Race,
				// 에셋매니저를 인클루드했기때문에 쓸수있는 델리게이트
				FSFOnRaceBuildingDataReady::CreateUObject(
					this,
					&ThisClass::HandleRaceDataReady));
	}	// 카탈로그 로딩 ->
		// 카탈로그안 건물Def로딩 ->
		// UI용 이미지 로딩 ->
		// 완료이벤트 = HandleRaceDataReady 실행
}

void ASFGameMode::HandleRaceDataReady(ESFRace InRace, bool bSuccess)
{
	// 비동기 로딩완료이후 종족 + 성공,실패여부와 함께 실행되는 콜백함수 시작
	// 이함수가 실행될때는 종족카탈로그, 종족건물
	if (!IsValid(SFGameState) ||
		SFGameState->GetStageState() !=	ESFStageState::LoadingRaceData)
	{
		return;
	}
	
	if (!bSuccess)
	{	// Catalog의 배열중 하나라도 에셋이 비어있으면 로딩 실패
		SF_LOG(Error, "[SFGameMode] 종족 데이터 로드 실패. Race=%s",
			*UEnum::GetValueAsString(InRace));

		FailStageBootstrap();
		return;
	}

	if (PendingRaceRequestCount <= 0)
	{
		FailStageBootstrap();
		return;
	}
	
	// 종족하나 로딩완료
	--PendingRaceRequestCount;
	
	UpdateStageLoadingProgress();

	if (PendingRaceRequestCount > 0)
	{
		return;
	}

	// 종족 로딩 전부 완료
	
	for (const TPair<int32, ESFRace>& Team : BootstrapTeamRaces)
	{	// Soft Catalog에서 TObject로 로딩완료
		// 카탈로그는 에셋매니저가 관리한다 
		USFBuildingCatalog* Catalog =
			BuildingAssetSubsystem->GetLoadedCatalog(Team.Value);

		if (!IsValid(Catalog) ||
			!SFGameState->RegisterTeam(Team.Key, Team.Value, Catalog))
		{
			FailStageBootstrap();
			return;
		}
	}

	if (!SFGameState->InitializeStage(BootstrapStageRowName))
	{
		FailStageBootstrap();
		return;
	}

	RequestInitialBuildingAssets();
}

void ASFGameMode::RequestInitialBuildingAssets()
{
	UWorld* World = GetWorld();

    if (!IsValid(World) ||
        !IsValid(SFGameState) ||
        !IsValid(BuildingAssetSubsystem))
    {
        FailStageBootstrap();
        return;
    }

	/*
	* 하나의 Definition에서 여러 Tier가 필요할 수 있다.
	*
	* 예:
	* Nexus -> Tier 1, Tier 2
	* House -> Tier 0
	*/
	TMap<USFBuildingDefinition*, TSet<int32>>RequiredTiersByDefinition;

    auto AddRequiredTier =
        [&RequiredTiersByDefinition](
            USFBuildingDefinition* Definition,
            int32 TierIndex) -> bool
        {
            if (!IsValid(Definition) ||
                TierIndex < 0 ||
                !Definition->FindTier(TierIndex))
            {
                return false;
            }

            RequiredTiersByDefinition
                .FindOrAdd(Definition)
                .Add(TierIndex);

            return true;
        };

    /*
     * 1. 모든 팀의 건설 UI용 건물은 Tier 0을 준비한다.
     */
    for (const TPair<int32, ESFRace>& Team
        : BootstrapTeamRaces)
    {
        USFBuildingCatalog* Catalog =
            SFGameState->GetTeamBuildingCatalog(
                Team.Key);

        if (!IsValid(Catalog))
        {
            FailStageBootstrap();
            return;
        }

        for (const TSoftObjectPtr<
            USFBuildingDefinition>& DefinitionRef
            : Catalog->BuildableDefinitions)
        {
            USFBuildingDefinition* Definition =
                DefinitionRef.Get();

            if (!AddRequiredTier(Definition, 0))
            {
                SF_LOG(
                    Error,
                    "[SFGameMode] Buildable Tier 0 "
                    "수집 실패. Team=%d, Asset=%s",
                    Team.Key,
                    *DefinitionRef
                        .ToSoftObjectPath()
                        .ToString());

                FailStageBootstrap();
                return;
            }
        }
    }

    /*
     * 2. Anchor 건물은 시작 Tier와 다음 Tier를 준비한다.
     */
    int32 AnchorCount = 0;

    for (TActorIterator<
        ASFBuildingSpawnAnchor> It(World);
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

        const int32 InitialTier =
            Anchor->GetInitialTierIndex();

        // Anchor가 실제로 생성될 Tier이므로 반드시 존재해야 한다.
        if (!AddRequiredTier(
            Definition,
            InitialTier))
        {
            SF_LOG(
                Error,
                "[SFGameMode] Anchor 시작 Tier가 "
                "유효하지 않습니다. "
                "Anchor=%s, Tier=%d",
                *GetNameSafe(Anchor),
                InitialTier);

            FailStageBootstrap();
            return;
        }

        /*
         * 다음 Tier는 존재할 때만 추가한다.
         * 마지막 Tier라면 오류가 아니다.
         */
        const int32 NextTier =
            InitialTier + 1;

        if (Definition->FindTier(NextTier))
        {
            AddRequiredTier(
                Definition,
                NextTier);
        }
    }

    if (AnchorCount == 0 ||
        RequiredTiersByDefinition.IsEmpty())
    {
        FailStageBootstrap();
        return;
    }

    int32 RequestCount = 0;

    for (const TPair<
        USFBuildingDefinition*,
        TSet<int32>>& Pair
        : RequiredTiersByDefinition)
    {
        RequestCount += Pair.Value.Num();
    }

	TotalInitialBuildingAssetRequestCount = RequestCount;
	PendingInitialBuildingAssetRequestCount = RequestCount;

	if (!SFGameState->BeginInitialBuildingAssetsLoading())
	{
		FailStageBootstrap();
		return;
	}

	UpdateStageLoadingProgress();
    /*
     * 콜백이 즉시 실행될 수 있으므로
     * 카운터 설정 후 요청을 시작한다.
     */
    for (const TPair<USFBuildingDefinition*, TSet<int32>>& Pair
						: RequiredTiersByDefinition)
    {
        USFBuildingDefinition* Definition = Pair.Key;

        for (const int32 TierIndex : Pair.Value)
        {
        	if (!IsValid(SFGameState) ||
				SFGameState->GetStageState() !=	ESFStageState::LoadingInitialBuildingAssets)
        	{
        		return;
        	}

            BuildingAssetSubsystem
                ->RequestBuildingAssetsAsync(
                    Definition,
                    TierIndex,
                    FSFOnBuildingAssetsReady::
                    CreateUObject(
                        this,
                        &ThisClass::
                        HandleInitialBuildingAssetsReady));
        }
    }
}

void ASFGameMode::HandleInitialBuildingAssetsReady(USFBuildingDefinition* InDefinition, int32 InTierIndex, bool bSuccess)
{
	if (!IsValid(SFGameState) ||
		SFGameState->GetStageState() != ESFStageState::LoadingInitialBuildingAssets)
	{
		return;
	}

	if (!bSuccess ||
	!IsValid(InDefinition) ||
	InTierIndex < 0 ||
	!InDefinition->FindTier(InTierIndex))
	{
		SF_LOG(
			Error,
			"[SFGameMode] 초기 건물 에셋 로딩 실패. "
			"Definition=%s, Tier=%d",
			*GetNameSafe(InDefinition),
			InTierIndex);

		FailStageBootstrap();
		return;
	}

	if (PendingInitialBuildingAssetRequestCount <= 0)
	{
		SF_LOG(
			Error,
			"[SFGameMode] 초기 Tier 0 요청 카운터가 "
			"잘못된 상태입니다.");

		FailStageBootstrap();
		return;
	}

	--PendingInitialBuildingAssetRequestCount;

	UpdateStageLoadingProgress();
	
	SF_LOG(
		Log,
		"[SFGameMode] 초기 Tier 준비 완료. "
		"Definition=%s, Tier=%d, Remaining=%d",
		*GetNameSafe(InDefinition),
		InTierIndex,
		PendingInitialBuildingAssetRequestCount);

	if (PendingInitialBuildingAssetRequestCount == 0)
	{
		CompleteStageBootstrap();
	}
}

void ASFGameMode::CompleteStageBootstrap()
{
	if (!IsValid(SFGameState) ||
		SFGameState->GetStageState() != ESFStageState::LoadingInitialBuildingAssets)
	{
		return;
	}

	if (!SpawnStageBuildingsFromAnchors())
	{
		FailStageBootstrap();
		return;
	}

	SFGameState->SetStageLoadingProgress(1.f);

	if (!SFGameState->MarkStageReadyToStart())
	{
		FailStageBootstrap();
		return;
	}

	// 필요하면 이곳에서 UI 미응답용 비상 타이머를 시작한다.
}

void ASFGameMode::FailStageBootstrap()
{
	if (!IsValid(SFGameState))
	{
		SF_LOG(Error,
			"[SFGameMode] 비동기 스테이지 초기화 실패. "
			"GameState가 유효하지 않습니다.");

		return;
	}

	if (SFGameState->GetStageState() ==	ESFStageState::InitializationFailed)
	{
		return;
	}

	SF_LOG(Error, "[SFGameMode] 비동기 스테이지 초기화 실패.");

	if (!SFGameState->FailStageInitialization())
	{
		SF_LOG(Error,
			"[SFGameMode] GameState가 초기화 실패 상태 전환을 거부했습니다.");
	}
}

void ASFGameMode::UpdateStageLoadingProgress()
{
	if (!IsValid(SFGameState))
	{
		return;
	}

	// 종족 진행도
	const float RaceDataProgress =
		TotalRaceRequestCount > 0
			? 1.f -
				static_cast<float>(PendingRaceRequestCount) /
				static_cast<float>(TotalRaceRequestCount)
			: 0.f;
	// 종족 안 건물 진행도
	const float InitialBuildingAssetProgress =
		TotalInitialBuildingAssetRequestCount > 0
			? 1.f -
				static_cast<float>(PendingInitialBuildingAssetRequestCount) /
				static_cast<float>(TotalInitialBuildingAssetRequestCount)
			: 0.f;
	
	
	constexpr float RaceDataWeight = 0.30f;
	constexpr float InitialBuildingAssetWeight = 0.65f;

	// 비율계산
	const float TotalProgress = FMath::Clamp(
		RaceDataProgress * RaceDataWeight +
		InitialBuildingAssetProgress *
			InitialBuildingAssetWeight,
		0.f,
		0.95f);

	SFGameState->SetStageLoadingProgress(TotalProgress);
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

void ASFGameMode::StartStageAfterLoadingScreen()
{
	if (!IsValid(SFGameState) ||
		SFGameState->GetStageState() != ESFStageState::ReadyToStart)
	{
		return;
	}

	if (!StartStage())
	{
		FailStageBootstrap();
		return;
	}

	SF_LOG(Log,
		"[SFGameMode] 로딩 화면 종료 후 스테이지를 시작했습니다.");
}

ASFGameState* ASFGameMode::GetSFGameState() const
{
	return SFGameState;
}
