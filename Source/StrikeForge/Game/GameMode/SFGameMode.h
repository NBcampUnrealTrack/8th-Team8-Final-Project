// SFGameMode.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "../GameState/SFGameState.h"

#include "SFGameMode.generated.h"

class ASFNexusBuilding;
class ASFBuildingSpawnAnchor;
class USFBuildingDefinition;
class USFBuildingAssetSubsystem;

enum class ESFStageBootstrapState : uint8
{
	None,
    LoadingRaceData,

    // Buildable과 Anchor를 포함한 초기 Tier 로딩
    LoadingInitialBuildingAssets,

    Ready,
    Failed
};

UCLASS()
class STRIKEFORGE_API ASFGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	ASFGameMode();
	
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	// 스테이지 초기화가 끝난 후 게임 시작
	UFUNCTION(BlueprintCallable, Category = "SF|Stage")
	bool StartStage();

	// 파괴된 코어건물(메인)을 기준으로 승리, 패배 처리
	UFUNCTION(BlueprintCallable, Category = "SF|Stage")
	bool ProcessCoreDestroyed(int32 DestroyedTeamIndex);

	// 승리 결과 스테이지 종료
	UFUNCTION(BlueprintCallable, Category = "SF|Stage")
	bool ProcessVictory();

	// 패배 결과 스테이지 종료
	UFUNCTION(BlueprintCallable, Category = "SF|Stage")
	bool ProcessDefeat();

	// 전달받은 결과를 기준으로 스테이지 종료
	bool FinishStage(ESFStageResult NewStageResult);
	
	// getter
	UFUNCTION(BlueprintPure, Category = "SF|Stage")
	ASFGameState* GetSFGameState() const;
	UFUNCTION(BlueprintPure, Category = "SF|Team")
	FORCEINLINE int32 GetPlayerTeamIndex() const { return PlayerTeamIndex; }

	// 26.08.18
	// 데이터테이블 정보를 이용해 지정된 위치에 넥서스를 생성
	// 넥서스 생성을 런타임에 gamemode가 해서 팀인덱스를 등록할지
	// 뷰포트배치로 nexusbuilding의 beginplay에서 팀인덱스를 등록할지 고민했으나
	// 여러방면상 nexusbuilding이 생성과 동시에 팀인덱스 등록으로 생각하고있음
	// UFUNCTION(BlueprintCallable, Category = "SF|Nexus")
	// ASFNexusBuilding* SpawnNexus(FName NexusRowName, const FTransform& SpawnTransform, int32 TeamIndex);
protected:
	// 시작시 게임인스턴스의 설정된 종족값을 가지고옴 - beginplay보다 먼저호출됨.
	virtual void InitGameState() override;
	virtual void BeginPlay() override;
	
private:
	/** 레벨의 모든 BuildingSpawnAnchor를 조회하여 팀별 종족 건물을 생성한다. */
	bool SpawnStageBuildingsFromAnchors();

	/** 하나의 BuildingSpawnAnchor를 실제 건물로 변환한다. */
	bool SpawnBuildingAtAnchor(const ASFBuildingSpawnAnchor* InAnchor);
	
	// Async
	/** 스테이지 비동기 초기화를 시작한다. BeginPlay에서 호출되며 RequestRequiredRaceData로 이어진다. */
	void BeginStageBootstrap();
	/** 스테이지에 필요한 종족 데이터를 모두 요청한다. BeginStageBootstrap에서 호출되며 HandleRaceDataReady로 이어진다. */
	void RequestRequiredRaceData();
	/** 종족 로드 결과를 모아 팀과 스테이지 데이터를 초기화한다. Subsystem 콜백으로 호출되며 RequestAnchorBuildingAssets로 이어진다. */
	void HandleRaceDataReady(ESFRace InRace, bool bSuccess);
	/** Anchor에 필요한 건물 Tier 0 리소스를 요청한다. HandleRaceDataReady에서 호출되며 HandleAnchorAssetsReady로 이어진다. */
	void RequestInitialBuildingAssets();
	/** Anchor 리소스 로드 결과를 모은다. Subsystem 콜백으로 호출되며 모든 요청 완료 시 CompleteStageBootstrap으로 이어진다. */
	void HandleInitialBuildingAssetsReady(
		USFBuildingDefinition* InDefinition,
		int32 InTierIndex,
		bool bSuccess);
	/** Anchor 건물을 생성하고 스테이지를 시작한다. HandleAnchorAssetsReady에서 호출되며 SpawnStageBuildingsFromAnchors와 StartStage로 이어진다. */
	void CompleteStageBootstrap();
	/** 비동기 스테이지 초기화를 실패 상태로 종료한다. 각 검증·로드 단계에서 호출되며 이후 초기화 진행을 중단한다. */
	void FailStageBootstrap();
	// UI 진행도
	void UpdateStageLoadingProgress();
	
	UPROPERTY(Transient)
	TObjectPtr<USFBuildingAssetSubsystem>
		BuildingAssetSubsystem;

	ESFStageBootstrapState StageBootstrapState =
	ESFStageBootstrapState::None;

	FName BootstrapStageRowName = NAME_None;

	TMap<int32, ESFRace> BootstrapTeamRaces;

	int32 TotalRaceRequestCount = 0;
	int32 PendingRaceRequestCount = 0;

	int32 TotalInitialBuildingAssetRequestCount = 0;
	int32 PendingInitialBuildingAssetRequestCount = 0;
private:
	UPROPERTY()
	TObjectPtr<ASFGameState> SFGameState;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Stage", meta = (AllowPrivateAccess = "true"))
	int32 PlayerTeamIndex = 0;
	
	/** 플레이어 팀의 시작 자원 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Stage", meta = (AllowPrivateAccess = "true"))
	int32 StartingMineral = 200;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Stage", meta = (AllowPrivateAccess = "true"))
	int32 StartingEther = 100;
};
