#include "SFBuildCatalogPanelWidget.h"

#include "SFBuildCatalogEntryWidget.h"
#include "Components/HorizontalBox.h"

void USFBuildCatalogPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RefreshBuildingEntries();
}

void USFBuildCatalogPanelWidget::RefreshBuildingEntries()
{
	if (!IsValid(BuildingEntryHorizontalBox) || !BuildingEntryWidgetClass)
	{
		return;
	}

	BuildingEntryHorizontalBox->ClearChildren();

	for (const FName& BuildingRowName : TestBuildingRowNames)
	{
		USFBuildCatalogEntryWidget* EntryWidget =
			CreateWidget<USFBuildCatalogEntryWidget>(
				GetOwningPlayer(),
				BuildingEntryWidgetClass
			);

		if (!IsValid(EntryWidget))
		{
			continue;
		}

		EntryWidget->InitializeBuildingEntry(BuildingRowName);

		BuildingEntryHorizontalBox->AddChildToHorizontalBox(EntryWidget);
	}
}