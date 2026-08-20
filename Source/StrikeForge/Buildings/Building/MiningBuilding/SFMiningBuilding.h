#pragma once

#include "CoreMinimal.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "SFMiningBuilding.generated.h"

class AResourceBase;
enum class EResourceType : uint8;

UCLASS()
class STRIKEFORGE_API ASFMiningBuilding : public ASFBaseBuilding
{
	GENERATED_BODY()

public:
	ASFMiningBuilding();
	virtual FSFSelectableInfo GetSelectableInfo_Implementation() override;
	
	virtual void OnBuildingDefinitionLoaded(const FBuildingAppearance& Definition) override;
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
	
	void TryLinkResource(AResourceBase* InResourceRef);
	
	// getter
	FORCEINLINE EResourceType GetMineResourceType() const { return CanDigResourceType; }
protected:
	// timer start
	void StartMining();
	void StopMining();
	void PauseMining();
	void UnPauseMining();
	
	void Mining();
	
private:
	// Mining Income Rate
	float DiggingIncomeRate; // = (생산량 / 타이머 인터벌)
	
	int32 DiggingAmount = 5; // 임시하드코딩
	
	float DiggingInterval = 5.f; // 임시하드코딩
	
	FTimerHandle DiggingProductTimerHandle;
	
	EResourceType CanDigResourceType;
	TWeakObjectPtr<AResourceBase> LinkedResource;
};
