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

private:
	// 카메라가 항상 바라보는 대상 위치 (보통 캐릭터 위치)
	UPROPERTY(EditAnywhere, Category = "SF|MenuCamera")
	FVector LookTarget = FVector::ZeroVector;

	// 캐릭터를 기준으로 도는 각도들(도 단위, 0 = 시작 위치). 이 각도들을 순서대로 부드럽게 오가며 보여줌
	// 거리/높이는 시작 위치 기준으로 항상 고정되므로 얼굴이 갑자기 확대되는 현상이 없음
	UPROPERTY(EditAnywhere, Category = "SF|MenuCamera")
	TArray<float> AngleWaypoints = { 0.0f, 35.0f, 0.0f, -35.0f };

	// 한 각도에서 다음 각도로 이동하는 데 걸리는 시간(초)
	UPROPERTY(EditAnywhere, Category = "SF|MenuCamera")
	float TravelTime = 5.0f;

	// 한 각도에 도착해서 머무는 시간(초)
	UPROPERTY(EditAnywhere, Category = "SF|MenuCamera")
	float DwellTime = 2.5f;

	// 시작 시점에 자동 계산되는 값들 (편집 불가)
	float OrbitRadius = 0.0f;
	float BaseYawDegrees = 0.0f;
	float HeightOffset = 0.0f;

	int32 CurrentWaypointIndex = 0;
	int32 NextWaypointIndex = 0;
	float PhaseTimer = 0.0f;
	bool bIsMoving = false;
};