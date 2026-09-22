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
	
	bool Overlaps(const FBuildingGridInfo& Other) const
	{
		if (!IsValid() || !Other.IsValid())
		{
			return false;
		}

		/*
		 * Max는 실제 마지막 셀 다음 좌표입니다.
		 *
		 * Origin=(0,0), Size=(2,2)라면
		 * 사용하는 셀은 (0,0), (1,0), (0,1), (1,1)이고
		 * MaxExclusive는 (2,2)입니다.
		 */
		const FIntPoint ThisMaxExclusive =
			OriginCell + GridSize;

		const FIntPoint OtherMaxExclusive =
			Other.OriginCell + Other.GridSize;

		return
			OriginCell.X < OtherMaxExclusive.X &&
			ThisMaxExclusive.X > Other.OriginCell.X &&
			OriginCell.Y < OtherMaxExclusive.Y &&
			ThisMaxExclusive.Y > Other.OriginCell.Y;
	}
};