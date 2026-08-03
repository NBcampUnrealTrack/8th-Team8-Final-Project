#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "SFDataTableManager.generated.h"

UCLASS()
class STRIKEFORGE_API USFDataTableManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	
	template<typename T>
	const T* GetDataRow(EDataTableType TableType, FName RowName, const FString& ContextString = TEXT("GetDataRow")) const
	{
		const USFGameInstance* SF_GI =	Cast<USFGameInstance>(GetGameInstance());

		if (!IsValid(SF_GI))
		{
			UE_LOG(LogTemp, Error, TEXT("GameInstance is invalid"));
			return nullptr;
		}

		UDataTable* DataTable = SF_GI->GetDataTableType(TableType);

		if (!IsValid(DataTable))
		{
			UE_LOG(LogTemp,	Error,	TEXT("DataTable is invalid. TableType: %d"), static_cast<int32>(TableType));
			return nullptr;
		}

		const T* FoundRow = DataTable->FindRow<T>(RowName, ContextString,true);

		if (!FoundRow)
		{
			UE_LOG(LogTemp,	Warning, TEXT("Row is invalid. Table: %s, Row: %s"), *GetNameSafe(DataTable), *RowName.ToString());
		}
		
		UE_LOG(LogTemp,	Warning, TEXT("Success."));
		return FoundRow;
	}
	
private:
	
};