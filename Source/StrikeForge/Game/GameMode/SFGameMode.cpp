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
#include "Units/Spawner/UnitSpawner.h"

#include "Game/Subsystem/SFBuildingAssetSubsystem.h"
#include "Game/Subsystem/SFBuildingSynergySubsystem.h"
#include "Misc/ScopeExit.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "Components/AudioComponent.h"
#include "Sound/AmbientSound.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundWave.h"
#include "UObject/ConstructorHelpers.h"

ASFGameMode::ASFGameMode()
{
	// GameMode가 사용할 GameState 클래스 지정
	// World Setting에서 별도로 잘못 설정하는 상황 방지
	GameStateClass = ASFGameState::StaticClass();

	StageMusicComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("StageMusic"));
	SetRootComponent(StageMusicComponent);
	StageMusicComponent->bAutoActivate = false;
	StageMusicComponent->bAutoDestroy = false;
	StageMusicComponent->bAllowSpatialization = false;
	StageMusicComponent->SetUISound(true);
	static ConstructorHelpers::FObjectFinder<USoundCue> DefaultStageMusic(
		TEXT("/Game/Sound/BGM/StageBgm/Stage1_Cue.Stage1_Cue"));
	StageMusicComponent->SetSound(DefaultStageMusic.Object);
}

void ASFGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	ASFInGamePlayerController* SFPC = Cast<ASFInGamePlayerController>(NewPlayer);
	if (!IsValid(SFPC))	return;

	const int32 AssignedTeamIndex = PlayerTeamIndex;
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
	// 다른 레벨 액터의 컴포넌트가 자동 활성화되기 전에 BGM을 맡는다.
	PrepareStageMusic();

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

void ASFGameMode::PrepareStageMusic()
{
	bool bFoundLevelMusic = false;
	USoundClass* DefaultMusicClass = StageMusicComponent->Sound
		? StageMusicComponent->Sound->GetSoundClass() : nullptr;
	for (TActorIterator<AAmbientSound> It(GetWorld()); It; ++It)
	{
		UAudioComponent* Audio = It->GetAudioComponent();
		USoundBase* Sound = IsValid(Audio) ? Audio->Sound.Get() : nullptr;
		if (!IsValid(Sound) || !Sound->GetPathName().StartsWith(TEXT("/Game/Sound/BGM/")))
		{
			continue;
		}

		if (!bFoundLevelMusic)
		{
			StageMusicComponent->SetSound(Sound);
			StageMusicComponent->SetVolumeMultiplier(Audio->VolumeMultiplier);
			StageMusicComponent->SetPitchMultiplier(Audio->PitchMultiplier);
			bFoundLevelMusic = true;
		}
		Audio->bAutoActivate = false;
		Audio->Stop();
	}

	// 원본 SoundWave를 변경하지 않고 오디오 엔진에서 끊김 없이 반복한다.
	if (USoundWave* Wave = Cast<USoundWave>(StageMusicComponent->Sound))
	{
		USoundCue* LoopingMusic = NewObject<USoundCue>(this);
		USoundNodeWavePlayer* Player = NewObject<USoundNodeWavePlayer>(LoopingMusic);
		Player->SetSoundWave(Wave);
		Player->bLooping = true;
		LoopingMusic->FirstNode = Player;
		// 패키징된 게임에서도 루프 지속 시간이 0으로 남지 않도록 설정한다.
		LoopingMusic->Duration = INDEFINITELY_LOOPING_DURATION;
		LoopingMusic->VolumeMultiplier = 1.f;
		LoopingMusic->SoundClassObject = Wave->GetSoundClass() ? Wave->GetSoundClass() : DefaultMusicClass;
		LoopingMusic->CacheAggregateValues();
		StageMusicComponent->SetSound(LoopingMusic);
	}
	StageMusicComponent->OnAudioFinishedNative.AddUObject(this, &ThisClass::HandleStageMusicFinished);
}

void ASFGameMode::StartStageMusic()
{
	if (bStageMusicStarted || !IsValid(StageMusicComponent->Sound)) return;
	bStageMusicStarted = true;
	StageMusicComponent->SetUISound(true);
	StageMusicComponent->Play();
	SF_LOG(Log, "[SFGameMode] Stage music started after loading screen.");
}

void ASFGameMode::HandleStageMusicFinished(UAudioComponent* FinishedComponent)
{
	// 루프가 없는 SoundCue를 지정해도 같은 곡을 다시 재생한다.
	if (bStageMusicStarted && FinishedComponent == StageMusicComponent && !IsActorBeingDestroyed())
	{
		StageMusicComponent->Play();
	}
}

void ASFGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bStageMusicStarted = false;
	StageMusicComponent->OnAudioFinishedNative.RemoveAll(this);
	StageMusicComponent->Stop();
	Super::EndPlay(EndPlayReason);
}

bool ASFGameMode::SpawnStageBuildingsFromAnchors()
{
	// 선배치 건물은 등록만 모아 두고, 생성이 모두 끝난 뒤 시너지를 한 번만 계산합니다.
	// 아래의 어떤 return 경로로 나가도 ON_SCOPE_EXIT가 EndUpdateBatch를 호출합니다.
	USFBuildingSynergySubsystem* Synergy = USFBuildingSynergySubsystem::Get(this);
	if (IsValid(Synergy))
	{
		Synergy->BeginUpdateBatch();
	}

	ON_SCOPE_EXIT
	{
		if (IsValid(Synergy))
		{
			Synergy->EndUpdateBatch();
		}
	};

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
			SF_LOG(Warning,
				"[SFGameMode] 미설정 Anchor를 건너뜁니다. "
				"Anchor=%s",
				*GetNameSafe(Anchor));

			continue;
		}

		if (Anchor->GetBuildingType() == ESFBuildingType::Nexus)
		{
			const int32 TeamIndex = Anchor->GetTeamIndex();

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

	int32 SpawnedBuildingCount = 0;
	int32 SkippedAnchorCount = 0;

	// Nexus는 스테이지 진행에 필수이므로 기존처럼 실패 처리한다.
	for (const ASFBuildingSpawnAnchor* Anchor : Anchors)
	{
		if (Anchor->GetBuildingType() != ESFBuildingType::Nexus)
		{
			continue;
		}

		if (!SpawnBuildingAtAnchor(Anchor))
		{
			SF_LOG(Error, "[SFGameMode] 필수 Nexus Anchor 생성 실패. Anchor=%s",
				*GetNameSafe(Anchor));

			return false;
		}

		++SpawnedBuildingCount;
	}

	// House, Mine, Production 등은 실패한 Anchor만 건너뛴다.
	for (const ASFBuildingSpawnAnchor* Anchor : Anchors)
	{
		if (Anchor->GetBuildingType() == ESFBuildingType::None)
		{
			++SkippedAnchorCount;
			continue;
		}

		if (Anchor->GetBuildingType() == ESFBuildingType::Nexus)
		{
			continue;
		}

		if (!SpawnBuildingAtAnchor(Anchor))
		{
			++SkippedAnchorCount;

			SF_LOG(Warning,
				"[SFGameMode] Anchor 건물을 건너뜁니다. "
				"Anchor=%s, Team=%d, Type=%s",
				*GetNameSafe(Anchor), Anchor->GetTeamIndex(), *UEnum::GetValueAsString(Anchor->GetBuildingType()));

			continue;
		}

		++SpawnedBuildingCount;
	}

	// 성공 여부와 관계없이 런타임 Anchor는 정리한다.
	for (ASFBuildingSpawnAnchor* Anchor : Anchors)
	{
		if (IsValid(Anchor))
		{
			Anchor->Destroy();
		}
	}

	SF_LOG(Log, "[SFGameMode] Anchor 생성 완료. 성공=%d, 건너뜀=%d",
		SpawnedBuildingCount, SkippedAnchorCount);

	return SpawnedBuildingCount > 0;
}

