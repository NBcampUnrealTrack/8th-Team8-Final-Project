// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "Datas/Unit/UnitProductData.h"
#include "Interfaces/Building/Productable.h"
#include "SFProductionBuilding.generated.h"

// USTRUCT(BlueprintType)
// struct FSpawnableUnitsData
// {
// 	GENERATED_BODY()
// 	
// 	FName SpawnableUnitName;
// 	FResources
// };


class UProductComponent;

UCLASS()
class STRIKEFORGE_API ASFProductionBuilding 
	: public ASFBaseBuilding
	, public IProductable
{
	GENERATED_BODY()

public:
	ASFProductionBuilding();

protected:

	virtual void BeginPlay() override;
	virtual void OnConstructionFinished() override;
	virtual void InitInfo(const FBuildingAppearance* InData) override;
	
	virtual void ProductUnits_Implementation(FName InUnitName) override;

	void TryAddUnitProductLine(FName InUnitName);
	
	void UpdateSpawnableUnitNames(int32 InCurrentTier);
protected:
	UPROPERTY()
	TObjectPtr<UProductComponent> ProductComponent;

	TArray<FName> StoredSpawnableUnitDatas;
};
