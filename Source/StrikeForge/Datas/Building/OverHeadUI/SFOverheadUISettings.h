#pragma once

#include "CoreMinimal.h"
#include "SFOverheadUISettings.generated.h"

/**
 * 머리 위 판에 올라가는 요소.
 *
 * 요소를 늘리려면 여기에 하나 추가하고 판 위젯에 자식을 붙이면 된다.
 * 컴포넌트를 더 달 필요가 없다.
 */
UENUM(BlueprintType)
enum class ESFOverheadElement : uint8
{
	/** 체력 바. 보통 마우스를 올렸을 때만 보인다. */
	Health,

	/** 생산·채굴 진행도 바. 일하는 동안만 보인다. */
	Progress,

	/**
	 * 자원 획득 배지(+8). 잠깐 떴다 스스로 사라진다.
	 *
	 * 다른 요소와 달리 끄는 주체가 소유자가 아니라 애니메이션이다.
	 * 판이 OnElementFinished로 알리면 컴포넌트가 비트를 내린다.
	 */
	ResourceGain,

	MAX UMETA(Hidden)
};

/**
 * 머리 위 판 하나의 배치 설정. 액터당 하나다.
 *
 * 요소끼리의 세로 배치는 여기서 정하지 않는다. 그건 판 위젯의 UMG 레이아웃이 정한다.
 * 여기는 판 전체를 메시 기준 어디에 얼마만 한 크기로 놓을지만 다룬다.
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
	 * 음수면 위로, 양수면 아래로.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overhead")
	float VerticalRatio = -0.7f;

	/** 계산 결과에 그대로 더하는 보정. 대부분 0으로 둔다. 픽셀. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overhead")
	FVector2D ScreenOffsetAdjust = FVector2D::ZeroVector;

	/** 판 가로 길이 = 메시 반폭 × 이 값. 픽셀. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overhead", meta = (ClampMin = "0.01"))
	float WidthRatio = 0.7f;

	/** 가로 길이의 하한(X)·상한(Y). 픽셀. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overhead")
	FVector2D WidthRange = FVector2D(150.f, 400.f);

	/**
	 * 판 세로 크기. 픽셀.
	 *
	 * 요소가 여럿 들어가므로 바 하나 두께가 아니라 판 전체 높이다.
	 * 요소가 꺼져도 자리는 남으므로(Hidden) 판 높이는 늘 이 값이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overhead", meta = (ClampMin = "1.0"))
	float PanelHeight = 44.f;

	/**
	 * 판 안에서 요소끼리 벌리는 간격. 픽셀.
	 *
	 * 판 위젯의 ElementSpacer 크기가 된다. 스페이서를 어디에 둘지는 UMG가 정하고,
	 * 얼마나 벌릴지만 여기서 건물마다 정한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overhead", meta = (ClampMin = "0.0"))
	float ElementSpacing = 4.f;
};
