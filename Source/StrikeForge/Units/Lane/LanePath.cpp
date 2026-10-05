#include "LanePath.h"

#include "Game/Subsystem/SFCombatSubsystem.h"

#include "Components/SplineComponent.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

/**
 * 코리도 시각화.
 *
 * LaneHalfWidth 는 눈으로 확인하지 않으면 맞출 수가 없는 값이다. 숫자만 보고는
 * 흙길 가장자리에 걸치는지 잔디까지 먹는지 알 수 없고, 그 차이가 곧 "유닛이
 * 길 위에 머무는가" 를 가른다. PIE 에서 켜놓고 값을 조정한다.
 *
 *   sf.DrawLaneDebug 1   코리도 경계와 중심선을 그린다
 */
static TAutoConsoleVariable<int32> CVarDrawLaneDebug(
	TEXT("sf.DrawLaneDebug"),
	0,
	TEXT("진격로 코리도(중심선과 폭)를 그린다 (0=off, 1=on)."),
	ECVF_Cheat);

ALanePath::ALanePath()
{
	// 평소에는 아무 일도 하지 않는다. sf.DrawLaneDebug 를 PIE 중에 켤 수 있어야
	// 하므로 틱 자체는 열어두고, Tick 첫 줄에서 즉시 빠져나간다.
	PrimaryActorTick.bCanEverTick = true;

	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	SetRootComponent(Spline);
}

void ALanePath::BeginPlay()
{
	Super::BeginPlay();

	if (USFCombatSubsystem* Combat = USFCombatSubsystem::Get(this))
	{
		Combat->RegisterLane(this);
	}
}

void ALanePath::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USFCombatSubsystem* Combat = USFCombatSubsystem::Get(this))
	{
		Combat->UnregisterLane(this);
	}

	Super::EndPlay(EndPlayReason);
}

FVector ALanePath::GetPointAtAlpha(float InAlpha) const
{
	if (!Spline) return GetActorLocation();

	const float Clamped = FMath::Clamp(InAlpha, 0.f, 1.f);
	const float Distance = Spline->GetSplineLength() * Clamped;

	return Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
}

float ALanePath::GetAlphaAtLocation(const FVector& InWorldLocation) const
{
	if (!Spline) return 0.f;

	const float Length = Spline->GetSplineLength();
	if (Length <= KINDA_SMALL_NUMBER) return 0.f;

	const float Key = Spline->FindInputKeyClosestToWorldLocation(InWorldLocation);
	const float Distance = Spline->GetDistanceAlongSplineAtSplineInputKey(Key);

	return FMath::Clamp(Distance / Length, 0.f, 1.f);
}

float ALanePath::GetSplineLength() const
{
	return Spline ? Spline->GetSplineLength() : 0.f;
}

bool ALanePath::IsUsableByTeam(int32 InTeamIndex) const
{
	// 팀 번호를 비교하지만 진영 분기가 아니다 — "이 라인이 이 팀의 것인가" 만
	// 묻고, 어느 쪽이 아군인지는 알지도 못한다. -1 이면 모두가 쓴다.
	return TeamIndex == INDEX_NONE || TeamIndex == InTeamIndex;
}

// ─────────────────────────────────────────────────────────
// 코리도 판정
//
// 스플라인 위 최근접 점을 찾은 뒤 "수평" 거리만 잰다.
//
// FindInputKeyClosestToWorldLocation 자체는 3D 거리로 키를 찾으므로, 경사가
// 심한 구간에서는 최적점에서 살짝 어긋난 키가 나온다. 그래도 그 점까지의
// 거리를 XY 로 재면 오차는 코리도 폭에 비해 무시할 수준이고, 판정이 높이차에
// 오염되지 않는다는 이점이 훨씬 크다 — 캡슐 중심은 지면보다 90cm 위에 있어서,
// 3D 로 재면 길 한복판에 선 유닛도 90cm 벗어난 것으로 잡힌다.
// ─────────────────────────────────────────────────────────
float ALanePath::GetDistanceOutsideLane(const FVector& InWorldLocation) const
{
	// 스플라인이 없으면 판정할 근거가 없다. "안" 으로 답해 제한을 걸지 않는다.
	if (!Spline) return 0.f;

	const float Key = Spline->FindInputKeyClosestToWorldLocation(InWorldLocation);
	const FVector OnSpline = Spline->GetLocationAtSplineInputKey(Key, ESplineCoordinateSpace::World);

	const float Horizontal = FVector::Dist2D(InWorldLocation, OnSpline);

	return FMath::Max(0.f, Horizontal - LaneHalfWidth);
}

bool ALanePath::IsInsideLane(const FVector& InWorldLocation) const
{
	return GetDistanceOutsideLane(InWorldLocation) <= 0.f;
}

FVector ALanePath::GetClosestLanePoint(const FVector& InWorldLocation) const
{
	if (!Spline) return InWorldLocation;

	const float Key = Spline->FindInputKeyClosestToWorldLocation(InWorldLocation);

	return Spline->GetLocationAtSplineInputKey(Key, ESplineCoordinateSpace::World);
}

// ─────────────────────────────────────────────────────────
// 디버그 — 코리도 시각화
// ─────────────────────────────────────────────────────────
void ALanePath::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

#if ENABLE_DRAW_DEBUG
	if (GetWorld() && GetWorld()->IsGameWorld()) return;
	if (CVarDrawLaneDebug.GetValueOnGameThread() == 0) return;
	if (!Spline) return;

	const UWorld* World = GetWorld();
	if (!IsValid(World)) return;

	const float Length = Spline->GetSplineLength();
	if (Length <= KINDA_SMALL_NUMBER) return;

	// 200cm 마다 한 마디씩 끊어 그린다. 더 촘촘히 그려도 눈으로는 차이가 없고
	// 라인이 여럿이면 드로우 콜만 늘어난다.
	const float Step = 200.f;
	const int32 Segments = FMath::Max(1, FMath::CeilToInt(Length / Step));

	FVector PrevLeft = FVector::ZeroVector;
	FVector PrevRight = FVector::ZeroVector;

	for (int32 Index = 0; Index <= Segments; ++Index)
	{
		const float Distance = FMath::Min(Length, Index * Step);

		const FVector Center = Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
		FVector Right = Spline->GetRightVectorAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);

		// 수평 성분만 쓴다. 코리도 판정 자체가 XY 기준이므로 그림도 같아야 한다.
		Right.Z = 0.f;
		Right = Right.GetSafeNormal();

		const FVector Left = Center - Right * LaneHalfWidth;
		const FVector RightEdge = Center + Right * LaneHalfWidth;

		if (Index > 0)
		{
			DrawDebugLine(World, PrevLeft, Left, FColor::Green, false, -1.f, 0, 6.f);
			DrawDebugLine(World, PrevRight, RightEdge, FColor::Green, false, -1.f, 0, 6.f);
			DrawDebugLine(World, Center, Spline->GetLocationAtDistanceAlongSpline(
				FMath::Max(0.f, Distance - Step), ESplineCoordinateSpace::World),
				FColor::Yellow, false, -1.f, 0, 3.f);
		}

		PrevLeft = Left;
		PrevRight = RightEdge;
	}
#endif
}
