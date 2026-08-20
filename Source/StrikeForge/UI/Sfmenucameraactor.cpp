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

	InitialLocation = GetActorLocation();

	if (LookTarget.IsZero())
	{
		LookTarget = InitialLocation;
	}

	if (WaypointOffsets.Num() == 0)
	{
		WaypointOffsets.Add(FVector::ZeroVector);
	}

	CurrentWaypointIndex = 0;
	NextWaypointIndex = (WaypointOffsets.Num() > 1) ? 1 : 0;
	bIsMoving = false;
	PhaseTimer = 0.0f;

	SetActorLocation(InitialLocation + WaypointOffsets[CurrentWaypointIndex]);

	// 이 카메라를 플레이어의 View Target으로 자동 지정
	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		PlayerController->SetViewTarget(this);
	}
}

FVector ASFMenuCameraActor::GetCurrentTargetLocation() const
{
	return InitialLocation + WaypointOffsets[CurrentWaypointIndex];
}

void ASFMenuCameraActor::AdvanceWaypoint()
{
	if (WaypointOffsets.Num() <= 1)
	{
		return;
	}

	CurrentWaypointIndex = NextWaypointIndex;
	NextWaypointIndex = (NextWaypointIndex + 1) % WaypointOffsets.Num();
}

void ASFMenuCameraActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (WaypointOffsets.Num() <= 1)
	{
		return;
	}

	PhaseTimer += DeltaTime;

	if (bIsMoving)
	{
		const float Alpha = FMath::Clamp(PhaseTimer / TravelTime, 0.0f, 1.0f);
		const float SmoothAlpha = FMath::SmoothStep(0.0f, 1.0f, Alpha);

		const FVector StartLocation = InitialLocation + WaypointOffsets[CurrentWaypointIndex];
		const FVector TargetLocation = InitialLocation + WaypointOffsets[NextWaypointIndex];
		const FVector NewLocation = FMath::Lerp(StartLocation, TargetLocation, SmoothAlpha);

		SetActorLocation(NewLocation);

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

	// 항상 LookTarget(캐릭터)을 바라보게 회전
	const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), LookTarget);
	SetActorRotation(LookAtRotation);
}