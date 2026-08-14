#include "SFBuildCatalogPanelWidget.h"

#include "SFBuildCatalogEntryWidget.h"

#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"

#include "Game/Subsystem/SFDataTableManager.h"
#include "Logging/SFLog.h"

void USFBuildCatalogPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RefreshBuildingEntries();
}

void USFBuildCatalogPanelWidget::RefreshBuildingEntries()
{
	if (
		!IsValid(BuildingEntryHorizontalBox) ||
		!BuildingEntryWidgetClass
		)
	{
		return;
	}

	BuildingEntryHorizontalBox->ClearChildren();

	UGameInstance* GameInstance =
		GetGameInstance();

	if (!IsValid(GameInstance))
	{
		SF_LOG(
			Error,
			"[SFBuildCatalogPanelWidget] "
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
			"[SFBuildCatalogPanelWidget] "
			"DataTableManager is invalid."
		);

		return;
	}

	const TArray<FName> BuildingRowNames =
		DataTableManager->GetDataRowNames(
			EDataTableType::BuildCatalogUI
		);

	SF_LOG(
		Log,
		"[SFBuildCatalogPanelWidget] "
		"Build Catalog Row Count: %d",
		BuildingRowNames.Num()
	);

	for (const FName& BuildingRowName : BuildingRowNames)
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

		EntryWidget->InitializeBuildingEntry(
			BuildingRowName
		);
		
		UHorizontalBoxSlot* EntrySlot =
			BuildingEntryHorizontalBox
			->AddChildToHorizontalBox(
				EntryWidget
			);

		if (IsValid(EntrySlot))
		{
			EntrySlot->SetPadding(
				FMargin(
					5.0f,
					0.0f,
					5.0f,
					0.0f
				)
			);
		}

		SF_LOG(
			Log,
			"[SFBuildCatalogPanelWidget] "
			"Entry Created: %s",
			*BuildingRowName.ToString()
		);
	}
}