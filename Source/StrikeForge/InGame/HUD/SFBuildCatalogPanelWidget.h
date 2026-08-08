#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFBuildCatalogPanelWidget.generated.h"

class UHorizontalBox;
class USFBuildCatalogEntryWidget;

UCLASS()
class STRIKEFORGE_API USFBuildCatalogPanelWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

private:
	void RefreshBuildingEntries();

	// ==================== Variables ====================

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> BuildingEntryHorizontalBox;

	UPROPERTY(EditDefaultsOnly, Category = "SF|Build Catalog")
	TSubclassOf<USFBuildCatalogEntryWidget> BuildingEntryWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "SF|Build Catalog")
	TArray<FName> TestBuildingRowNames;
};