bool ASFGameMode::SpawnBuildingAtAnchor(const ASFBuildingSpawnAnchor* InAnchor)
{
	if (!IsValid(InAnchor) || !IsValid(SFGameState)) return false;

	const int32 TeamIndex = InAnchor->GetTeamIndex();
	const ESFBuildingType BuildingType = InAnchor->GetBuildingType();

	if (BuildingType == ESFBuildingType::None)
	{
		SF_LOG(Error, "[SFGameMode] SpawnAnchor의 BuildingType이 None입니다. " "Anchor=%s", *GetNameSafe(InAnchor));
		return false;
	}

	USFBuildingCatalog* Catalog = SFGameState->GetTeamBuildingCatalog(TeamIndex);
	if (!IsValid(Catalog))
	{
		SF_LOG(Error, "[SFGameMode] Team %d의 BuildingCatalog가 없습니다. "	"Anchor=%s", TeamIndex, *GetNameSafe(InAnchor));
		return false;
	}

	USFBuildingDefinition* Definition = Catalog->FindLoadedDefinitionByType(BuildingType);
	if (!IsValid(Definition))
	{
		SF_LOG(Error, "[SFGameMode] BuildingType에 맞는 Definition이 없습니다. "
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

	// 앵커는 곧 제거된다. 그 전에 같은 위치의 스포너를 실제 생산 건물에
	// 연결해 두어야 로딩이 길어져도 파괴 신호를 놓치지 않는다.
	if (BuildingType == ESFBuildingType::Barracks ||
		BuildingType == ESFBuildingType::RangedProduction ||
		BuildingType == ESFBuildingType::SpecialProduction)
	{
		constexpr float SpawnerAnchorRadius = 150.f;
		for (TActorIterator<AUnitSpawner> It(GetWorld()); It; ++It)
		{
			AUnitSpawner* Spawner = *It;
			if (IsValid(Spawner) &&
				FVector::DistSquared2D(Spawner->GetActorLocation(), InAnchor->GetActorLocation()) <=
				FMath::Square(SpawnerAnchorRadius))
			{
				Spawner->AssignOwnerBuilding(SpawnedBuilding);
			}
		}
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
#if WITH_EDITOR
	SFGameInstance->ResolveEditorStageSelection(GetWorld());
#endif
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

	BootstrapBuildingRules = StageData->BuildingRules;
	BootstrapStartingMineral = FMath::Max(0, StageData->StartingMineral);
	BootstrapStartingEther = FMath::Max(0, StageData->StartingEther);
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
		SFGameState->GetStageState() != ESFStageState::LoadingRaceData)
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

	if (!SFGameState->InitializeStage(BootstrapStageRowName, BootstrapBuildingRules))
	{
		FailStageBootstrap();
		return;
	}

	// 30% Loading Completed Timer 1.f
	GetWorldTimerManager().SetTimer(
		Stage30PercentDelayTimerHandle,
		this,
		&ThisClass::RequestInitialBuildingAssets,
		Stage30PercentDelayTime,
		false);

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
	* 스테이지 시작 전에 이 스테이지에서 쓸 수 있는 모든 Tier를 전부 로딩한다.
	*
	* 로딩 범위: Tier 0(또는 Anchor 시작 Tier)부터
	*           스테이지 테이블(DT_SFStage)의 BuildingRules.MaxAllowedTier까지.
	*
	* 로딩 화면에서 비동기 로딩을 모두 끝내므로
	* 게임 도중(건설 시작, 업그레이드)에는 비동기 요청을 하지 않는다.
	* 이전에는 건설이 시작될 때 다음 Tier를 선로딩했지만,
	* 그 선로딩이 게임 도중 게임 스레드를 막는 것이 트레이스로 확인되어 이 방식으로 바꿨다.
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
	 * FirstTier 다음 Tier부터 스테이지 최대 Tier까지 존재하는 Tier를 모두 추가한다.
	 * 마지막 Tier를 넘어가면 오류가 아니라 거기서 멈춘다.
	 */
	auto AddUpgradeTiersUpToStageMax =
		[this, &AddRequiredTier](
			USFBuildingDefinition* Definition,
			int32 FirstTier)
		{
			if (!IsValid(Definition))
			{
				return;
			}

			const int32 StageMaxTierIndex =
				SFGameState->GetMaxAllowedTierIndexInCurrentStage(
					Definition->BuildingId);

			for (int32 TierIndex = FirstTier + 1;
				TierIndex <= StageMaxTierIndex;
				++TierIndex)
			{
				if (!AddRequiredTier(Definition, TierIndex))
				{
					break;
				}
			}
		};

	/*
	 * 1. 모든 팀의 건설 UI용 건물은 Tier 0부터 스테이지 최대 Tier까지 준비한다.
	 */
	for (const TPair<int32, ESFRace>& Team
		: BootstrapTeamRaces)
	{
		USFBuildingCatalog* Catalog =
			SFGameState->GetTeamBuildingCatalog(Team.Key);

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

			AddUpgradeTiersUpToStageMax(Definition, 0);
		}
	}

	/*
	 * 2. Anchor 건물은 시작 Tier부터 스테이지 최대 Tier까지 준비한다.
	 * 넥서스는 BuildableDefinitions에 없고 Anchor로만 들어오므로 여기서 처리된다.
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

		// Level authors may keep unconfigured placement markers while dressing a map.
		// They request no assets and must not abort otherwise valid Nexus setup.
		if (Anchor->GetBuildingType() == ESFBuildingType::None)
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
		 * [이전 방식] 다음 Tier 하나만 준비하고, 그 이후 Tier는 건설 시작 때 비동기로 선로딩했다.
		 * 스테이지 시작 전 전부 로딩하는 방식으로 바뀌어 아래 AddUpgradeTiersUpToStageMax로 대체한다.
		 *
		 * const int32 NextTier =
		 * 	InitialTier + 1;
		 *
		 * if (Definition->FindTier(NextTier))
		 * {
		 * 	AddRequiredTier(
		 * 		Definition,
		 * 		NextTier);
		 * }
		 */
		AddUpgradeTiersUpToStageMax(Definition, InitialTier);
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
		TSet<int32>>&Pair
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
				SFGameState->GetStageState() != ESFStageState::LoadingInitialBuildingAssets)
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
		SF_LOG(Error,
			"[SFGameMode] 초기 건물 에셋 로딩 실패. "
			"Definition=%s, Tier=%d",
			*GetNameSafe(InDefinition),
			InTierIndex);

		FailStageBootstrap();
		return;
	}

	if (PendingInitialBuildingAssetRequestCount <= 0)
	{
		SF_LOG(Error,
			"[SFGameMode] 초기 Tier 0 요청 카운터가 "
			"잘못된 상태입니다.");

		FailStageBootstrap();
		return;
	}

	--PendingInitialBuildingAssetRequestCount;

	UpdateStageLoadingProgress();

	SF_LOG(Log,
		"[SFGameMode] 초기 Tier 준비 완료. "
		"Definition=%s, Tier=%d, Remaining=%d",
		*GetNameSafe(InDefinition), InTierIndex, PendingInitialBuildingAssetRequestCount);

	if (PendingInitialBuildingAssetRequestCount == 0)
	{
		// 90% Loading Completed Timer 1.f
		GetWorldTimerManager().SetTimer(
			Stage90PercentDelayTimerHandle,
			this,
			&ThisClass::BeginStageRenderPreparation,
			Stage90PercentDelayTime,
			false);
	}
}

