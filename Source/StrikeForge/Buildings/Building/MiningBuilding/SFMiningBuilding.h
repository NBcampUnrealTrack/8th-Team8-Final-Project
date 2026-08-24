#pragma once

#include "CoreMinimal.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "SFMiningBuilding.generated.h"

class UMiningComponent;
class AResourceBase;
enum class EResourceType : uint8;

UCLASS()
class STRIKEFORGE_API ASFMiningBuilding : public ASFBaseBuilding
{
	GENERATED_BODY()

public:
	ASFMiningBuilding();
	virtual FSFSelectableInfo GetSelectableInfo_Implementation() override;
	
	void BindDelegateFunction();
	void UnBindDelegateFunction();
	virtual void OnBuildingDefinitionLoaded(const FBuildingAppearance& Definition) override;
	// APPLY BUILDING's ABILITY
	virtual void OnTierActivated() override;
	//
	virtual void OnUpgradeStarted() override;
	// DELETE BUILDING's ABILITY
	// OnDestroyed1
	virtual void OnTierDeactivated() override;
	// when construct destroyed, play effects
	// OnDestroyed2
	virtual void OnBuildingDestroyed() override;
	
	bool TryLinkResource(AResourceBase* InResourceRef);

	// getter
	EResourceType GetMineResourceType() const;
	int32 GetExtractAmount() const;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	void ApplyIncomeRate(float NewRate);

	UFUNCTION()
	void OnResourceDepletedReceived();
	UFUNCTION()
	void OnResourceExtractedReceived(EResourceType InType, int32 InAmount);
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UMiningComponent> MiningComponent;
private:
	float CurrentIncomeRatePerSecond = 0.f;
};
