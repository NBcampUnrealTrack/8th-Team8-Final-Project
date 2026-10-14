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

	/** 점유하는 마지막 셀의 다음 좌표를 반환하여 반열림 사각형 계산에 사용합니다. */
	FIntPoint GetMaxExclusive() const
	{
		return OriginCell + GridSize;
	}

	/** 짝수 크기의 셀 사이 중심도 정수로 보존할 수 있도록 중심 좌표를 2배 값으로 반환합니다. */
	FIntPoint GetCenterTwice() const
	{
		return FIntPoint(
			OriginCell.X * 2 + GridSize.X,
			OriginCell.Y * 2 + GridSize.Y);
	}

	/** 다른 점유 영역의 기하 중심이 이 영역 안에 포함되는지 반 셀 정밀도로 검사합니다. */
	bool ContainsCenterOf(const FBuildingGridInfo& Other) const
	{
		if (!IsValid() || !Other.IsValid())
		{
			return false;
		}

		const FIntPoint CenterTwice = Other.GetCenterTwice();
		const FIntPoint MinTwice(OriginCell.X * 2, OriginCell.Y * 2);
		const FIntPoint MaxExclusive = GetMaxExclusive();
		const FIntPoint MaxTwice(MaxExclusive.X * 2, MaxExclusive.Y * 2);

		return CenterTwice.X >= MinTwice.X && CenterTwice.X < MaxTwice.X &&
			CenterTwice.Y >= MinTwice.Y && CenterTwice.Y < MaxTwice.Y;
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
		const FIntPoint ThisMaxExclusive = GetMaxExclusive();
		const FIntPoint OtherMaxExclusive = Other.GetMaxExclusive();

		return
			OriginCell.X < OtherMaxExclusive.X &&
			ThisMaxExclusive.X > Other.OriginCell.X &&
			OriginCell.Y < OtherMaxExclusive.Y &&
			ThisMaxExclusive.Y > Other.OriginCell.Y;
	}
};
