// SFGameState.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "Datas/Stage/SFStageData.h"
#include "Game/GameState/SFTeamState.h"
#include "SFGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FSFOnStageLoadingProgressChanged,
    float,
    Progress);

class ASFNexusBuilding;
class ASFBaseBuilding;
class USFBuildingCatalog;

// 상태 정의 = 진행 상황
UENUM(BlueprintType)
enum class ESFStageState : uint8
{
    // 기존 enum 값을 보존하기 위해 기존 항목 순서를 변경하지 않는다.
    Initializing                 UMETA(DisplayName = "Initializing"),
    InitializationFailed         UMETA(DisplayName = "Initialization Failed"),
    LoadingRaceData              UMETA(DisplayName = "Loading Race Data"),
    LoadingInitialBuildingAssets UMETA(DisplayName = "Loading Initial Building Assets"),
    ReadyToStart                 UMETA(DisplayName = "Ready To Start"),
    Playing                      UMETA(DisplayName = "Playing"),
    Finished                     UMETA(DisplayName = "Finished"),
};

// 상태 정의 = 결과
UENUM(BlueprintType)
enum class ESFStageResult : uint8
{
    Undetermined UMETA(DisplayName = "Undetermined"), // 결과 중간값 = Finish전까지 상태
    Victory      UMETA(DisplayName = "Victory"),
    Defeat       UMETA(DisplayName = "Defeat")
};

// 스테이지 상태 변경 이벤트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FSFOnStageStateChanged,
    ESFStageState,
    NewStageState
);

// 스테이지 결과 확정 이벤트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FSFOnStageResultChanged,
    ESFStageResult,
    NewStageResult
);

