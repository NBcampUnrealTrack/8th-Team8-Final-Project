// SFGameState.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
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

public:

    // UI 부분
    // UI가 나중에 상태 변화를 받을 때 이용 가능한 통로 생성
    UPROPERTY(BlueprintAssignable, Category = "SF|Stage")
    FSFOnStageStateChanged OnStageStateChanged;

    // 결과 승 or 패배 시 받을 때 사용(결과창)
    UPROPERTY(BlueprintAssignable, Category = "SF|Stage")
    FSFOnStageResultChanged OnStageResultChanged;

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SF|Stage", meta = (AllowPrivateAccess = "true"))
    ESFStageState StageState;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SF|Stage", meta = (AllowPrivateAccess = "true"))
    ESFStageResult StageResult;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SF|Stage", meta = (AllowPrivateAccess = "true"))
    FName CurrentStageRowName = NAME_None;
	
};
