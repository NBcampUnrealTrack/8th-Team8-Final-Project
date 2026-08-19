// SFGameState.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Datas/Resource/ResourceTypes.h"
#include "SFGameState.generated.h"

// 상태 정의 = 진행 상황
UENUM(BlueprintType)
enum class ESFStageState : uint8
{
    Initializing UMETA(DisplayName = "Initializing"),
    Playing      UMETA(DisplayName = "Playing"),
    Finished     UMETA(DisplayName = "Finished")
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

// 광물 초기값 및 광물 자동생산량 변경
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FSFOnMineralChanged,
    int32,
    NewMineral,
    float,
    NewMineralIncomeRate
);

// 에테르 초기값 및 에테르 자동 생산량 변경
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FSFOnEtherChanged,
    int32,
    NewEther,
    float,
    NewEtherIncomeRate
);

// 현재 및 최대 인구수 변경
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FSFOnPopulationChanged,
    int32,
    NewCurrentPopulation,
    int32,
    NewMaximumPopulation
);

class ASFNexusBuilding;

UCLASS()
class STRIKEFORGE_API ASFGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
    ASFGameState();

    // GamaMode가 현재 선택된 스테이지 ID를 전달
    bool InitializeStage(FName StageRowName);

    bool StartStage(); // 스테이지 초기화 부분 -> GameMode에서 호출

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
    int32 GetMineral() const;

    UFUNCTION(BlueprintPure, Category = "SF|Resource")
    int32 GetEther() const;

    UFUNCTION(BlueprintPure, Category = "SF|Resource")
    float GetMineralIncomeRate() const;

    UFUNCTION(BlueprintPure, Category = "SF|Resource")
    float GetEtherIncomeRate() const;

    UFUNCTION(BlueprintPure, Category = "SF|Population")
    int32 GetCurrentPopulation() const;

    UFUNCTION(BlueprintPure, Category = "SF|Population")
    int32 GetMaximumPopulation() const;

    // 자원 값 변경
    UFUNCTION(BlueprintCallable, Category = "SF|Resource")
    void SetMineral(int32 NewMineral);

    UFUNCTION(BlueprintCallable, Category = "SF|Resource")
    void SetEther(int32 NewEther);

    UFUNCTION(BlueprintCallable, Category = "SF|Resource")
    void SetMineralIncomeRate(float NewMineralIncomeRate);

    UFUNCTION(BlueprintCallable, Category = "SF|Resource")
    void SetEtherIncomeRate(float NewEtherIncomeRate);

    UFUNCTION(BlueprintCallable, Category = "SF|Population")
    void SetPopulation(int32 NewCurrentPopulation, int32 NewMaximumPopulation);
    
    /** 자원이 충분한지만 확인한다. 아무것도 바꾸지 않는다. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Resource")
    bool CanAfford(int32 InMineral, int32 InEther) const;

    /** 충분하면 차감하고 true. 실패 시 아무것도 바꾸지 않는다. */
    UFUNCTION(BlueprintCallable, Category = "SF|Resource")
    bool TrySpendCost(int32 InMineral, int32 InEther);
    
    UFUNCTION(BlueprintCallable, Category = "SF|Resource")
    void RefundCost(int32 InMineral, int32 InEther); 
    // 여기까지

    // 26.08.18
    // Nexus
    // 팀별 넥서스 포인터 등록
    UFUNCTION(BlueprintCallable, Category = "SF|Nexus")
    bool RegisterNexus(int32 TeamIndex, ASFNexusBuilding* Nexus);

    // 등록된 넥서스 포인터 제거
    UFUNCTION(BlueprintCallable, Category = "SF|Nexus")
    bool UnregisterNexus(int32 TeamIndex, ASFNexusBuilding* Nexus);

    // 팀 번호를 이용해 넥서스 조회
    UFUNCTION(BlueprintPure, Category = "SF|Nexus")
    ASFNexusBuilding* GetNexus(int32 TeamIndex) const;

public:

    // UI 부분
    // UI가 나중에 상태 변화를 받을 때 이용 가능한 통로 생성
    UPROPERTY(BlueprintAssignable, Category = "SF|Stage")
    FSFOnStageStateChanged OnStageStateChanged;

    // 결과 승 or 패배 시 받을 때 사용(결과창)
    UPROPERTY(BlueprintAssignable, Category = "SF|Stage")
    FSFOnStageResultChanged OnStageResultChanged;

    // InGameUI 바인딩 용 Delegate
    UPROPERTY(BlueprintAssignable, Category = "SF|Resource")
    FSFOnMineralChanged OnMineralChanged;

    UPROPERTY(BlueprintAssignable, Category = "SF|Resource")
    FSFOnEtherChanged OnEtherChanged;

    UPROPERTY(BlueprintAssignable, Category = "SF|Population")
    FSFOnPopulationChanged OnPopulationChanged;

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SF|Stage", meta = (AllowPrivateAccess = "true"))
    ESFStageState StageState;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SF|Stage", meta = (AllowPrivateAccess = "true"))
    ESFStageResult StageResult;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SF|Stage", meta = (AllowPrivateAccess = "true"))
    FName CurrentStageRowName = NAME_None;

    // InGameUI 용 초기값
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SF|Resource", meta = (AllowPrivateAccess = "true"))
    int32 Mineral = 0;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SF|Resource", meta = (AllowPrivateAccess = "true"))
    int32 Ether = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SF|Resource", meta = (AllowPrivateAccess = "true"))
    float MineralIncomeRate = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SF|Resource", meta = (AllowPrivateAccess = "true"))
    float EtherIncomeRate = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SF|Population", meta = (AllowPrivateAccess = "true"))
    int32 CurrentPopulation = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SF|Population", meta = (AllowPrivateAccess = "true"))
    int32 MaximumPopulation = 0;

    // 26.08.18
    // TeamIndex를 기준으로 각 팀의 넥서스를 저장
    UPROPERTY()
    TMap<int32, TObjectPtr<ASFNexusBuilding>> NexusByTeam;
	
};