void ASFGameMode::BeginStageRenderPreparation()
{
	if (!IsValid(SFGameState) ||
		SFGameState->GetStageState() != ESFStageState::LoadingInitialBuildingAssets)
	{
		return;
	}

	if (PendingInitialBuildingAssetRequestCount != 0)
	{
		SF_LOG(Error,
			"[SFGameMode] 남은 건물 에셋 요청이 있는데 "
			"렌더 준비 단계가 시작됐습니다. Remaining=%d",
			PendingInitialBuildingAssetRequestCount);

		FailStageBootstrap();
		return;
	}

	if (bStageRenderPreparationStarted)
	{
		return;
	}

	/*
	 * 이 함수가 true를 반환하면:
	 * 1. 모든 Anchor의 건물 생성 성공
	 * 2. 생성된 건물의 BeginPlay 실행 완료
	 * 3. 모든 원본 Anchor 제거 요청 완료
	 */
	if (!SpawnStageBuildingsFromAnchors())
	{
		FailStageBootstrap();
		return;
	}

	bStageRenderPreparationStarted = true;

	/*
	 * 비동기 로딩 90% + 모든 Anchor 생성 8%.
	 * Anchor 생성은 같은 프레임에서 동기적으로 끝나므로
	 * 90~98%가 단계적으로 올라가지 않고 98%로 변경된다.
	 */
	SFGameState->SetStageLoadingProgress(0.98f);

	GetWorldTimerManager().SetTimer(
		Stage98PercentDelayTimerHandle,
		this,
		&ThisClass::CompleteStageBootstrap,
		Stage98PercentDelayTime,
		false);
}

void ASFGameMode::CompleteStageBootstrap()
{
	if (!IsValid(SFGameState) ||
		SFGameState->GetStageState() != ESFStageState::LoadingInitialBuildingAssets)
	{
		return;
	}

	if (!bStageRenderPreparationStarted ||
		PendingInitialBuildingAssetRequestCount != 0)
	{
		SF_LOG(Error,
			"[SFGameMode] 스테이지 완료 조건이 올바르지 않습니다. "
			"RenderPreparation=%s, Remaining=%d",
			bStageRenderPreparationStarted
			? TEXT("true")
			: TEXT("false"),
			PendingInitialBuildingAssetRequestCount);

		FailStageBootstrap();
		return;
	}

	bStageRenderPreparationStarted = false;

	// 에셋 로딩, Anchor 생성, 0.5초 준비가 모두 완료됐다.
	SFGameState->SetStageLoadingProgress(1.f);

	if (!SFGameState->MarkStageReadyToStart())
	{
		FailStageBootstrap();
		return;
	}

	SF_LOG(Log,
		"[SFGameMode] Anchor 렌더 준비 완료. ReadyToStart.");
}

