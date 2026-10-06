// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "Interfaces/Building/Productable.h"
#include "Interfaces/Building/SFProductionRateModifiable.h"
#include "SFProductionBuilding.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FProductUnitArrayChanged, const TArray<FName>&, SpawnableRowNames);

class UProductComponent;

// UENUM()
// enum class EBuildingWorkStates : uint8
// {
// 	Idle,
// 	Working,
// 	Paused,
// 	Blocked,
// };

UCLASS()
class STRIKEFORGE_API ASFProductionBuilding
	: public ASFBaseBuilding
	, public IProductable
	, public ISFProductionRateModifiable
{
	GENERATED_BODY()
#if WITH_DEV_AUTOMATION_TESTS
	friend class FSFStage03ProductionTest;
#endif

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
	/** 지휘관 스킬 배율을 별도 식별자로 등록하여 다른 생산 보너스와 함께 적용합니다. */
	void ApplySpeedBuff(float InMultiflyValue);
	/** 지휘관 스킬 식별자의 배율만 제거하고 다른 생산 보너스는 유지합니다. */
	void ResetSpeedMultiplier();
	/** 생산 건물용 인터페이스 요청을 실제 ProductComponent 배율 등록으로 전달합니다. */
	virtual void SetProductionSpeedModifier(FName ModifierId, float InMultiplier) override;
	/** ProductComponent에서 지정된 출처의 생산 배율만 제거합니다. */
	virtual void RemoveProductionSpeedModifier(FName ModifierId) override;
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnBuildingDefinitionLoaded() override;
	
	virtual void OnTierActivated() override;
	virtual void OnUpgradeStarted() override;
	virtual void OnBuildingDestroyed() override;

	virtual FSFSelectableInfo GetSelectableInfo_Implementation() override;
	virtual void ProductUnits_Implementation(FName InUnitName) override;
	virtual FProductableData GetProductableData_Implementation() override;

	void TryAddUnitProductLine(FName InUnitName);

	UFUNCTION()
	void HandleProductLineChanged(TArray<FProductInfo> InProductLine);
	
	UPROPERTY()
	TObjectPtr<UProductComponent> ProductComponent;

	TArray<FName> StoredSpawnableUnitDatas;
	
	// 집결지 위치벡터
	//FVector AssemblePoint;
};
