#include "SFBuildCatalogPanelWidget.h"
#include "SFBuildCatalogEntryWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Logging/SFLog.h"
#include "Datas/Building/SFBuildingCatalog.h"
#include "Datas/Building/SFBuildingDefinition.h"
#include "Game/GameState/SFGameState.h"
#include "InGame/Controller/SFInGamePlayerController.h"

void USFBuildCatalogPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CachedGameState =
		GetWorld()
		? GetWorld()->GetGameState<ASFGameState>()
		: nullptr;

	if (!IsValid(CachedGameState))
	{
		return;
	}

	CachedGameState->OnStageStateChanged.RemoveDynamic(
		this,
		&ThisClass::HandleStageStateChanged);

	CachedGameState->OnStageStateChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleStageStateChanged);

	if (CachedGameState->IsStagePlaying())
	{
		RefreshBuildingEntries();
	}
}

void USFBuildCatalogPanelWidget::NativeDestruct()
{
	if (IsValid(CachedGameState))
	{
		CachedGameState->OnStageStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleStageStateChanged);
	}

	CachedGameState = nullptr;
	
	Super::NativeDestruct();
}

void USFBuildCatalogPanelWidget::HandleStageStateChanged(ESFStageState NewState)
{
	if (NewState == ESFStageState::Playing)
	{
		RefreshBuildingEntries();
	}
}

void USFBuildCatalogPanelWidget::RefreshBuildingEntries()
{
	if (!IsValid(BuildingEntryHorizontalBox) ||
		!BuildingEntryWidgetClass)
	{
		return;
	}

	BuildingEntryHorizontalBox->ClearChildren();

	ASFInGamePlayerController* SFPC =
		Cast<ASFInGamePlayerController>(
			GetOwningPlayer());

	if (!IsValid(SFPC))
	{
		SF_LOG(
			Error,
			"[SFBuildCatalogPanelWidget] "
			"PlayerController가 유효하지 않습니다.");

		return;
	}

	ASFGameState* SFGameState =
		GetWorld()->GetGameState<ASFGameState>();

	if (!IsValid(SFGameState))
	{
		SF_LOG(
			Error,
			"[SFBuildCatalogPanelWidget] "
			"GameState가 유효하지 않습니다.");

		return;
	}

	USFBuildingCatalog* Catalog =
		SFGameState->GetTeamBuildingCatalog(
			SFPC->GetMyTeamIndex());

	if (!IsValid(Catalog))
	{
		SF_LOG(
			Error,
			"[SFBuildCatalogPanelWidget] "
			"Team %d의 BuildingCatalog가 없습니다.",
			SFPC->GetMyTeamIndex());

		return;
	}

	for (const TSoftObjectPtr<USFBuildingDefinition>& DefinitionRef
		: Catalog->BuildableDefinitions)
	{
		if (DefinitionRef.IsNull())
		{
			continue;
		}

		USFBuildingDefinition* Definition =
			DefinitionRef.Get();

		if (!IsValid(Definition))
		{
			SF_LOG(
				Warning,
				"[SFBuildCatalogPanelWidget] "
				"Definition 로드에 실패했습니다. Asset=%s",
				*DefinitionRef.ToSoftObjectPath().ToString());

			continue;
		}

		USFBuildCatalogEntryWidget* EntryWidget =
			CreateWidget<USFBuildCatalogEntryWidget>(
				GetOwningPlayer(),
				BuildingEntryWidgetClass);

		if (!IsValid(EntryWidget))
		{
			continue;
		}

		EntryWidget->InitializeBuildingEntry(
			Definition);

		UHorizontalBoxSlot* EntrySlot =
			BuildingEntryHorizontalBox
			->AddChildToHorizontalBox(
				EntryWidget);
		

		if (IsValid(EntrySlot))
		{
			EntrySlot->SetPadding(
				FMargin(5.f, 0.f, 5.f, 0.f));
		}

		SF_LOG(
			Log,
			"[SFBuildCatalogPanelWidget] "
			"Entry Created: %s",
			*Definition->BuildingId.ToString());
	}
}
