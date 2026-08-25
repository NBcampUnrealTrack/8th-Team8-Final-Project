#include "SFBuildCatalogEntryWidget.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "Logging/SFLog.h"
#include "Datas/Building/SFBuildingDefinition.h"
#include "Datas/Building/BuildingTierData/SFBuildingTierData.h"

void USFBuildCatalogEntryWidget::InitializeBuildingEntry(
USFBuildingDefinition* InDefinition)
{
	if (!IsValid(InDefinition))
	{
		SF_LOG(
			Error,
			"[SFBuildCatalogEntryWidget] "
			"BuildingDefinition이 유효하지 않습니다.");

		return;
	}

	BuildingDefinition = InDefinition;
	BuildingRowName = BuildingDefinition->BuildingId;

	if (BuildingRowName.IsNone())
	{
		SF_LOG(
			Error,
			"[SFBuildCatalogEntryWidget] "
			"BuildingId가 비어 있습니다. Definition=%s",
			*GetNameSafe(BuildingDefinition));

		return;
	}

	const FSFBuildingTierData* FirstTier =
		BuildingDefinition->FindTier(0);

	if (!FirstTier)
	{
		SF_LOG(
			Error,
			"[SFBuildCatalogEntryWidget] "
			"첫 번째 Tier가 없습니다. Building=%s",
			*BuildingRowName.ToString());

		return;
	}

	/*
	 * 기존 WBP 이벤트 시그니처를 유지하기 위한 UI ViewData다.
	 * 값의 원본은 이제 DT가 아니라 Definition이다.
	 */
	FSFBuildCatalogUIDataRow CatalogData;

	CatalogData.DisplayName =
		BuildingDefinition->DisplayName;

	CatalogData.Icon =
		BuildingDefinition->Icon;

	CatalogData.MineralCost =
		FirstTier->Construction.ConstructCost.Mineral;

	CatalogData.EtherCost =
		FirstTier->Construction.ConstructCost.Ether;

	CatalogData.RequiredTier =
		BuildingDefinition->RequiredNexusTier;

	OnBuildingEntryInitialized(
		BuildingRowName,
		CatalogData);

	SF_LOG(
		Log,
		"[SFBuildCatalogEntryWidget] "
		"Initialized | Building=%s | Mineral=%d | Ether=%d | Tier=%d",
		*BuildingRowName.ToString(),
		CatalogData.MineralCost,
		CatalogData.EtherCost,
		CatalogData.RequiredTier);
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
{// horizontal child widghet clicked
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
	
	// onclicked spawn building by rowname ======================================================================================
	ASFInGamePlayerController* SFPC = Cast<ASFInGamePlayerController>(GetOwningPlayer());
	if (!IsValid(SFPC))
	{
		UE_LOG(LogTemp, Warning, TEXT("USFBuildCatalogEntryWidget::HandleEntryClicked: SFInGamePlayerController IS INVALID"));
		return;
	}
	
	// 자원이 충분한가 아닌가 검사 로직. 가능구역1
	if (SFPC->SpawnBuildingFunction(BuildingRowName))
	{
		SFPC->SetSFControlMode(ESFControlMode::BuildPlacement);
	}
}