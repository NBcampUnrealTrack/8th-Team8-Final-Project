#include "SFBuildCatalogEntryWidget.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "Logging/SFLog.h"
#include "Datas/Building/SFBuildingDefinition.h"
#include "Datas/Building/BuildingTierData/SFBuildingTierData.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "InGame/HUD/Tutorial/SFTutorialSubsystem.h"

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

FResources
USFBuildCatalogEntryWidget::GetConstructionCost() const
{
	if (!IsValid(BuildingDefinition))
	{
		return FResources{};
	}

	const FSFBuildingTierData* FirstTier =
		BuildingDefinition->FindTier(0);

	if (!FirstTier)
	{
		return FResources{};
	}

	return FirstTier->Construction.ConstructCost;
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

	// 튜토리얼이 특정 건물을 고르라고 안내하는 중이면 다른 항목은 통과시키지 않는다.
	//
	// 브로드캐스트보다 앞에 둔다. 아래로 내리면 선택 표시 같은 UI 반응만
	// 일어나고 실제 건설은 안 되는, 눌린 것도 안 눌린 것도 아닌 상태가 된다.
	//
	// 튜토리얼이 없는 판이나 건물 선택을 요구하지 않는 단계에서는 항상 true 라
	// 일반 플레이에는 영향이 없다.
	if (!USFTutorialSubsystem::IsBuildingSelectionAllowed(this, BuildingRowName))
	{
		SF_LOG(
			Log,
			"[SFBuildCatalogEntryWidget] "
			"튜토리얼이 다른 건물을 요구하는 중이라 무시합니다: %s",
			*BuildingRowName.ToString()
		);

		return;
	}

	if (!bCanAfford)
	{
		SF_LOG(
			Warning,
			"[SFBuildCatalogEntryWidget] "
			"Cannot build because resources are insufficient: %s",
			*BuildingRowName.ToString()
		);

		OnBuildResourceWarningRequested.Broadcast();

		if (IsValid(ResourceWarningSound))
		{
			UGameplayStatics::PlaySound2D(
				this,
				ResourceWarningSound
			);
		}

		return;
	}

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

		// 배치 모드로 실제 들어간 뒤에 알린다.
		//
		// 자원이 모자라 생성이 실패하면 손에 든 건물이 없는데, 그때 알리면
		// 튜토리얼만 "배치하세요" 로 넘어가 버린다.
		USFTutorialSubsystem::NotifyBuildingSelected(this, BuildingRowName);
	}
}

void USFBuildCatalogEntryWidget::SetCanAfford(
	bool bNewCanAfford
)
{
	bCanAfford =
		bNewCanAfford;

	BP_OnAffordabilityChanged(
		bCanAfford
	);

	SF_LOG(
		Log,
		"[SFBuildCatalogEntryWidget] "
		"Affordability changed | Building: %s | CanAfford: %s",
		*BuildingRowName.ToString(),
		bCanAfford
		? TEXT("True")
		: TEXT("False")
	);
}