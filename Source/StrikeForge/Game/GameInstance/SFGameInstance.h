#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"

// 추가부분 26.08.10
#include "Datas/Stage/SFStageData.h"
#include "Datas/Race/SFRaceType.h"

#include "SFGameInstance.generated.h"
// 수정부분
class UWorld;
class USFBuildingCatalog;

UENUM(BlueprintType)
enum class EDataTableType : uint8
{
	UI,
	Unit,
	Stage,	// 추가부분
	ProductionUI,
};

UCLASS()
class STRIKEFORGE_API USFGameInstance : public UGameInstance
{
	GENERATED_BODY()
public:
	UDataTable* GetDataTableType(EDataTableType TableType) const;

	// 추가한 부분
	UFUNCTION(BlueprintCallable, Category = "SF|Level")
	bool MoveToTitle();

	UFUNCTION(BlueprintCallable, Category = "SF|Level")
	bool MoveToLobby();

	UFUNCTION(BlueprintCallable, Category = "SF|Stage")
	bool SelectStage(FName StageRowName);

	UFUNCTION(BlueprintPure, Category = "SF|Stage")
	FName GetSelectedStageRowName() const;

	UFUNCTION(BlueprintCallable, Category = "SF|Stage")
	bool MoveToStage();

	// 추가 부분 26.08.10
	// 스테이지 진행 상태 확인
	UFUNCTION(BlueprintPure, Category = "SF|Stage")
	ESFStageProgressState GetStageProgressState(FName StageRowName) const;

	// 스테이지 클리어 기록
	UFUNCTION(BlueprintCallable, Category = "SF|Stage")
	bool MarkStageCleared(FName StageRowName);
	
	UFUNCTION(BlueprintCallable, Category = "SF|Stage")
	bool RetryCurrentStage();
	
	// Race
	// 종족 선택 함수 
	// 종족Enum만 선택해놓으면 GM이 시작할때 Load함수를 부르고 종족에 맞는 Catalog를 가져간다
	UFUNCTION(BlueprintCallable, Category = "SF|Race")
	bool SelectRace(ESFRace InRace);
	// TMap에서 종족에 맞는 카탈로그 반환
	// 비동기로 비활성화
	//USFBuildingCatalog* LoadBuildingCatalog(ESFRace InRace) const;
	TSoftObjectPtr<USFBuildingCatalog>
			GetBuildingCatalogAsset(ESFRace InRace) const;
	
	// 플레이어 종족 getter
	UFUNCTION(BlueprintPure, Category = "SF|Race")
	FORCEINLINE ESFRace GetSelectedRace() const { return SelectedRace; }
private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataTable|UI", meta = (AllowPrivateAccess = true))
	TObjectPtr<UDataTable> UIDataTable;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataTable|Unit", meta = (AllowPrivateAccess = true))
	TObjectPtr<UDataTable> UnitsDataTable;

	// 추가 부분
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataTable|Game", meta = (AllowPrivateAccess = true))
	TObjectPtr<UDataTable> StageDataTable;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category = "DataTable|UI",meta = (AllowPrivateAccess = true))
	TObjectPtr<UDataTable> ProductionUIDataTable;
	// 추가한 부분
	bool TravelToLevel(const TSoftObjectPtr<UWorld>& TargetLevel);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Level", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UWorld> TitleLevel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Level", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UWorld> LobbyLevel;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SF|Stage", meta = (AllowPrivateAccess = "true"))
	FName SelectedStageRowName = "Stage01";

	// 추가 부분 26.08.10
	// 가장 높은 순서의 클리어 스테이지
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SF|Stage", meta = (AllowPrivateAccess = "true"))
	int32 HighestClearedStageOrder = 0;
	
	// 모든 종족 리스트
	UPROPERTY(EditDefaultsOnly,	BlueprintReadOnly, Category = "SF|Race", meta = (AllowPrivateAccess = "true"))
	TMap<ESFRace, TSoftObjectPtr<USFBuildingCatalog>> CatalogsByRace;
	// 플레이어 종족 : 기본값 = human
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SF|Race",	meta = (AllowPrivateAccess = "true"))
	ESFRace SelectedRace = ESFRace::Human;
};
