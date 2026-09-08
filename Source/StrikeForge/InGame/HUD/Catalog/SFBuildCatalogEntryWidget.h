#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "Datas/Resource/SFResourceTypes.h"

#include "InGame/HUD/Catalog/SFBuildCatalogUIData.h"

#include "SFBuildCatalogEntryWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FSFOnBuildCatalogEntryClicked,
	FName,
	BuildingRowName
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSFOnBuildResourceWarningRequested);

class USFBuildingDefinition;
class USoundBase;

UCLASS()
class STRIKEFORGE_API USFBuildCatalogEntryWidget
	: public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "SF|Build Catalog")
	void InitializeBuildingEntry(
	USFBuildingDefinition* InDefinition);

	UFUNCTION(BlueprintPure, Category = "SF|Build Catalog")
	FName GetBuildingRowName() const;

	UPROPERTY(
		BlueprintAssignable,
		Category = "SF|Build Catalog"
	)
	
	FSFOnBuildCatalogEntryClicked OnBuildCatalogEntryClicked;

	UPROPERTY(
		BlueprintAssignable,
		Category = "SF|Build Catalog|Sound"
	)
	FSFOnBuildResourceWarningRequested OnBuildResourceWarningRequested;

	UFUNCTION(
		BlueprintCallable,
		Category = "SF|Build Catalog"
	)
	void SetCanAfford(
		bool bNewCanAfford
	);

	UFUNCTION(BlueprintPure,Category = "SF|Build Catalog")
	bool CanAfford() const
	{
		return bCanAfford;
	}

	UFUNCTION(
		BlueprintPure,
		Category = "SF|Build Catalog"
	)
	FResources GetConstructionCost() const;

protected:
	virtual void NativeConstruct() override;

	virtual void NativeDestruct() override;

	UFUNCTION(
		BlueprintImplementableEvent,
		Category = "SF|Build Catalog"
	)
	void OnBuildingEntryInitialized(
		FName InBuildingRowName,
		const FSFBuildCatalogUIDataRow& InCatalogData
	);

	UFUNCTION(
		BlueprintImplementableEvent,
		Category = "SF|Build Catalog"
	)
	void BP_OnAffordabilityChanged(
		bool bNewCanAfford
	);

private:
	void HandleEntryClicked();

	UPROPERTY(Transient)
	FName BuildingRowName = NAME_None;
	// 현재종족의 건물 종합 카테고리
	UPROPERTY(Transient)
	TObjectPtr<USFBuildingDefinition> BuildingDefinition;

	UPROPERTY(Transient)
	bool bCanAfford = true;

protected:
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "SF|Build Catalog|Sound"
	)
	TObjectPtr<USoundBase> ResourceWarningSound = nullptr;
};