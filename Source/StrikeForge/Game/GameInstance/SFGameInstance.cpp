#include "SFGameInstance.h"

// 추가한 부분
#include "Datas/Building/SFBuildingCatalog.h"
#include "Kismet/GameplayStatics.h"
#include "Logging/SFLog.h"
#include "Game/Subsystem/SFDataTableManager.h"
#include "Datas/Stage/SFStageData.h"
#include "Game/Subsystem/SFBuildingAssetSubsystem.h"

UDataTable* USFGameInstance::GetDataTableType(EDataTableType TableType) const
{
	switch (TableType)
	{
	case EDataTableType::UI:
		{
			return UIDataTable;
		}
	case EDataTableType::Unit:
		{
			return UnitsDataTable;
		}
	case EDataTableType::Stage: // 추가부분
		{
			return StageDataTable;
		}
	case EDataTableType::ProductionUI:
	    {
	        return ProductionUIDataTable;
	    }
		default:
		{
			return nullptr;
		}
	}
}

// 추가된 부분
bool USFGameInstance::MoveToTitle()
{
    if (USFBuildingAssetSubsystem* AssetSubsystem =
    GetSubsystem<USFBuildingAssetSubsystem>())
    {
        AssetSubsystem->ReleaseStageAssets();
    }
    
	return TravelToLevel(TitleLevel);
}

bool USFGameInstance::MoveToLobby()
{
    if (USFBuildingAssetSubsystem* AssetSubsystem =
    GetSubsystem<USFBuildingAssetSubsystem>())
    {
        AssetSubsystem->ReleaseStageAssets();
    }
    
	return TravelToLevel(LobbyLevel);
}

bool USFGameInstance::SelectStage(FName StageRowName)
{
    SelectedStageRowName = NAME_None;

    if (StageRowName.IsNone())
    {
        SF_LOG(Warning, "[SFGameInstance] Stage Row Name is None.");

        return false;
    }

    USFDataTableManager* DataTableManager = GetSubsystem<USFDataTableManager>();

    if (!IsValid(DataTableManager))
    {
        SF_LOG(Error, "[SFGameInstance] DataTableManager is invalid.");

        return false;
    }

    const FSFStageData* StageData = DataTableManager->GetDataRow<FSFStageData>(EDataTableType::Stage, StageRowName, TEXT("SelectStage"));

    if (!StageData)
    {
        SF_LOG(Warning, "[SFGameInstance] Stage Row was not found: %s", *StageRowName.ToString());

        return false;
    }

    if (StageData->StageLevel.IsNull())
    {
        SF_LOG(Warning, "[SFGameInstance] Stage Level is not assigned: %s", *StageRowName.ToString());

        return false;
    }

    // 추가부분 26.08.10
    if (GetStageProgressState(StageRowName)
        == ESFStageProgressState::Locked)
    {
        SF_LOG(Warning, "[SFGameInstance] Locked stage cannot be selected: %s", *StageRowName.ToString());

        return false;
    }
    
    // stage테이블의 default플레이어 종족값 캐싱
    if (StageData->PlayerRace == ESFRace::None)
    {
        // elseif 때문에 7스테이지만 
        // 이전 스테이지의 종족값 제거
        SelectedRace = ESFRace::None;
    }
    else if (!SelectRace(StageData->PlayerRace))
    {
        SF_LOG(
            Error,
            "[SFGameInstance] 플레이어 종족 설정 실패. "
            "Stage=%s, Race=%s",
            *StageRowName.ToString(),
            *UEnum::GetValueAsString(
                StageData->PlayerRace));

        return false;
    }
    SelectedStageRowName = StageRowName;

    SF_LOG(Log, "[SFGameInstance] Stage selected: %s, DisplayName: %s", *SelectedStageRowName.ToString(), *StageData->DisplayName.ToString());

    return true;
}

FName USFGameInstance::GetSelectedStageRowName() const
{
    return SelectedStageRowName;
}

bool USFGameInstance::MoveToStage()
{
    if (SelectedStageRowName.IsNone())
    {
        SF_LOG(Warning, "[SFGameInstance] No stage has been selected.");

        return false;
    }

    USFDataTableManager* DataTableManager = GetSubsystem<USFDataTableManager>();

    if (!IsValid(DataTableManager))
    {
        SF_LOG(Error, "[SFGameInstance] DataTableManager is invalid.");

        return false;
    }

    const FSFStageData* StageData = DataTableManager->GetDataRow<FSFStageData>(EDataTableType::Stage, SelectedStageRowName, TEXT("MoveToStage"));

    if (!StageData)
    {
        SF_LOG(Warning, "[SFGameInstance] Selected Stage Row was not found: %s", *SelectedStageRowName.ToString());

        return false;
    }

    if (SelectedRace == ESFRace::None)
    {
        SF_LOG(
            Warning,
            "[SFGameInstance] 종족을 선택해야 스테이지로 이동할 수 있습니다.");

        return false;
    }
    
    return TravelToLevel(StageData->StageLevel);
}

