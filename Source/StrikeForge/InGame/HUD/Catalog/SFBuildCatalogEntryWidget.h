#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"

#include "InGame/HUD/Catalog/SFBuildCatalogUIData.h"

#include "SFBuildCatalogEntryWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FSFOnBuildCatalogEntryClicked,
	FName,
	BuildingRowName
);

UCLASS()
class STRIKEFORGE_API USFBuildCatalogEntryWidget
	: public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "SF|Build Catalog")
	void InitializeBuildingEntry(
		FName InBuildingRowName
	);

	UFUNCTION(BlueprintPure, Category = "SF|Build Catalog")
	FName GetBuildingRowName() const;

	UPROPERTY(
		BlueprintAssignable,
		Category = "SF|Build Catalog"
	)
	
	FSFOnBuildCatalogEntryClicked OnBuildCatalogEntryClicked;

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

private:
	void HandleEntryClicked();

	UPROPERTY(Transient)
	FName BuildingRowName = NAME_None;
};