// SFGameMode.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "../GameState/SFGameState.h"

#include "SFGameMode.generated.h"

class ASFNexusBuilding;

UCLASS()
class STRIKEFORGE_API ASFGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	ASFGameMode();
	
	virtual void PostLogin(APlayerController* NewPlayer) override;
	
	UFUNCTION(BlueprintPure, Category = "SF|Team")
	FORCEINLINE int32 GetPlayerTeamIndex() const { return PlayerTeamIndex; }
protected:
	// 시작시 게임인스턴스의 설정된 종족값을 가지고옴
	virtual void InitGameState() override;
	virtual void BeginPlay() override;
	
public:
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

	UFUNCTION(BlueprintPure, Category = "SF|Stage")
	ASFGameState* GetSFGameState() const;

	// 26.08.18
	// 데이터테이블 정보를 이용해 지정된 위치에 넥서스를 생성
	// 넥서스 생성을 런타임에 gamemode가 해서 팀인덱스를 등록할지
	// 뷰포트배치로 nexusbuilding의 beginplay에서 팀인덱스를 등록할지 고민했으나
	// 여러방면상 nexusbuilding이 생성과 동시에 팀인덱스 등록으로 생각하고있음
	// UFUNCTION(BlueprintCallable, Category = "SF|Nexus")
	// ASFNexusBuilding* SpawnNexus(FName NexusRowName, const FTransform& SpawnTransform, int32 TeamIndex);

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
