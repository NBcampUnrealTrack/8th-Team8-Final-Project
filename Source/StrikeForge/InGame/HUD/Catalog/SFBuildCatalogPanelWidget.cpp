#include "SFBuildCatalogPanelWidget.h"

#include "SFBuildCatalogEntryWidget.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "EngineUtils.h"

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
	BoundGameState->OnStageBuildingPermissionsChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleStageBuildingPermissionsChanged);

	if (BoundGameState->IsStagePlaying())
	{
		BindResourceData();
		BindBuildingCompletionEvents();
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
		BoundGameState->OnStageBuildingPermissionsChanged.RemoveDynamic(
			this,
			&ThisClass::HandleStageBuildingPermissionsChanged);
	}

	UnbindBuildingCompletionEvents();
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

void USFBuildCatalogPanelWidget::HandleStageBuildingPermissionsChanged()
{
	RefreshEntryAffordability();
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

		const int32 SlotIndex =
			CurrentEntryWidgets.Num();

		static const TCHAR* BuildHotkeys[] =
		{
			TEXT("H"),
			TEXT("M"),
			TEXT("E"),
			TEXT("C"),
			TEXT("A"),
			TEXT("S")
		};

		EntryWidget->InitializeBuildingEntry(
			Definition
		);

		if (
			SlotIndex >= 0 &&
			SlotIndex < UE_ARRAY_COUNT(BuildHotkeys)
			)
		{
			EntryWidget->SetHotkeyText(
				FText::FromString(
					FString(
						BuildHotkeys[SlotIndex]
					)
				)
			);
		}
		
		UHorizontalBoxSlot* EntrySlot =
			BuildingEntryHorizontalBox
			->AddChildToHorizontalBox(
				EntryWidget);
		

		if (IsValid(EntrySlot))
		{
			EntrySlot->SetPadding(
				FMargin(3.f, 0.f, -8.f, 0.f));
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

void USFBuildCatalogPanelWidget::BindBuildingCompletionEvents()
{
	UWorld* World =
		GetWorld();

	if (!IsValid(World))
	{
		return;
	}

	ActorSpawnedHandle =
		World->AddOnActorSpawnedHandler(
			FOnActorSpawned::FDelegate::CreateUObject(
				this,
				&USFBuildCatalogPanelWidget::HandleBuildingSpawned
			)
		);

	for (
		TActorIterator<ASFBaseBuilding> It(World);
		It;
		++It
		)
	{
		BindBuildingCompletionEvent(
			*It
		);
	}
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

		const FName SpawnBuildingId =
			EntryWidget->GetBuildingRowName();

		const FName TechBuildingId =
			EntryWidget->GetSelectionUIRowName();

		const bool bStageAllowsBuild =
			BoundGameState->IsBuildingAllowedInCurrentStage(
				TechBuildingId
			);

		const bool bTechRequirementsMet =
			BoundGameState->AreBuildingRequirementsMet(
				CachedTeamIndex,
				TechBuildingId
			);

		const bool bCanBuild =
			bStageAllowsBuild &&
			bTechRequirementsMet;

		EntryWidget->SetStageBuildAllowed(
			bCanBuild
		);

		SF_LOG(
			Log,
			"[SFBuildCatalogPanelWidget] "
			"Tech Check | Building: %s | "
			"StageAllowed: %s | "
			"RequirementsMet: %s | "
			"CanBuild: %s",
			*TechBuildingId.ToString(),
			bStageAllowsBuild
			? TEXT("True")
			: TEXT("False"),
			bTechRequirementsMet
			? TEXT("True")
			: TEXT("False"),
			bCanBuild
			? TEXT("True")
			: TEXT("False")
		);

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

void USFBuildCatalogPanelWidget::BindBuildingCompletionEvent(
	ASFBaseBuilding* Building
)
{
	if (!IsValid(Building))
	{
		return;
	}

	if (
		Building->GetTeamIndex() !=
		CachedTeamIndex
		)
	{
		return;
	}

	for (
		const TWeakObjectPtr<ASFBaseBuilding>&
		BoundBuilding :
		BoundBuildings
		)
	{
		if (BoundBuilding.Get() == Building)
		{
			return;
		}
	}

	Building->OnBuildingTierActivated.AddUniqueDynamic(
		this,
		&USFBuildCatalogPanelWidget::HandleBuildingTierActivated
	);

	BoundBuildings.Add(
		Building
	);
}

void USFBuildCatalogPanelWidget::HandleBuildingSpawned(
	AActor* SpawnedActor
)
{
	ASFBaseBuilding* Building =
		Cast<ASFBaseBuilding>(
			SpawnedActor
		);

	if (!IsValid(Building))
	{
		return;
	}

	BindBuildingCompletionEvent(
		Building
	);
}

void USFBuildCatalogPanelWidget::HandleBuildingTierActivated(
	int32 ActivatedTier
)
{
	RefreshEntryAffordability();
}

void USFBuildCatalogPanelWidget::UnbindBuildingCompletionEvents()
{
	UWorld* World =
		GetWorld();

	if (
		IsValid(World) &&
		ActorSpawnedHandle.IsValid()
		)
	{
		World->RemoveOnActorSpawnedHandler(
			ActorSpawnedHandle
		);

		ActorSpawnedHandle.Reset();
	}

	for (
		const TWeakObjectPtr<ASFBaseBuilding>&
		BoundBuilding :
		BoundBuildings
		)
	{
		if (IsValid(BoundBuilding.Get()))
		{
			BoundBuilding->OnBuildingTierActivated.RemoveDynamic(
				this,
				&USFBuildCatalogPanelWidget::HandleBuildingTierActivated
			);
		}
	}

	BoundBuildings.Reset();
}

bool USFBuildCatalogPanelWidget::RequestBuildingBySlot(
	int32 SlotIndex
)
{
	SF_LOG(
		Log,
		"[BuildInput] Catalog Request Slot=%d EntryCount=%d",
		SlotIndex,
		CurrentEntryWidgets.Num()
	);

	if (!CurrentEntryWidgets.IsValidIndex(SlotIndex))
	{
		SF_LOG(
			Warning,
			"[SFBuildCatalogPanelWidget] "
			"잘못된 건물 슬롯입니다. Slot=%d",
			SlotIndex
		);

		return false;
	}

	USFBuildCatalogEntryWidget* EntryWidget =
		CurrentEntryWidgets[SlotIndex].Get();

	if (!IsValid(EntryWidget))
	{
		return false;
	}

	EntryWidget->TriggerEntryClickedFromHotkey();

	return true;
}

UWidget* USFBuildCatalogPanelWidget::FindTutorialEntryWidget(FName BuildingRowName) const
{
	for (const USFBuildCatalogEntryWidget* Entry : CurrentEntryWidgets)
	{
		if (IsValid(Entry) && Entry->GetBuildingRowName() == BuildingRowName)
		{
			return const_cast<USFBuildCatalogEntryWidget*>(Entry);
		}
	}

	return nullptr;
}
