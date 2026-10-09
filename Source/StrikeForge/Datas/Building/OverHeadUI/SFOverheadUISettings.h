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
	 * 판이 OnElementFinished로 알리면 매니저가 StatusComponent의 비트를 내린다.
	 */
	ResourceGain,

	/** 건물 상태 아이콘(생산 대기·광산 고갈). 상태가 이어지는 동안 떠 있다. */
	StatusIcon,

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
	FVector2D Pivot = FVector2D(0.5f, 0.5f);

	/**
	 * 화면 세로 오프셋 = 메시 반높이 × 이 값. 픽셀.
	 *
	 * 앵커가 메시 중심이므로 음수를 줘야 건물 위로 올라간다. 메시가 클수록 더 멀리
	 * 띄워지므로 건물 크기가 달라도 같은 값으로 비슷한 자리에 놓인다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overhead")
	float VerticalRatio = 0.f;

	/**
	 * 건물 기준 앞뒤 오프셋 = 메시 앞뒤 반길이 × 이 값. 월드 거리.
	 *
	 * 1이면 정면 벽, -1이면 뒷벽, 0이면 지금처럼 건물 중앙이다.
	 * 정면은 액터 기준 +Y 방향이다. 건물이 90도 회전해 놓이면서 메시 정면이 +Y 쪽이 됐다.
	 * 정면이 -Y 쪽인 메시는 음수를 넣는다.
	 * VerticalRatio와 달리 화면이 아니라 건물 기준이라 건물 회전을 그대로 따라간다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overhead")
	float ForwardRatio = 0.f;

	/** 판 가로 길이 = 메시 반폭 × 이 값. 픽셀. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overhead", meta = (ClampMin = "0.01"))
	float WidthRatio = 0.7f;

	/** 가로 길이의 하한(X)·상한(Y). 픽셀. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overhead")
	FVector2D WidthRange = FVector2D(100.f, 300.f);

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
