#include "SFGameInstance.h"

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
		default:
		{
			return nullptr;
		}
	}
}
