#pragma once

#include "CoreMinimal.h"
#include "BuildingGridInfo.generated.h"

USTRUCT(BlueprintType)
struct FBuildingGridInfo
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FIntPoint OriginCell = FIntPoint::ZeroValue;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FIntPoint GridSize = FIntPoint(1, 1);

	bool IsValid() const
	{
		return GridSize.X > 0 && GridSize.Y > 0;
	}
};