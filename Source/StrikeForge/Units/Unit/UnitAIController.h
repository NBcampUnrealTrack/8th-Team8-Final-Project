#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "UnitAIController.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSFOnCompleteMoving, bool, IsSuccessed);

/**
 * 전투 유닛 전용 AIController.
 *
 * 역할은 "이동"만 담당한다. 타겟 선정과 공격 판정은 AUnitBase 에 있다.
 * 벽·구조물 회피는 NavMesh 가, 유닛끼리의 밀림은 DetourCrowd 가 처리한다.
 */
UCLASS()
class STRIKEFORGE_API AUnitAIController : public AAIController
{
	GENERATED_BODY()

public:
	AUnitAIController(const FObjectInitializer& ObjectInitializer);

	/**
	 * 액터 추적 이동. 목표가 같고 이미 이동 중이면 재요청하지 않는다.
	 * PathFollowingComponent 가 목표 액터를 추적하므로 매 프레임 부를 필요가 없다.
	 */
	void RequestMoveToActor(AActor* InGoal, float InAcceptanceRadius);

	/**
	 * 지점 이동.
	 *
	 * @param bInThrottle 목표 지점이 계속 조금씩 움직이는 경우(공격 슬롯 추적)에 켠다.
	 *                    위치가 바뀔 때마다 MoveTo 를 새로 내면 DetourCrowd 가
	 *                    요청마다 에이전트를 리셋해 유닛이 조향을 시작하지 못한다.
	 *                    플레이어 명령은 즉시 반영돼야 하므로 끈 채로 부른다.
	 */
	void RequestMoveToLocation(const FVector& InLocation, float InAcceptanceRadius, bool bInThrottle = false);

	/** 이동을 멈추고 경로를 버린다 */
	void ClearMoveRequest();

	/**
	 * 경로 캐시만 버린다. 이동은 멈추지 않는다.
	 * 다음 요청이 즉시 새 경로로 나가되, 그 사이 관성은 유지된다.
	 */
	void ForceRepath();

	/** 현재 이동 명령이 살아 있는가 */
	UFUNCTION(BlueprintCallable)
	bool IsMoving() const;

#pragma region Properties

	UPROPERTY(EditDefaultsOnly, Category = "Avoidance")
	uint8 bUseCrowdAvoidance : 1;

	UPROPERTY(EditDefaultsOnly, Category = "Avoidance", meta = (ClampMin = "0.0"))
	float CrowdSeparationWeight = 2.f;

	/**
	 * 같은 목표에 대한 경로 재요청 최소 간격(초).
	 *
	 * 0 으로 두면 매 프레임 MoveTo 가 나가고, DetourCrowd 는 요청마다
	 * 에이전트를 리셋하므로 유닛이 조향을 시작하지 못하고 그 자리에 굳는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ClampMin = "0.05"))
	float MinRequestInterval = 0.5f;

	/**
	 * 공격 슬롯 추적 전용 재요청 간격(초).
	 *
	 * 슬롯은 대상을 따라 계속 조금씩 움직이므로 재요청이 잦다.
	 * MinRequestInterval(0.5) 을 그대로 쓰면 낡은 지점을 오래 붙들어
	 * 접근이 눈에 띄게 굼떠진다. 사거리 근처에서만 쓰는 경로라
	 * 조금 짧게 잡아도 DetourCrowd 가 버틴다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ClampMin = "0.05"))
	float SlotRequestInterval = 0.2f;

	/** 켜면 MoveTo 결과와 경로 상태를 Output Log 에 찍는다 */
	UPROPERTY(EditDefaultsOnly, Category = "Debug")
	uint8 bLogMoveRequests : 1;

#pragma endregion

	FSFOnCompleteMoving OnCompleteMoving;	// 이동 완료 델리게이트

protected:
	virtual void BeginPlay() override;

	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;
private:
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> LastGoalActor;
	FVector LastGoalLocation = FVector::ZeroVector;
	uint8 bHasLocationGoal : 1;

	/** 마지막으로 MoveTo 를 호출한 시각 */
	float LastRequestTime = -1000.f;
};
