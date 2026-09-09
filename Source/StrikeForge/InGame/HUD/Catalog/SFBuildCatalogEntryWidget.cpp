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

FName USFBuildCatalogEntryWidget::GetSelectionUIRowName() const
{
    if (
        IsValid(BuildingDefinition) &&
        !BuildingDefinition->SelectionUIRowName.IsNone()
        )
    {
        return BuildingDefinition->SelectionUIRowName;
    }

    // 공통 UI 이름이 없으면 실제 BuildingId를 사용한다.
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

    // 1. 스테이지 건설 제한 검사
    if (!bStageBuildAllowed)
    {
        SF_LOG(
            Warning,
            "[SFBuildCatalogEntryWidget] "
            "Building is locked by current stage: %s",
            *BuildingRowName.ToString()
        );

        if (IsValid(ResourceWarningSound))
        {
            UGameplayStatics::PlaySound2D(
                this,
                ResourceWarningSound
            );
        }

        return;
    }

    // 2. 튜토리얼 건물 선택 제한
    if (!USFTutorialSubsystem::IsBuildingSelectionAllowed(
        this,
        BuildingRowName
    ))
    {
        SF_LOG(
            Log,
            "[SFBuildCatalogEntryWidget] "
            "튜토리얼이 다른 건물을 요구하는 중이라 무시합니다: %s",
            *BuildingRowName.ToString()
        );

        return;
    }

    // 3. 자원 부족 검사
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

    // 4. 클릭 이벤트 전달
    OnBuildCatalogEntryClicked.Broadcast(
        BuildingRowName
    );

    ASFInGamePlayerController* SFPC =
        Cast<ASFInGamePlayerController>(
            GetOwningPlayer()
        );

    if (!IsValid(SFPC))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "USFBuildCatalogEntryWidget::"
                "HandleEntryClicked: "
                "SFInGamePlayerController is invalid."
            )
        );

        return;
    }

    // 5. 건설 함수는 한 번만 호출
    if (SFPC->SpawnBuildingFunction(BuildingRowName))
    {
        SFPC->SetSFControlMode(
            ESFControlMode::BuildPlacement
        );

        USFTutorialSubsystem::NotifyBuildingSelected(
            this,
            BuildingRowName
        );
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

void USFBuildCatalogEntryWidget::SetStageBuildAllowed(
	bool bAllowed
)
{
	bStageBuildAllowed =
		bAllowed;

	BP_OnStageBuildLockChanged(
		!bStageBuildAllowed
	);
}