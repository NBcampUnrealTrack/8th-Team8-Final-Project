#pragma once

#include "CoreMinimal.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "Buildings/Resource/Interface/SFMinable.h"
#include "Interfaces/Building/SFProductionRateModifiable.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "SFMiningBuilding.generated.h"

class UMiningComponent;

UCLASS()
class STRIKEFORGE_API ASFMiningBuilding 
	: public ASFBaseBuilding
	, public IMinable
	, public ISFProductionRateModifiable
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

	/** 생산속도 인터페이스 요청을 MiningComponent의 작업 배율로 전달합니다. */
	virtual void SetProductionSpeedModifier(FName ModifierId, float InMultiplier) override;
	/** MiningComponent에서 지정된 출처의 작업 배율만 제거합니다. */
	virtual void RemoveProductionSpeedModifier(FName ModifierId) override;
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
