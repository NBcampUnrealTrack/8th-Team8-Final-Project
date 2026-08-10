#include "SFGameInstance.h"

// 추가한 부분
#include "Kismet/GameplayStatics.h"
#include "Logging/SFLog.h"
#include "Game/Subsystem/SFDataTableManager.h"
#include "Datas/Stage/SFStageData.h"

UDataTable* USFGameInstance::GetDataTableType(EDataTableType TableType) const
{
	switch (TableType)
	{
	case EDataTableType::UI:
		{
			return UIDataTable;
		}
    case EDataTableType::BuildCatalogUI:
        {
            return BuildCatalogUIDataTable;
        }
	case EDataTableType::Building:
		{
			return BuildingDataTable;
		}
	case EDataTableType::Unit:
		{
			return UnitsDataTable;
		}
	case EDataTableType::Stage: // 추가부분
		{
			return StageDataTable;
		}
	case EDataTableType::Product:
		{
			return ProductDataTable;
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
	return TravelToLevel(TitleLevel);
}

bool USFGameInstance::MoveToLobby()
{
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

    return TravelToLevel(StageData->StageLevel);
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
