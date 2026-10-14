#pragma once
#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "SFMenuCameraActor.generated.h"

UCLASS()
class STRIKEFORGE_API ASFMenuCameraActor : public ACameraActor
{
    GENERATED_BODY()

public:
    ASFMenuCameraActor();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    void AdvanceWaypoint();
    void ApplyOrbit(float Angle, float Lift = 0.f);

private:
    UPROPERTY(EditAnywhere, Category = "SF|MenuCamera")
    FVector LookTarget = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, Category = "SF|MenuCamera")
    TArray<float> AngleWaypoints = { 0.0f, 35.0f, 0.0f, -35.0f };

    UPROPERTY(EditAnywhere, Category = "SF|MenuCamera")
    float TravelTime = 5.0f;

    UPROPERTY(EditAnywhere, Category = "SF|MenuCamera")
    float DwellTime = 2.5f;

    // Continuous, gentle orbit for the Start scene; other menu cameras retain their waypoints.
    UPROPERTY(EditAnywhere, Category = "SF|MenuCamera")
    bool bContinuousDrift = false;

    UPROPERTY(EditAnywhere, Category = "SF|MenuCamera", meta = (ClampMin = "0", ClampMax = "15"))
    float DriftAngle = 5.f;

    UPROPERTY(EditAnywhere, Category = "SF|MenuCamera", meta = (ClampMin = "8"))
    float DriftPeriod = 32.f;

    UPROPERTY(EditAnywhere, Category = "SF|MenuCamera", meta = (ClampMin = "0"))
    float DriftHeight = 14.f;

    bool bHasLookTarget = false;
    float DriftTime = 0.f;
    float OrbitRadius = 0.0f;
    float BaseYawDegrees = 0.0f;
    float HeightOffset = 0.0f;

    int32 CurrentWaypointIndex = 0;
    int32 NextWaypointIndex = 0;
    float PhaseTimer = 0.0f;
    bool bIsMoving = false;
};