void ASFGameMode::FailStageBootstrap()
{
	bStageRenderPreparationStarted = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(Stage98PercentDelayTimerHandle);
	}


	if (!IsValid(SFGameState))
	{
		SF_LOG(Error,
			"[SFGameMode] 비동기 스테이지 초기화 실패. "
			"GameState가 유효하지 않습니다.");

		return;
	}

	if (SFGameState->GetStageState() == ESFStageState::InitializationFailed)
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
	constexpr float InitialBuildingAssetWeight = 0.60f;

	const float TotalProgress = FMath::Clamp(
		RaceDataProgress * RaceDataWeight +
		InitialBuildingAssetProgress *
		InitialBuildingAssetWeight,
		0.f,
		0.90f);

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

	// DT_SFStage의 해당 행에서 정한 시작 광물을 지급한다.
	SFGameState->AddResource(
		PlayerTeamIndex,
		EResourceType::Mineral,
		BootstrapStartingMineral);
	SFGameState->AddResource(
		PlayerTeamIndex,
		EResourceType::Ether,
		BootstrapStartingEther);
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

	if (!CreateStageCameraBoundaries())
	{
		SF_LOG(
			Warning,
			"[SFGameMode] NavMesh 기준 카메라 경계를 생성하지 못했습니다.");
	}

	if (!StartStage())
	{
		FailStageBootstrap();
		return;
	}

	// StartStage의 상태 알림에서 튜토리얼이 일시정지해도 UI 사운드는 바로 재생된다.
	StartStageMusic();

	SF_LOG(Log,
		"[SFGameMode] 로딩 화면 종료 후 스테이지를 시작했습니다.");
}

