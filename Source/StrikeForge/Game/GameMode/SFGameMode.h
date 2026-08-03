// SFGameMode.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "../GameState/SFGameState.h"

#include "SFGameMode.generated.h"

UCLASS()
class STRIKEFORGE_API ASFGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	ASFGameMode();

protected:
	virtual void BeginPlay() override;

public:
	// 스테이지 초기화가 끝난 후 게임 시작
	UFUNCTION(BlueprintCallable, Category = "SF|Stage")
	bool StartStage();

	// 승리 결과 스테이지 종료
	UFUNCTION(BlueprintCallable, Category = "SF|Stage")
	bool ProcessVictory();

	// 패배 결과 스테이지 종료
	UFUNCTION(BlueprintCallable, Category = "SF|Stage")
	bool ProcessDefeat();

	// 전달받은 결과를 기준으로 스테이지 종료
	bool FinishStage(ESFStageResult NewStageResult);

	UFUNCTION(BlueprintPure, Category = "SF|Stage")
	ASFGameState* GetSFGameState() const;

private:
	UPROPERTY()
	TObjectPtr<ASFGameState> SFGameState;
};
