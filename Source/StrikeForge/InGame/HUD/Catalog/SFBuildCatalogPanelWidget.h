#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFBuildCatalogPanelWidget.generated.h"

class UHorizontalBox;
class USFBuildCatalogEntryWidget;
class ASFGameState;
class USFTeamState;

UCLASS()
class STRIKEFORGE_API USFBuildCatalogPanelWidget
	: public UUserWidget
{
	GENERATED_BODY()

	//==================== = Functions ====================

private:
	void BindResourceData();

	void UnbindResourceData();

	void RefreshEntryAffordability();

	UFUNCTION()
	void HandleMineralChanged(
		int32 NewMineral,
		float NewIncomeRate
	);

	UFUNCTION()
	void HandleEtherChanged(
		int32 NewEther,
		float NewIncomeRate
	);
	
protected:
	virtual void NativeConstruct() override;

	virtual void NativeDestruct() override;

	// ==================== Variables ====================
private:
	void RefreshBuildingEntries();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> BuildingEntryHorizontalBox;

	UPROPERTY(EditDefaultsOnly, Category = "SF|Build Catalog")
	TSubclassOf<USFBuildCatalogEntryWidget> BuildingEntryWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<ASFGameState> BoundGameState = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USFBuildCatalogEntryWidget>>
		CurrentEntryWidgets;

	int32 CachedTeamIndex = 0;
};