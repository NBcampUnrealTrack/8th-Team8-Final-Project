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
	virtual void OnTierActivated() override;

	virtual void OnTierDeactivated() override;
	
	void SetProgressColorByResourceType(EResourceType InResourceType);
	// IMinable
	virtual bool SetMiningTargets(const TArray<FSFMiningCellTarget>& InTargets) override;
	virtual void ClearMiningTargets() override;
	
	virtual FIntPoint GetResourceSearchGridSize() const override;
	
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
