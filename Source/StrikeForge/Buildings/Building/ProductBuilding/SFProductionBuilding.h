// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Buildings/Building/SFBaseBuilding.h"
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
	
	//HUD가 ProductComponent를 직접 만지지 않도록 생산 건물에 래퍼 함수를 추가
	UFUNCTION(BlueprintPure, Category = "SF|Production|Rally")
	bool HasRallyPoint() const;

	UFUNCTION(BlueprintPure, Category = "SF|Production|Rally")
	FVector GetRallyPoint() const;

	// setter
	void SetAssembleVectorPoint(const FVector& InAssemblePoint);
	void ApplySpeedBuff(float InMultiflyValue) const;
	void ResetSpeedMultiplier();
protected:
	virtual void OnTierActivated() override;
	virtual void OnUpgradeStarted() override;
	virtual void OnBuildingDestroyed() override;

	virtual FSFSelectableInfo GetSelectableInfo_Implementation() override;
	virtual void ProductUnits_Implementation(FName InUnitName) override;
	virtual FProductableData GetProductableData_Implementation() override;

	void TryAddUnitProductLine(FName InUnitName);

	UPROPERTY()
	TObjectPtr<UProductComponent> ProductComponent;

	TArray<FName> StoredSpawnableUnitDatas;
	
	// 집결지 위치벡터
	//FVector AssemblePoint;
};
