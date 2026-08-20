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
	FVector GetCurrentTargetLocation() const;
	void AdvanceWaypoint();

private:
	// 카메라가 계속 바라보는 대상 위치 (보통 캐릭터 위치)
	UPROPERTY(EditAnywhere, Category = "SF|MenuCamera")
	FVector LookTarget = FVector::ZeroVector;

	// 시작 위치를 기준으로 한 상대 오프셋들을 순서대로 이동하며 보여줌
	// 예: X만 다르게 주면 좌우로 이동하며 캐릭터 측면/정면을 번갈아 보여줌
	UPROPERTY(EditAnywhere, Category = "SF|MenuCamera")
	TArray<FVector> WaypointOffsets = { FVector(0.f, 0.f, 0.f), FVector(200.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f), FVector(-200.f, 0.f, 0.f) };

	// 한 지점에서 다음 지점으로 이동하는 데 걸리는 시간(초)
	UPROPERTY(EditAnywhere, Category = "SF|MenuCamera")
	float TravelTime = 4.0f;

	// 한 지점에 도착해서 머무는 시간(초)
	UPROPERTY(EditAnywhere, Category = "SF|MenuCamera")
	float DwellTime = 2.5f;

	FVector InitialLocation;
	int32 CurrentWaypointIndex = 0;
	int32 NextWaypointIndex = 0;
	float PhaseTimer = 0.0f;
	bool bIsMoving = false;
};