#include "SFMenuCameraActor.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"

ASFMenuCameraActor::ASFMenuCameraActor()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCameraComponent()->bConstrainAspectRatio = false;
}

void ASFMenuCameraActor::BeginPlay()
{
    Super::BeginPlay();

    const FVector InitialLocation = GetActorLocation();

    // Old maps without a target keep their placed view instead of aiming at themselves.
    bHasLookTarget = !LookTarget.IsZero();
    GetCameraComponent()->bConstrainAspectRatio = false;
    GetCameraComponent()->bOverrideAspectRatioAxisConstraint = true;
    GetCameraComponent()->AspectRatioAxisConstraint = EAspectRatioAxisConstraint::AspectRatio_MaintainYFOV;

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
    if (bHasLookTarget) ApplyOrbit(BaseYawDegrees);

    if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
    {
        if(PlayerController->IsLocalController())PlayerController->SetViewTarget(this);
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
    if (!bHasLookTarget) return;
    if (bContinuousDrift)
    {
        DriftTime += DeltaTime;
        const float Phase = DriftTime * 2.f * PI / FMath::Max(DriftPeriod, 8.f);
        ApplyOrbit(BaseYawDegrees + DriftAngle * FMath::Sin(Phase), DriftHeight * FMath::Sin(Phase * 2.f));
        return;
    }

    if (AngleWaypoints.Num() <= 1)
    {
        return;
    }

    PhaseTimer += DeltaTime;

    float CurrentAngle = BaseYawDegrees + AngleWaypoints[CurrentWaypointIndex];

    if (bIsMoving)
    {
        const float Alpha = FMath::Clamp(PhaseTimer / FMath::Max(TravelTime, .01f), 0.0f, 1.0f);
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

    ApplyOrbit(CurrentAngle);
}

void ASFMenuCameraActor::ApplyOrbit(float Angle, float Lift)
{
    const float AngleRad = FMath::DegreesToRadians(Angle);
    FVector NewLocation = LookTarget + FVector(FMath::Cos(AngleRad), FMath::Sin(AngleRad), 0.0f) * OrbitRadius;
    NewLocation.Z = LookTarget.Z + HeightOffset + Lift;

    SetActorLocation(NewLocation);

    const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(NewLocation, LookTarget);
    SetActorRotation(LookAtRotation);
}
