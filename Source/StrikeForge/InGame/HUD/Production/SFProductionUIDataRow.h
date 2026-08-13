#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "SFProductionUIDataRow.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFProductionUIDataRow
    : public FTableRowBase
{
    GENERATED_BODY()

public:
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Production|UI"
    )
    FText DisplayName;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Production|UI"
    )
    TSoftObjectPtr<UTexture2D> Icon;
};