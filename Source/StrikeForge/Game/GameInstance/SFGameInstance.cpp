#include "SFGameInstance.h"

// 추가한 부분
#include "Kismet/GameplayStatics.h"
#include "Logging/SFLog.h"

UDataTable* USFGameInstance::GetDataTableType(EDataTableType TableType) const
{
	switch (TableType)
	{
	case EDataTableType::UI:
		{
			return UIDataTable;
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

bool USFGameInstance::MoveToStage()
{
	return TravelToLevel(StageLevel);
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
		SF_LOG(Error, "[SFGameInstance] World is inalid");

		return false;
	}

	const FString TargetLevelPath = TargetLevel.ToSoftObjectPath().ToString();

	SF_LOG(Log, "[SFGameInstance] Level travel requested : %s", *TargetLevelPath);

	UGameplayStatics::OpenLevelBySoftObjectPtr(this, TargetLevel);

	return true;
}
