#include "SFBuildCatalogPanelWidget.h"
#include "SFBuildCatalogEntryWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Logging/SFLog.h"
#include "Datas/Building/SFBuildingCatalog.h"
#include "Datas/Building/SFBuildingDefinition.h"
#include "Game/GameState/SFGameState.h"
#include "Game/GameState/SFTeamState.h"
#include "InGame/Controller/SFInGamePlayerController.h"

void USFBuildCatalogPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BoundGameState =
		GetWorld()
		? GetWorld()->GetGameState<ASFGameState>()
		: nullptr;

	if (!IsValid(BoundGameState))
	{
		return;
	}

	BoundGameState->OnStageStateChanged.RemoveDynamic(
		this,
		&ThisClass::HandleStageStateChanged);

	BoundGameState->OnStageStateChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleStageStateChanged);

	if (BoundGameState->IsStagePlaying())
	{
		BindResourceData();
		RefreshBuildingEntries();
	}
}

void USFBuildCatalogPanelWidget::NativeDestruct()
{
	if (IsValid(BoundGameState))
	{
		BoundGameState->OnStageStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleStageStateChanged);
	}

	UnbindResourceData();

	CurrentEntryWidgets.Reset();

	BoundGameState = nullptr;
	
	Super::NativeDestruct();
}

void USFBuildCatalogPanelWidget::HandleStageStateChanged(ESFStageState NewState)
{
	if (NewState == ESFStageState::Playing)
	{
		BindResourceData();
		RefreshBuildingEntries();
	}
}

void USFBuildCatalogPanelWidget::RefreshBuildingEntries()
{
	CurrentEntryWidgets.Reset();

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

		CurrentEntryWidgets.Add(
			EntryWidget
		);

		SF_LOG(
			Log,
			"[SFBuildCatalogPanelWidget] "
			"Entry Created: %s",
			*Definition->BuildingId.ToString());
	}

	RefreshEntryAffordability();
}

void USFBuildCatalogPanelWidget::RefreshEntryAffordability()
{
	if (!IsValid(BoundGameState))
	{
		return;
	}

	for (
		USFBuildCatalogEntryWidget* EntryWidget
		: CurrentEntryWidgets
		)
	{
		if (!IsValid(EntryWidget))
		{
			continue;
		}

		const FResources ConstructionCost =
			EntryWidget->GetConstructionCost();

		const bool bCanAfford =
			BoundGameState->CanAffordResource(
				CachedTeamIndex,
				ConstructionCost
			);

		EntryWidget->SetCanAfford(
			bCanAfford
		);
	}
}

void USFBuildCatalogPanelWidget::BindResourceData()
{
	UnbindResourceData();

	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return;
	}

	if (!IsValid(BoundGameState))
	{
		return;
	}

	ASFInGamePlayerController* SFPC =
		Cast<ASFInGamePlayerController>(
			GetOwningPlayer()
		);

	if (IsValid(SFPC))
	{
		CachedTeamIndex =
			SFPC->GetMyTeamIndex();
	}

	USFTeamState* TeamState =
		BoundGameState->FindOrAddTeam(
			CachedTeamIndex
		);

	if (!IsValid(TeamState))
	{
		return;
	}

	TeamState->OnMineralChanged.AddUniqueDynamic(
		this,
		&USFBuildCatalogPanelWidget::HandleMineralChanged
	);

	TeamState->OnEtherChanged.AddUniqueDynamic(
		this,
		&USFBuildCatalogPanelWidget::HandleEtherChanged
	);
}

void USFBuildCatalogPanelWidget::UnbindResourceData()
{
	if (!IsValid(BoundGameState))
	{
		return;
	}

	USFTeamState* TeamState =
		BoundGameState->GetTeamState(
			CachedTeamIndex
		);

	if (IsValid(TeamState))
	{
		TeamState->OnMineralChanged.RemoveDynamic(
			this,
			&USFBuildCatalogPanelWidget::HandleMineralChanged
		);

		TeamState->OnEtherChanged.RemoveDynamic(
			this,
			&USFBuildCatalogPanelWidget::HandleEtherChanged
		);
	}
}

void USFBuildCatalogPanelWidget::HandleMineralChanged(
	int32 NewMineral,
	float NewIncomeRate
)
{
	RefreshEntryAffordability();
}

void USFBuildCatalogPanelWidget::HandleEtherChanged(
	int32 NewEther,
	float NewIncomeRate
)
{
	RefreshEntryAffordability();
}