// 추가부분 26.08.10
ESFStageProgressState USFGameInstance::GetStageProgressState(FName StageRowName) const
{
    if (StageRowName.IsNone())
    {
        return ESFStageProgressState::Locked;
    }

    const USFDataTableManager* DataTableManager =
        GetSubsystem<USFDataTableManager>();

    if (!IsValid(DataTableManager))
    {
        return ESFStageProgressState::Locked;
    }

    const FSFStageData* StageData =
        DataTableManager->GetDataRow<FSFStageData>(
            EDataTableType::Stage,
            StageRowName,
            TEXT("GetStageProgressState")
        );

    if (!StageData || StageData->StageOrder <= 0)
    {
        return ESFStageProgressState::Locked;
    }

    if (StageData->StageOrder <= HighestClearedStageOrder)
    {
        return ESFStageProgressState::Cleared;
    }

    if (StageData->StageOrder == HighestClearedStageOrder + 1)
    {
        return ESFStageProgressState::Unlocked;
    }

    return ESFStageProgressState::Locked;
}

// 추가 부분 26.08.10
bool USFGameInstance::MarkStageCleared(FName StageRowName)
{
    if (StageRowName.IsNone())
    {
        SF_LOG(Warning, "[SFGameInstance] Stage Row Name is None.");
        return false;
    }

    USFDataTableManager* DataTableManager =
        GetSubsystem<USFDataTableManager>();

    if (!IsValid(DataTableManager))
    {
        SF_LOG(Error, "[SFGameInstance] DataTableManager is invalid.");
        return false;
    }

    const FSFStageData* StageData =
        DataTableManager->GetDataRow<FSFStageData>(
            EDataTableType::Stage,
            StageRowName,
            TEXT("MarkStageCleared")
        );

    if (!StageData || StageData->StageOrder <= 0)
    {
        SF_LOG(
            Warning,
            "[SFGameInstance] Invalid stage data: %s",
            *StageRowName.ToString()
        );

        return false;
    }

    if (StageData->StageOrder > HighestClearedStageOrder + 1)
    {
        SF_LOG(
            Warning,
            "[SFGameInstance] Locked stage cannot be cleared: %s",
            *StageRowName.ToString()
        );

        return false;
    }

    HighestClearedStageOrder =
        FMath::Max(HighestClearedStageOrder, StageData->StageOrder);

    SF_LOG(
        Log,
        "[SFGameInstance] Stage cleared: %s, HighestClearedOrder: %d",
        *StageRowName.ToString(),
        HighestClearedStageOrder
    );

    return true;
}

bool USFGameInstance::TravelToLevel(const TSoftObjectPtr<UWorld>& TargetLevel)
{
	if (TargetLevel.IsNull())
	{
		SF_LOG(Error, "[SFGameInstance] Target level is not assigned.");

		return false;
	}

	if (!IsValid(GetWorld()))
	{
		SF_LOG(Error, "[SFGameInstance] World is invalid");

		return false;
	}

	const FString TargetLevelPath = TargetLevel.ToSoftObjectPath().ToString();

	SF_LOG(Log, "[SFGameInstance] Level travel requested : %s", *TargetLevelPath);

	UGameplayStatics::OpenLevelBySoftObjectPtr(this, TargetLevel);

	return true;
}

bool USFGameInstance::SelectRace(ESFRace InRace)
{   // 간단한 enum저장 함수
    const TSoftObjectPtr<USFBuildingCatalog>* Catalog =
        CatalogsByRace.Find(InRace);

    if (!Catalog || Catalog->IsNull())
    {
        SF_LOG(
            Error,
            "[SFGameInstance] 선택한 종족의 카탈로그가 없습니다. Race=%d",
            static_cast<int32>(InRace));

        return false;
    }

    SelectedRace = InRace;
    return true;
}

TSoftObjectPtr<USFBuildingCatalog> USFGameInstance::GetBuildingCatalogAsset(ESFRace InRace) const
{
    const TSoftObjectPtr<USFBuildingCatalog>* Found = CatalogsByRace.Find(InRace);
    return Found? *Found : TSoftObjectPtr<USFBuildingCatalog>();
}