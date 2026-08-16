#pragma once

#include "CoreMinimal.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "SFHouseBuilding.generated.h"

UCLASS()
class STRIKEFORGE_API ASFHouseBuilding : public ASFBaseBuilding
{
	GENERATED_BODY()

public:
	ASFHouseBuilding(const FObjectInitializer& ObjectInitializer);
	virtual FSFSelectableInfo GetSelectableInfo_Implementation() override;
	
	// APPLY BUILDING's ABILITY
	virtual void OnTierActivated(const FBuildingTier& ActivatedTier) override;
	//
	virtual void OnUpgradeStarted() override;
	// DELETE BUILDING's ABILITY
	// OnDestroyed1
	virtual void OnTierDeactivated(const FBuildingTier& DeactivatedTier) override;
	// when construct destroyed, play effects
	// OnDestroyed2
	virtual void OnBuildingDestroyed() override;
	
protected:
	// 증가시킬 인구
	UPROPERTY()
	int32 MaxPopulationIncreaseAmount;
private:
	
};
