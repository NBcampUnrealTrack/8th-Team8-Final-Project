// SFGameMode.cpp


#include "SFGameMode.h"
#include "Logging/SFLog.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "Buildings/Building/HouseBuilding/Nexus/SFNexusBuilding.h"
#include "Datas/Building/BuildingAppearance.h"
#include "Game/Subsystem/SFDataTableManager.h"

ASFGameMode::ASFGameMode()
{
	// GameMode가 사용할 GameState 클래스 지정
	// World Setting에서 별도로 잘못 설정하는 상황 방지
	GameStateClass = ASFGameState::StaticClass();
}

void ASFGameMode::BeginPlay()
{
	Super::BeginPlay();

	SFGameState = GetGameState<ASFGameState>();

	if (!IsValid(SFGameState))
	{
		SF_LOG(Error, "[SFGameMode] Failed to find SFGameState.");

		return;
	}

	SF_LOG(Log, "[SFGameMode] Stage initialization started.");

	// 나중에 타이틀, 로비, 인게임 맵 추가 시 작성될 부분

	StartStage();
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
ASFNexusBuilding* ASFGameMode::SpawnNexus(FName NexusRowName, const FTransform& SpawnTransform, int32 TeamIndex)
{
    if (NexusRowName.IsNone())
    {
        SF_LOG(Warning, "[SFGameMode] SpawnNexus failed: Nexus RowName is None.");

        return nullptr;
    }

    if (TeamIndex == INDEX_NONE)
    {
        SF_LOG(Warning, "[SFGameMode] SpawnNexus failed: TeamIndex is invalid.");

        return nullptr;
    }

    UGameInstance* GameInstance = GetGameInstance();

    if (!IsValid(GameInstance))
    {
        SF_LOG(Error, "[SFGameMode] SpawnNexus failed: GameInstance is invalid.");

        return nullptr;
    }

    USFDataTableManager* DataManager = GameInstance->GetSubsystem<USFDataTableManager>();

    if (!IsValid(DataManager))
    {
        SF_LOG(Error, "[SFGameMode] SpawnNexus failed: DataTableManager is invalid.");

        return nullptr;
    }

    const FBuildingAppearance* NexusData = DataManager->GetDataRow<FBuildingAppearance>(EDataTableType::Building, NexusRowName, TEXT("SpawnNexus"));

    if (NexusData == nullptr)
    {
        SF_LOG(Warning, "[SFGameMode] SpawnNexus failed: Row was not found. Row: %s", *NexusRowName.ToString());

        return nullptr;
    }

    UClass* NexusClass = NexusData->BuildingClass.LoadSynchronous();

    if (!IsValid(NexusClass))
    {
        SF_LOG(Warning, "[SFGameMode] SpawnNexus failed: BuildingClass is invalid. Row: %s", *NexusRowName.ToString());

        return nullptr;
    }

    if (!NexusClass->IsChildOf(ASFNexusBuilding::StaticClass()))
    {
        SF_LOG(Warning, "[SFGameMode] SpawnNexus failed: BuildingClass is not an SFNexusBuilding. Class: %s", *GetNameSafe(NexusClass));

        return nullptr;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        SF_LOG(Error, "[SFGameMode] SpawnNexus failed: World is invalid.");

        return nullptr;
    }

    ASFNexusBuilding* SpawnedNexus = World->SpawnActorDeferred<ASFNexusBuilding>(NexusClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

    if (!IsValid(SpawnedNexus))
    {
        SF_LOG(Error, "[SFGameMode] SpawnNexus failed: SpawnActorDeferred failed.");

        return nullptr;
    }

    // BeginPlay 전에 데이터테이블 RowName 설정
    SpawnedNexus->SetRowName(NexusRowName);

    // BeginPlay 전에 전투 팀 설정
    UCombatTargetComponent* CombatTarget = SpawnedNexus->FindComponentByClass<UCombatTargetComponent>();

    if (!IsValid(CombatTarget))
    {
        SF_LOG(Error, "[SFGameMode] SpawnNexus failed: CombatTargetComponent is invalid.");

        SpawnedNexus->Destroy();

        return nullptr;
    }

    CombatTarget->SetTeamIndex(TeamIndex);

    // 여기서 BeginPlay가 실행됨
    SpawnedNexus->FinishSpawning(SpawnTransform);

    if (!IsValid(SFGameState))
    {
        SFGameState = GetGameState<ASFGameState>();
    }

    if (!IsValid(SFGameState))
    {
        SF_LOG(Error, "[SFGameMode] SpawnNexus failed: SFGameState is invalid.");

        SpawnedNexus->Destroy();

        return nullptr;
    }

    if (!SFGameState->RegisterNexus(TeamIndex, SpawnedNexus))
    {
        SF_LOG(Error, "[SFGameMode] SpawnNexus failed: Nexus registration failed.");

        SpawnedNexus->Destroy();

        return nullptr;
    }

    SF_LOG(Log, "[SFGameMode] Nexus spawned. Team: %d, Row: %s, Nexus: %s", TeamIndex, *NexusRowName.ToString(), *GetNameSafe(SpawnedNexus));

    return SpawnedNexus;
}
