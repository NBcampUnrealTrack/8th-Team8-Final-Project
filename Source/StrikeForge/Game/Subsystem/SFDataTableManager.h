#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "Game/GameInstance/SFGameInstance.h"
#include "Logging/SFLog.h"

#include "SFDataTableManager.generated.h"

UCLASS()
class STRIKEFORGE_API USFDataTableManager
    : public UGameInstanceSubsystem
{
    GENERATED_BODY()

    // 함수 영역
public:
    template<typename T>
    const T* GetDataRow(
        EDataTableType TableType,
        FName RowName,
        const FString& ContextString = TEXT("GetDataRow")
    ) const
    {
        if (RowName.IsNone())
        {
            SF_LOG(
                Warning,
                "[SFDataTableManager] RowName is None."
            );

            return nullptr;
        }

        const USFGameInstance* SFGameInstance =
            Cast<USFGameInstance>(
                GetGameInstance()
            );

        if (!IsValid(SFGameInstance))
        {
            SF_LOG(
                Error,
                "[SFDataTableManager] GameInstance is invalid."
            );

            return nullptr;
        }

        UDataTable* DataTable =
            SFGameInstance->GetDataTableType(
                TableType
            );

        if (!IsValid(DataTable))
        {
            SF_LOG(
                Error,
                "[SFDataTableManager] "
                "DataTable is invalid. TableType: %d",
                static_cast<int32>(TableType)
            );

            return nullptr;
        }

        const T* FoundRow =
            DataTable->FindRow<T>(
                RowName,
                ContextString,
                false
            );

        if (FoundRow == nullptr)
        {
            SF_LOG(
                Warning,
                "[SFDataTableManager] "
                "Row was not found. Table: %s, Row: %s",
                *GetNameSafe(DataTable),
                *RowName.ToString()
            );

            return nullptr;
        }

        return FoundRow;
    }
};