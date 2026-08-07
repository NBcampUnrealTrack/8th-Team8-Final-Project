#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "SFGameInstance.generated.h"

// 수정부분
class UWorld;

UENUM(BlueprintType)
enum class EDataTableType : uint8
{
	UI,
	Building,
	Unit,
	Stage	// 추가부분
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
private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataTable", meta = (AllowPrivateAccess = true))
	TObjectPtr<UDataTable> BuildingDataTable;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataTable", meta = (AllowPrivateAccess = true))
	TObjectPtr<UDataTable> UIDataTable;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataTable", meta = (AllowPrivateAccess = true))
	TObjectPtr<UDataTable> UnitsDataTable;

	// 추가 부분
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataTable", meta = (AllowPrivateAccess = true))
	TObjectPtr<UDataTable> StageDataTable;

	// 추가한 부분
	bool TravelToLevel(const TSoftObjectPtr<UWorld>& TargetLevel);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Level", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UWorld> TitleLevel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Level", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UWorld> LobbyLevel;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "SF|Stage", meta = (AllowPrivateAccess = "true"))
	FName SelectedStageRowName = NAME_None;
};
