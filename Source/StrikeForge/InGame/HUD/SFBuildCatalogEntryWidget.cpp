#include "SFBuildCatalogEntryWidget.h"

void USFBuildCatalogEntryWidget::InitializeBuildingEntry(FName InBuildingRowName)
{
	BuildingRowName = InBuildingRowName;

	OnBuildingEntryInitialized(BuildingRowName);
}

FName USFBuildCatalogEntryWidget::GetBuildingRowName() const
{
	return BuildingRowName;
}