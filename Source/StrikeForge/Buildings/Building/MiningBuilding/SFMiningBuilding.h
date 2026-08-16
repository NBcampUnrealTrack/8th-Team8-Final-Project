#pragma once

#include "CoreMinimal.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "SFMiningBuilding.generated.h"

UCLASS()
class STRIKEFORGE_API ASFMiningBuilding : public ASFBaseBuilding
{
	GENERATED_BODY()

public:
	ASFMiningBuilding();
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
	
private:
	// Mining Income Rate
	float MiningIncomeRate;
};
