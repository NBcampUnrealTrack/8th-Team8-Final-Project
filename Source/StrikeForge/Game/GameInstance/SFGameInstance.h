#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"

// 추가부분 26.08.10
#include "Datas/Stage/SFStageData.h"

#include "SFGameInstance.generated.h"

// 수정부분
class UWorld;
class USFBuildingCatalog;

UENUM(BlueprintType)
enum class EDataTableType : uint8
{
	UI,
	BuildCatalogUI,
	Building,
	Unit,
	Stage,	// 추가부분
	Product,
	ProductionUI
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
	
	// 종족별 카탈로그
	USFBuildingCatalog* LoadPlayerBuildingCatalog() const;
private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataTable|Building", meta = (AllowPrivateAccess = true))
	TObjectPtr<UDataTable> BuildingDataTable;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataTable|UI", meta = (AllowPrivateAccess = true))
	TObjectPtr<UDataTable> UIDataTable;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataTable|UI", meta = (AllowPrivateAccess = true))
	TObjectPtr<UDataTable> BuildCatalogUIDataTable;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataTable|Unit", meta = (AllowPrivateAccess = true))
	TObjectPtr<UDataTable> UnitsDataTable;

	// 추가 부분
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataTable|Game", meta = (AllowPrivateAccess = true))
	TObjectPtr<UDataTable> StageDataTable;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataTable|Building", meta = (AllowPrivateAccess = true))
	TObjectPtr<UDataTable> ProductDataTable;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category = "DataTable|UI",meta = (AllowPrivateAccess = true))
	TObjectPtr<UDataTable> ProductionUIDataTable;
	// 추가한 부분
	bool TravelToLevel(const TSoftObjectPtr<UWorld>& TargetLevel);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Level", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UWorld> TitleLevel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Level", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UWorld> LobbyLevel;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SF|Stage", meta = (AllowPrivateAccess = "true"))
	FName SelectedStageRowName = NAME_None;

	// 추가 부분 26.08.10
	// 가장 높은 순서의 클리어 스테이지
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SF|Stage", meta = (AllowPrivateAccess = "true"))
	int32 HighestClearedStageOrder = 0;
	
	// 플레이어 종족
	/**
	 * 플레이어가 선택한 종족의 건물 카탈로그.
	 *
	 * 현재는 BP_SFGameInstance 기본값으로 Human 카탈로그를 사용한다.
	 * 추후 종족 선택 기능이 생기면 선택 결과로 교체한다.
	 */
	UPROPERTY(EditDefaultsOnly,	BlueprintReadOnly, Category = "SF|Race", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<USFBuildingCatalog> PlayerBuildingCatalog;
};
