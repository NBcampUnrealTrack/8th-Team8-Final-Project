#pragma once

#include "CoreMinimal.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "Buildings/Resource/Interface/SFMinable.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "SFMiningBuilding.generated.h"

class UMiningComponent;

UCLASS()
class STRIKEFORGE_API ASFMiningBuilding 
	: public ASFBaseBuilding
	, public IMinable
{
	GENERATED_BODY()

public:
	ASFMiningBuilding();
	virtual FSFSelectableInfo GetSelectableInfo_Implementation() override;
	virtual const FMinableData GetMinableData() override;
	
	void BindDelegateFunction();
	void UnBindDelegateFunction();
	virtual void OnBuildingDefinitionLoaded() override;
	// APPLY BUILDING's ABILITY
	virtual void OnTierActivated() override;

	// DELETE BUILDING's ABILITY
	// OnDestroyed1
	virtual void OnTierDeactivated() override;
	
	void SetProgressColorByResourceType(EResourceType InResourceType);
	// Minable
	virtual bool SetMiningTargets(const TArray<FSFMiningCellTarget>& InTargets) override;
	virtual void ClearMiningTargets() override;
	
	virtual FIntPoint GetResourceSearchGridSize() const override;
	
	// getter ui 초기값 설정
	virtual EResourceType GetMineResourceType() const override;
	int32 GetExtractAmount() const;
	int32 GetResourceAmount() const;
	float GetMiningProgressRatio() const;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	void ApplyIncomeRate(float NewRate);
	void RefreshMiningState();
	
	UFUNCTION()
	void OnResourceDepletedReceived();
	UFUNCTION()
	void OnResourceExtractedReceived(EResourceType InType, int32 InAmount);
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UMiningComponent> MiningComponent;
private:
	float CurrentIncomeRatePerSecond = 0.f;
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Mining|Grid", meta = (AllowPrivateAccess = true))
	FIntPoint ConfiguredResourceSearchGridSize = FIntPoint(1, 1);
};
