#include "LanePath.h"

#include "Components/SplineComponent.h"

ALanePath::ALanePath()
{
	PrimaryActorTick.bCanEverTick = false;

	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	SetRootComponent(Spline);
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
