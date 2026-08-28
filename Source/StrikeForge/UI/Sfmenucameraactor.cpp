#include "SFMenuCameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"

ASFMenuCameraActor::ASFMenuCameraActor()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ASFMenuCameraActor::BeginPlay()
{
	Super::BeginPlay();

	const FVector InitialLocation = GetActorLocation();

	if (LookTarget.IsZero())
	{
		LookTarget = InitialLocation;
	}

	// 시작 위치를 기준으로 반지름/기준 각도/높이를 고정해서 계산
	// 이후 이동은 이 반지름을 유지한 채 각도만 바뀌므로 거리(줌인/줌아웃)는 절대 변하지 않음
	const FVector ToInitial = InitialLocation - LookTarget;
	OrbitRadius = FVector(ToInitial.X, ToInitial.Y, 0.0f).Size();
	BaseYawDegrees = FMath::RadiansToDegrees(FMath::Atan2(ToInitial.Y, ToInitial.X));
	HeightOffset = ToInitial.Z;

	if (AngleWaypoints.Num() == 0)
	{
		AngleWaypoints.Add(0.0f);
	}

	CurrentWaypointIndex = 0;
	NextWaypointIndex = (AngleWaypoints.Num() > 1) ? 1 : 0;
	bIsMoving = false;
	PhaseTimer = 0.0f;

	// 이 카메라를 플레이어의 View Target으로 자동 지정
	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		PlayerController->SetViewTarget(this);
	}
}

void ASFMenuCameraActor::AdvanceWaypoint()
{
	if (AngleWaypoints.Num() <= 1)
	{
		return;
	}

	CurrentWaypointIndex = NextWaypointIndex;
	NextWaypointIndex = (NextWaypointIndex + 1) % AngleWaypoints.Num();
}

void ASFMenuCameraActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (AngleWaypoints.Num() <= 1)
	{
		return;
	}

	PhaseTimer += DeltaTime;

	float CurrentAngle = BaseYawDegrees + AngleWaypoints[CurrentWaypointIndex];

	if (bIsMoving)
	{
		const float Alpha = FMath::Clamp(PhaseTimer / TravelTime, 0.0f, 1.0f);
		const float SmoothAlpha = FMath::SmoothStep(0.0f, 1.0f, Alpha);

		const float StartAngle = BaseYawDegrees + AngleWaypoints[CurrentWaypointIndex];
		const float TargetAngle = BaseYawDegrees + AngleWaypoints[NextWaypointIndex];
		CurrentAngle = FMath::Lerp(StartAngle, TargetAngle, SmoothAlpha);

		if (Alpha >= 1.0f)
		{
			AdvanceWaypoint();
			bIsMoving = false;
			PhaseTimer = 0.0f;
		}
	}
	else
	{
		if (PhaseTimer >= DwellTime)
		{
			bIsMoving = true;
			PhaseTimer = 0.0f;
		}
	}

	// 반지름/높이는 고정한 채 각도만 바꿔서 이동 (거리 변화 없음 = 줌인/줌아웃 없음)
	const float AngleRad = FMath::DegreesToRadians(CurrentAngle);
	FVector NewLocation = LookTarget + FVector(FMath::Cos(AngleRad), FMath::Sin(AngleRad), 0.0f) * OrbitRadius;
	NewLocation.Z = LookTarget.Z + HeightOffset;

	SetActorLocation(NewLocation);

	// 항상 LookTarget(캐릭터)을 바라보게 회전
	const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(NewLocation, LookTarget);
	SetActorRotation(LookAtRotation);
}