bool ASFGameMode::CreateStageCameraBoundaries()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	ClearStageCameraBoundaries();

	UNavigationSystemV1* NavigationSystem =
		UNavigationSystemV1::GetCurrent(World);

	if (!IsValid(NavigationSystem))
	{
		SF_LOG(Error, "[SFGameMode] NavigationSystem을 찾지 못했습니다.");
		return false;
	}

	ANavigationData* NavigationData =
		NavigationSystem->GetDefaultNavDataInstance();

	if (!IsValid(NavigationData))
	{
		SF_LOG(Error, "[SFGameMode] 기본 NavMesh 데이터를 찾지 못했습니다.");
		return false;
	}

	const FBox NavigationBounds = NavigationData->GetBounds();
	const FVector2D NavigationSize(
		NavigationBounds.Max.X - NavigationBounds.Min.X,
		NavigationBounds.Max.Y - NavigationBounds.Min.Y
	);

	if (
		!NavigationBounds.IsValid ||
		NavigationSize.X <= KINDA_SMALL_NUMBER ||
		NavigationSize.Y <= KINDA_SMALL_NUMBER
		)
	{
		SF_LOG(Error, "[SFGameMode] NavMesh Bounds가 유효하지 않습니다.");
		return false;
	}

	const float SafeThickness =
		FMath::Max(CameraBoundaryThickness, 1.0f);
	const float SafeHalfHeight =
		FMath::Max(CameraBoundaryHalfHeight, 100.0f);
	// NavMesh GetBounds()의 외곽 좌표를 그대로 사용한다.
	// 별도 Inset을 적용하지 않아 생성된 경계의 외곽이 NavMesh 크기와 일치한다.
	const float MinX = NavigationBounds.Min.X;
	const float MaxX = NavigationBounds.Max.X;
	const float MinY = NavigationBounds.Min.Y;
	const float MaxY = NavigationBounds.Max.Y;
	const float CenterZ = NavigationBounds.GetCenter().Z;

	if (MinX >= MaxX || MinY >= MaxY)
	{
		SF_LOG(Error, "[SFGameMode] CameraBoundary Inset이 NavMesh보다 큽니다.");
		return false;
	}

	auto SpawnBoundary =
		[this, World, SafeThickness, SafeHalfHeight](
			const FVector& Center,
			const FVector& HalfExtent,
			const TCHAR* BoundaryName
			) -> bool
		{
			AActor* BoundaryActor =
				World->SpawnActor<AActor>(
					AActor::StaticClass(),
					FTransform::Identity
				);

			if (!IsValid(BoundaryActor))
			{
				return false;
			}

			UBoxComponent* BoundaryBox =
				NewObject<UBoxComponent>(
					BoundaryActor,
					FName(BoundaryName)
				);

			if (!IsValid(BoundaryBox))
			{
				BoundaryActor->Destroy();
				return false;
			}

			BoundaryActor->SetRootComponent(BoundaryBox);
			BoundaryActor->AddInstanceComponent(BoundaryBox);

			BoundaryBox->SetBoxExtent(HalfExtent);
			BoundaryBox->SetCollisionEnabled(
				ECollisionEnabled::QueryOnly
			);
			BoundaryBox->SetCollisionObjectType(
				ECC_GameTraceChannel2
			);
			BoundaryBox->SetCollisionResponseToAllChannels(
				ECR_Ignore
			);
			BoundaryBox->SetCollisionResponseToChannel(
				ECC_GameTraceChannel2,
				ECR_Block
			);
			// 카메라 폰의 CameraCollision은 Pawn 타입으로 이동한다.
			// SpringArm은 CameraBoundary 채널로 검사하지만,
			// 실제 Pawn 이동 Sweep은 경계가 Pawn을 Block해야 멈춘다.
			BoundaryBox->SetCollisionResponseToChannel(
				ECC_Pawn,
				ECR_Block
			);
			BoundaryBox->SetGenerateOverlapEvents(false);
			BoundaryBox->SetCanEverAffectNavigation(false);
			BoundaryBox->SetHiddenInGame(true);
			BoundaryBox->RegisterComponent();

			BoundaryActor->SetActorLocation(Center);
			BoundaryActor->SetActorEnableCollision(true);
			BoundaryActor->SetActorHiddenInGame(true);
			StageCameraBoundaryActors.Add(BoundaryActor);

			return true;
		};

	const float BoundaryCenterOffset = SafeThickness * 0.5f;
	const float CenterX =
		(NavigationBounds.Min.X + NavigationBounds.Max.X) * 0.5f;
	const float CenterY =
		(NavigationBounds.Min.Y + NavigationBounds.Max.Y) * 0.5f;

	const bool bMinXCreated = SpawnBoundary(
		FVector(MinX + BoundaryCenterOffset, CenterY, CenterZ),
		FVector(
			BoundaryCenterOffset,
			NavigationSize.Y * 0.5f,
			SafeHalfHeight
		),
		TEXT("StageCameraBoundary_MinX")
	);

	const bool bMaxXCreated = SpawnBoundary(
		FVector(MaxX - BoundaryCenterOffset, CenterY, CenterZ),
		FVector(
			BoundaryCenterOffset,
			NavigationSize.Y * 0.5f,
			SafeHalfHeight
		),
		TEXT("StageCameraBoundary_MaxX")
	);

	const bool bMinYCreated = SpawnBoundary(
		FVector(CenterX, MinY + BoundaryCenterOffset, CenterZ),
		FVector(
			NavigationSize.X * 0.5f,
			BoundaryCenterOffset,
			SafeHalfHeight
		),
		TEXT("StageCameraBoundary_MinY")
	);

	const bool bMaxYCreated = SpawnBoundary(
		FVector(CenterX, MaxY - BoundaryCenterOffset, CenterZ),
		FVector(
			NavigationSize.X * 0.5f,
			BoundaryCenterOffset,
			SafeHalfHeight
		),
		TEXT("StageCameraBoundary_MaxY")
	);

	const bool bSuccess =
		bMinXCreated &&
		bMaxXCreated &&
		bMinYCreated &&
		bMaxYCreated;

	SF_LOG(
		Log,
		"[SFGameMode] NavMesh 기준 카메라 경계 생성 완료. "
		"Min=(%.1f, %.1f), Max=(%.1f, %.1f)",
		MinX,
		MinY,
		MaxX,
		MaxY
	);

	return bSuccess;
}

void ASFGameMode::ClearStageCameraBoundaries()
{
	for (AActor* BoundaryActor : StageCameraBoundaryActors)
	{
		if (IsValid(BoundaryActor))
		{
			BoundaryActor->Destroy();
		}
	}

	StageCameraBoundaryActors.Reset();
}

ASFGameState* ASFGameMode::GetSFGameState() const
{
	return SFGameState;
}