UCLASS()
class STRIKEFORGE_API ASFGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
    ASFGameState();

    // GamaMode가 현재 선택된 스테이지 ID를 전달
    bool InitializeStage(FName StageRowName,const TArray<FSFStageBuildingRule>& InBuildingRules);
    /**
     * 스테이지 비동기 초기화를 시작한다.
     * GameMode::BeginStageBootstrap에서 호출되며
     * Initializing 상태를 LoadingRaceData로 변경한다.
     */
    bool BeginRaceDataLoading();

    /**
     * 종족 데이터 로딩이 끝난 뒤 초기 건물 에셋 로딩 단계로 전환한다.
     * GameMode::RequestInitialBuildingAssets에서 호출되며
     * LoadingRaceData를 LoadingInitialBuildingAssets로 변경한다.
     */
    bool BeginInitialBuildingAssetsLoading();

    /**
     * 스테이지의 기본 데이터와 건물이 준비되었음을 기록한다.
     * GameMode::CompleteStageBootstrap에서 호출되며
     * LoadingInitialBuildingAssets를 ReadyToStart로 변경한다.
     */
    bool MarkStageReadyToStart();
    
    // 스테이지 초기화 부분 -> GameMode에서 호출
    bool StartStage(); 

    /** 스테이지 초기화 실패를 기록하고 UI에 상태 변경을 알린다. */
    bool FailStageInitialization();
    
    // 스테이지 승 or 패 부분 -> GameMode에서 호출
    bool FinishStage(ESFStageResult NewStageResult);

    UFUNCTION(BlueprintPure, Category = "SF|Stage")
    FName GetCurrentStageRowName() const;

    UFUNCTION(BlueprintPure, Category = "SF|Stage")
    ESFStageState GetStageState() const;

    UFUNCTION(BlueprintPure, Category = "SF|Stage")
    ESFStageResult GetStageResult() const;

    UFUNCTION(BlueprintPure, Category = "SF|Stage")
    bool IsStagePlaying() const;

    UFUNCTION(BlueprintPure, Category = "SF|Stage")
    bool IsStageFinished() const;

    // InGameUI에서 사용될 광물, 에테르, 인구수
    UFUNCTION(BlueprintPure, Category = "SF|Resource")
    int32 GetMineral(int32 TeamIndex) const;
    UFUNCTION(BlueprintPure, Category = "SF|Resource")
    int32 GetEther(int32 TeamIndex) const;
    UFUNCTION(BlueprintPure, Category = "SF|Resource")
    float GetMineralIncomeRate(int32 TeamIndex) const;
    UFUNCTION(BlueprintPure, Category = "SF|Resource")
    float GetEtherIncomeRate(int32 TeamIndex) const;
    UFUNCTION(BlueprintPure, Category = "SF|Population")
    int32 GetCurrentPopulation(int32 TeamIndex) const;
    UFUNCTION(BlueprintPure, Category = "SF|Population")
    int32 GetMaximumPopulation(int32 TeamIndex) const;
    /** TeamIndex에 Race와 BuildingCatalog를 등록한다. */
    bool RegisterTeam(int32 TeamIndex, ESFRace InRace, USFBuildingCatalog* InBuildingCatalog);
    /** 이미 등록된 팀에 플레이어 컨트롤러를 연결한다. */
    bool RegisterPlayer(int32 TeamIndex, ASFInGamePlayerController* InPlayer);
    /* 지정한 팀의 건물 카탈로그를 반환한다. */
    UFUNCTION(BlueprintPure, Category = "SF|Building")
    USFBuildingCatalog* GetTeamBuildingCatalog(int32 TeamIndex) const;
    
    // 자원 값 변경
    UFUNCTION(BlueprintCallable, Category = "SF|Resource")
    void AddResource(int32 TeamIndex, EResourceType InResourceType, int32 InAmount);
    UFUNCTION(BlueprintCallable, Category = "SF|Resource")
    void AddResourceIncomeRate(int32 TeamIndex, EResourceType InResourceType, float InIncomeRate);
    UFUNCTION(BlueprintCallable, Category = "SF|Population")
    void AddPopulation(int32 TeamIndex, int32 InDelta);
    UFUNCTION(BlueprintCallable, Category = "SF|Population")
    void AddMaxPopulation(int32 TeamIndex, int32 InDelta);
    
    // ── 판정 · 소비 ──
    /** 자원이 충분한지만 확인한다. 아무것도 바꾸지 않는다 */
    UFUNCTION(BlueprintPure, Category = "SF|Resource")
    bool CanAffordResource(int32 TeamIndex, const FResources& InCost) const;
    /** 충분하면 차감하고 true. 실패 시 아무것도 바꾸지 않는다 */
    UFUNCTION(BlueprintCallable, Category = "SF|Resource")
    bool TrySpendCost(int32 TeamIndex, const FResources& InCost);
    UFUNCTION(BlueprintCallable, Category = "SF|Resource")
    void RefundCost(int32 TeamIndex, const FResources& InResources);
    UFUNCTION(BlueprintPure, Category = "SF|Population")
    bool CanAffordPopulation(int32 TeamIndex, int32 InPopulationCost) const;
    // 여기까지

    // 26.08.18
    // Nexus
    // 팀별 넥서스 포인터 등록
    UFUNCTION(BlueprintCallable, Category = "SF|Nexus")
    bool RegisterNexus(int32 TeamIndex, ASFNexusBuilding* Nexus);

    // 등록된 넥서스 포인터 제거
    UFUNCTION(BlueprintCallable, Category = "SF|Nexus")
    bool UnregisterNexus(int32 TeamIndex, ASFNexusBuilding* Nexus);
    /** 해당 팀에 등록된 컨트롤러를 제거한다. */
    bool UnregisterPlayer(int32 TeamIndex, ASFInGamePlayerController* InPlayer);
    
    // 팀 번호를 이용해 넥서스, 플레이어컨트롤러 조회
    UFUNCTION(BlueprintPure, Category = "SF|Nexus")
    ASFNexusBuilding* GetNexus(int32 TeamIndex) const;
    UFUNCTION(BlueprintPure, Category = "SF|Player")
    ASFInGamePlayerController* GetPlayer(int32 TeamIndex) const;
    
    UFUNCTION(BlueprintPure, Category = "SF|Team")
    USFTeamState* GetTeamState(int32 TeamIndex) const;
    USFTeamState* FindOrAddTeam(int32 TeamIndex);
    
    int32 GetNexusTier(int32 TeamIndex) const;

    // 로딩 진행도
    UFUNCTION(BlueprintPure, Category = "SF|Stage")
    FORCEINLINE float GetStageLoadingProgress() const { return StageLoadingProgress; }

    void SetStageLoadingProgress(float InProgress);

    bool IsBuildingAllowedInCurrentStage(
        FName InBuildingId
    ) const;

    int32 GetMaxAllowedTierInCurrentStage(
        FName InBuildingId
    ) const;

    bool AreBuildingRequirementsMet(
        int32 TeamIndex,
        FName InBuildingId
    ) const;

public:

    // UI 부분
    // UI가 나중에 상태 변화를 받을 때 이용 가능한 통로 생성
    UPROPERTY(BlueprintAssignable, Category = "SF|Stage")
    FSFOnStageStateChanged OnStageStateChanged;

    // 결과 승 or 패배 시 받을 때 사용(결과창)
    UPROPERTY(BlueprintAssignable, Category = "SF|Stage")
    FSFOnStageResultChanged OnStageResultChanged;

    // 진행도 수신 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "SF|Stage")
    FSFOnStageLoadingProgressChanged OnStageLoadingProgressChanged;
private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SF|Stage", meta = (AllowPrivateAccess = "true"))
    ESFStageState StageState;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SF|Stage", meta = (AllowPrivateAccess = "true"))
    ESFStageResult StageResult;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SF|Stage", meta = (AllowPrivateAccess = "true"))
    FName CurrentStageRowName = NAME_None;

    // 26.08.18
    // TeamIndex를 기준으로 각 팀의 넥서스를 저장
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SF|Team", meta = (AllowPrivateAccess = "true"))
    TMap<int32, TObjectPtr<USFTeamState>> TeamStates;
        
    // 로딩 진행도
    float StageLoadingProgress = 0.f;

    UPROPERTY()
    TArray<FSFStageBuildingRule> CurrentStageBuildingRules;
	
    bool HasCompletedBuilding(
        int32 TeamIndex,
        FName InBuildingId
    ) const;

};
