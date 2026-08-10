#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "SFBuildCatalogUIData.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFBuildCatalogUIDataRow
	: public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "SF|BuildCatalog"
	)
	FText DisplayName;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "SF|BuildCatalog"
	)
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "SF|BuildCatalog"
	)
	int32 MineralCost = 0;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "SF|BuildCatalog"
	)
	int32 EtherCost = 0;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "SF|BuildCatalog"
	)
	int32 RequiredTier = 1;
};