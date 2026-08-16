// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "Datas/Unit/UnitProductData.h"
#include "Interfaces/Building/Productable.h"
#include "SFProductionBuilding.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FProductUnitArrayChanged, const TArray<FName>&, SpawnableRowNames);

class UProductComponent;

UCLASS()
class STRIKEFORGE_API ASFProductionBuilding
	: public ASFBaseBuilding
	, public IProductable
{
	GENERATED_BODY()

public:
	ASFProductionBuilding(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(BlueprintAssignable, Category = "Production")
	FProductUnitArrayChanged OnSpawnableUnitsChanged;

protected:
	virtual void OnTierActivated(const FBuildingTier& ActivatedTier) override;
	virtual void OnUpgradeStarted() override;
	virtual void OnTierDeactivated(const FBuildingTier& DeactivatedTier) override;
	virtual void OnBuildingDestroyed() override;

	virtual FSFSelectableInfo GetSelectableInfo_Implementation() override;
	virtual void ProductUnits_Implementation(FName InUnitName) override;
	virtual FProductableData GetProductableData_Implementation() override;

	void TryAddUnitProductLine(FName InUnitName);

	UPROPERTY()
	TObjectPtr<UProductComponent> ProductComponent;

	TArray<FName> StoredSpawnableUnitDatas;
};
