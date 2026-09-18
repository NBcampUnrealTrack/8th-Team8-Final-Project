#pragma once

#include "CoreMinimal.h"
#include "SFOverheadUISettings.generated.h"

/**
 * 바 하나의 배치 설정.
 *
 * 앵커(월드 위치)는 여기에 없다. 같은 액터의 모든 바가 메시 중심 하나를 공유하며,
 * 위아래 배치는 아래의 화면 오프셋으로만 준다. 그래야 시점이 바뀌어도 서로 정렬된다.
 */
USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFOverheadUISettings
{
	GENERATED_BODY()

	/**
	 * 앵커의 어느 쪽에 그릴지.
	 * (0.5, 1) = 앵커 위쪽,  (0.5, 0) = 앵커 아래쪽,  (0.5, 0.5) = 앵커 중앙.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overhead")
	FVector2D Pivot = FVector2D(0.5f, 1.f);

	/**
	 * 화면 세로 오프셋 = 메시 반높이 × 이 값. 픽셀.
	 * 음수면 위로, 양수면 아래로. 체력바는 -0.7, 진행도 바는 +0.7 정도로 시작한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overhead")
	float VerticalRatio = -0.7f;

	/** 계산 결과에 그대로 더하는 보정. 대부분 0으로 둔다. 픽셀. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overhead")
	FVector2D ScreenOffsetAdjust = FVector2D::ZeroVector;

	/** 바 가로 길이 = 메시 반폭 × 이 값. 픽셀. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overhead", meta = (ClampMin = "0.01"))
	float WidthRatio = 0.7f;

	/** 가로 길이의 하한(X)·상한(Y). 픽셀. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overhead")
	FVector2D WidthRange = FVector2D(150.f, 400.f);

	/** 바 세로 두께. 픽셀. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overhead", meta = (ClampMin = "1.0"))
	float BarHeight = 20.f;
};
