#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "SFBuildCatalogEntryWidget.generated.h"

UCLASS()
class STRIKEFORGE_API USFBuildCatalogEntryWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "SF|Build Catalog")
	void InitializeBuildingEntry(FName InBuildingRowName);

	UFUNCTION(BlueprintPure, Category = "SF|Build Catalog")
	FName GetBuildingRowName() const;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "SF|Build Catalog")
	void OnBuildingEntryInitialized(FName InBuildingRowName);

private:
	UPROPERTY(Transient)
	FName BuildingRowName = NAME_None;
};