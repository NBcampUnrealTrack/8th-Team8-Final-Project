#include "SFBuildCatalogEntryWidget.h"

#include "Engine/GameInstance.h"

#include "Game/Subsystem/SFDataTableManager.h"
#include "Logging/SFLog.h"

void USFBuildCatalogEntryWidget::InitializeBuildingEntry(
	FName InBuildingRowName
)
{
	BuildingRowName = InBuildingRowName;

	if (BuildingRowName.IsNone())
	{
		SF_LOG(
			Warning,
			"[SFBuildCatalogEntryWidget] "
			"BuildingRowName is None."
		);

		return;
	}

	UGameInstance* GameInstance =
		GetGameInstance();

	if (!IsValid(GameInstance))
	{
		SF_LOG(
			Error,
			"[SFBuildCatalogEntryWidget] "
			"GameInstance is invalid."
		);

		return;
	}

	USFDataTableManager* DataTableManager =
		GameInstance->GetSubsystem<USFDataTableManager>();

	if (!IsValid(DataTableManager))
	{
		SF_LOG(
			Error,
			"[SFBuildCatalogEntryWidget] "
			"DataTableManager is invalid."
		);

		return;
	}

	const FSFBuildCatalogUIDataRow* CatalogData =
		DataTableManager
		->GetDataRow<FSFBuildCatalogUIDataRow>(
			EDataTableType::BuildCatalogUI,
			BuildingRowName,
			TEXT("SFBuildCatalogEntryWidget")
		);

	if (CatalogData == nullptr)
	{
		SF_LOG(
			Warning,
			"[SFBuildCatalogEntryWidget] "
			"Catalog data was not found. Row: %s",
			*BuildingRowName.ToString()
		);

		return;
	}

	OnBuildingEntryInitialized(
		BuildingRowName,
		*CatalogData
	);

	SF_LOG(
		Log,
		"[SFBuildCatalogEntryWidget] "
		"Initialized | Row: %s | "
		"Mineral: %d | Ether: %d | Tier: %d",
		*BuildingRowName.ToString(),
		CatalogData->MineralCost,
		CatalogData->EtherCost,
		CatalogData->RequiredTier
	);
}

FName USFBuildCatalogEntryWidget::GetBuildingRowName() const
{
	return BuildingRowName;
}

void USFBuildCatalogEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	OnClicked().RemoveAll(this);

	OnClicked().AddUObject(
		this,
		&USFBuildCatalogEntryWidget::HandleEntryClicked
	);
}

void USFBuildCatalogEntryWidget::NativeDestruct()
{
	OnClicked().RemoveAll(this);

	Super::NativeDestruct();
}

void USFBuildCatalogEntryWidget::HandleEntryClicked()
{
	if (BuildingRowName.IsNone())
	{
		SF_LOG(
			Warning,
			"[SFBuildCatalogEntryWidget] "
			"Clicked entry has no BuildingRowName."
		);

		return;
	}

	SF_LOG(
		Log,
		"[SFBuildCatalogEntryWidget] "
		"Entry Clicked: %s",
		*BuildingRowName.ToString()
	);

	OnBuildCatalogEntryClicked.Broadcast(
		BuildingRowName
	);
}