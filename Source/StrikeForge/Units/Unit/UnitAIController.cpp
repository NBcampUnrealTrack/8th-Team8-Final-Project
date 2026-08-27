#include "UnitAIController.h"

#include "Navigation/CrowdFollowingComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "Logging/SFLog.h"

/**
 * 이동 요청 로그 전역 스위치.
 *
 * 유닛별 bLogMoveRequests 를 켜려면 AIController 상속 BP 를 만들어야 해서
 * 급할 때 번거롭다. 콘솔에서 바로 켠다.
 *
 *   sf.LogMoveRequests 1
 */
static TAutoConsoleVariable<int32> CVarLogMoveRequests(
	TEXT("sf.LogMoveRequests"),
	0,
	TEXT("유닛 이동 요청 결과를 로그로 출력 (0=off, 1=on)"),
	ECVF_Cheat);

AUnitAIController::AUnitAIController(const FObjectInitializer& ObjectInitializer)
	// 기본 PathFollowingComponent 를 CrowdFollowing 으로 교체한다.
	// 이것이 있어야 유닛끼리 서로 밀고 지나가지 않고 자연스럽게 비켜간다.
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UCrowdFollowingComponent>(TEXT("PathFollowingComponent")))
	, bUseCrowdAvoidance(true)
	, bLogMoveRequests(false)
	, bHasLocationGoal(false)
{
	PrimaryActorTick.bCanEverTick = false;

	// 전투 유닛은 플레이어가 직접 조작하지 않는다
	bAttachToPawn = false;
	bWantsPlayerState = false;
}

void AUnitAIController::BeginPlay()
{
	Super::BeginPlay();

	UCrowdFollowingComponent* Crowd = Cast<UCrowdFollowingComponent>(GetPathFollowingComponent());
	if (!Crowd) return;

	if (bUseCrowdAvoidance)
	{
		Crowd->SetCrowdSimulationState(ECrowdSimulationState::Enabled);
		Crowd->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Medium);
		Crowd->SetCrowdSeparation(true);
		Crowd->SetCrowdSeparationWeight(CrowdSeparationWeight);
	}
	else
	{
		Crowd->SetCrowdSimulationState(ECrowdSimulationState::Disabled);
	}
}

void AUnitAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	// NavMesh 밖이거나 경로가 막혀서 이동이 실패/중단되었는지를 인수로 전달.
	OnCompleteMoving.Broadcast(!Result.IsFailure());
}

bool AUnitAIController::IsMoving() const
{
	return GetMoveStatus() == EPathFollowingStatus::Moving;
}

void AUnitAIController::RequestMoveToActor(AActor* InGoal, float InAcceptanceRadius)
{
	if (!IsValid(InGoal) || !GetPawn()) return;

	// 같은 목표를 향해 이미 이동 중이면 경로를 다시 계산하지 않는다.
	if (LastGoalActor.Get() == InGoal && !bHasLocationGoal && IsMoving())
	{
		return;
	}

	// 이동 중이 아니어도 매 프레임 재요청하면 안 된다.
	// DetourCrowd 는 MoveTo 마다 에이전트를 리셋하므로, 60fps 에서
	// 초당 60번 리셋되어 유닛이 조향을 시작하지 못하고 그 자리에 굳는다.
	const float Now = GetWorld()->GetTimeSeconds();
	if (LastGoalActor.Get() == InGoal && (Now - LastRequestTime) < MinRequestInterval)
	{
		return;
	}

	LastGoalActor = InGoal;
	bHasLocationGoal = false;
	LastRequestTime = Now;

	// 건물처럼 큰 목표는 자기 발밑의 NavMesh 를 통째로 덮는다.
	// 그러면 목표 지점을 NavMesh 로 투영할 수 없어 부분 경로조차 나오지 않고
	// MoveTo 가 Failed / PathValid 0 / Partial 0 을 돌려준다.
	//
	// 목표 액터를 그대로 넘기는 대신, 우리 쪽에서 목표를 향한 방향으로
	// 물러난 지점을 NavMesh 에 투영해 "도달 가능한 위치"를 목표로 삼는다.
	FAIMoveRequest Request;
	bool bUsedProjectedGoal = false;

	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		const FVector PawnLoc = GetPawn()->GetActorLocation();
		const FVector GoalLoc = InGoal->GetActorLocation();

		FNavLocation Projected;

		// 목표 자체가 NavMesh 위라면 그대로 쓴다 (유닛 등 작은 대상)
		if (NavSys->ProjectPointToNavigation(GoalLoc, Projected, FVector(200.f, 200.f, 500.f)))
		{
			Request.SetGoalLocation(Projected.Location);
			bUsedProjectedGoal = true;
		}
		else
		{
			// 목표가 NavMesh 밖이다. 목표를 바라보는 방향에서 조금씩 물러나며
			// NavMesh 위의 접근 지점을 찾는다.
			FVector ToPawn = PawnLoc - GoalLoc;
			ToPawn.Z = 0.f;

			if (!ToPawn.IsNearlyZero())
			{
				ToPawn.Normalize();

				// 건물 크기를 모르므로 바운딩 박스 반경에서 시작해 점점 넓힌다
				const FBox GoalBox = InGoal->GetComponentsBoundingBox(true);
				const float BaseRadius = GoalBox.IsValid
					? GoalBox.GetExtent().Size2D()
					: 300.f;

				for (int32 Step = 1; Step <= 5 && !bUsedProjectedGoal; ++Step)
				{
					const FVector Candidate = GoalLoc + ToPawn * (BaseRadius + Step * 150.f);

					if (NavSys->ProjectPointToNavigation(Candidate, Projected, FVector(300.f, 300.f, 500.f)))
					{
						Request.SetGoalLocation(Projected.Location);
						bUsedProjectedGoal = true;
					}
				}
			}
		}
	}

	// 투영에 실패하면 원래대로 액터를 목표로 넘긴다
	if (!bUsedProjectedGoal)
	{
		Request.SetGoalActor(InGoal);
	}

	Request.SetAcceptanceRadius(InAcceptanceRadius);
	Request.SetUsePathfinding(true);
	Request.SetAllowPartialPath(true);				// 목표가 NavMesh 밖이어도 최대한 접근

	// 에이전트 반지름을 더하지 않는다.
	// 더하면 도달 반경이 약 47cm 부풀어, 벽 반대편에서 도착 판정이 나버린다.
	Request.SetReachTestIncludesAgentRadius(false);

	// 목표 반지름은 유지한다. 큰 건물의 중심까지 파고들지 않게 하려면 필요하다.
	Request.SetReachTestIncludesGoalRadius(true);

	Request.SetCanStrafe(false);

	const FPathFollowingRequestResult Result = MoveTo(Request);

	// 실패해도 LastGoalActor 는 유지한다.
	// 비우면 MinRequestInterval 억제가 풀려 매 프레임 재요청이 나가고,
	// 경로가 원래 없는 상황에서는 로그가 초당 수십 줄씩 쏟아진다.
	// 시간 억제(0.5초)만으로 충분히 재시도된다.

	if (bLogMoveRequests || CVarLogMoveRequests.GetValueOnGameThread() > 0)
	{
		const UPathFollowingComponent* PFC = GetPathFollowingComponent();
		const bool bPartial = PFC && PFC->GetPath().IsValid() && PFC->GetPath()->IsPartial();

		SF_LOG(Warning, "[%s] MoveTo(%s%s) -> %s / PathValid %d / Partial %d",
			GetPawn() ? *GetPawn()->GetName() : TEXT("NoPawn"),
			*InGoal->GetName(),
			bUsedProjectedGoal ? TEXT(" [투영]") : TEXT(" [액터직접]"),
			Result.Code == EPathFollowingRequestResult::RequestSuccessful ? TEXT("Success")
				: Result.Code == EPathFollowingRequestResult::AlreadyAtGoal ? TEXT("AlreadyAtGoal")
				: TEXT("Failed"),
			PFC && PFC->HasValidPath() ? 1 : 0,
			bPartial ? 1 : 0);
	}
}

void AUnitAIController::RequestMoveToLocation(const FVector& InLocation, float InAcceptanceRadius, bool bInThrottle)
{
	if (!GetPawn()) return;

	const bool bSameSpot = bHasLocationGoal
		&& FVector::DistSquared(LastGoalLocation, InLocation) < FMath::Square(100.f);

	if (bSameSpot && IsMoving()) return;

	const float Now = GetWorld()->GetTimeSeconds();
	if (bSameSpot && (Now - LastRequestTime) < MinRequestInterval) return;

	// 목표가 조금씩 계속 움직이는 경로(공격 슬롯 추적)는 위치가 달라졌다는
	// 이유만으로 매번 재요청이 나간다. 그러면 DetourCrowd 에이전트가 그때마다
	// 리셋되어 유닛이 가속을 못 하고 제자리에서 비비게 된다.
	// 이 경로만 시간으로도 억제한다.
	//
	// ForceRepath / ClearMoveRequest 가 LastRequestTime 을 되감으므로
	// 플레이어 명령은 이 억제에 걸리지 않는다.
	if (bInThrottle && (Now - LastRequestTime) < SlotRequestInterval) return;

	LastGoalLocation = InLocation;
	LastGoalActor = nullptr;
	bHasLocationGoal = true;
	LastRequestTime = Now;

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSys || !NavSys->GetDefaultNavDataInstance()) return;
	
	FNavLocation OutLoc;
	FVector QueryExt(1000.f, 1000.f, 1000.f);
	FAIMoveRequest Request;
	if (NavSys->ProjectPointToNavigation(InLocation, OutLoc, QueryExt))
	{
		Request.SetGoalLocation(OutLoc);
	}
	else
	{
		Request.SetGoalLocation(InLocation);
	}
	Request.SetAcceptanceRadius(InAcceptanceRadius);
	Request.SetUsePathfinding(true);
	Request.SetAllowPartialPath(true);
	Request.SetReachTestIncludesAgentRadius(false);
	Request.SetCanStrafe(false);

	MoveTo(Request);
}

void AUnitAIController::ClearMoveRequest()
{
	if (IsMoving())
	{
		StopMovement();
	}

	LastGoalActor = nullptr;
	bHasLocationGoal = false;

	// 다음 요청은 즉시 나가야 한다 (끼임 복구 경로)
	LastRequestTime = -1000.f;
}

void AUnitAIController::ForceRepath()
{
	// StopMovement 를 호출하지 않는 것이 핵심이다.
	// 캐시만 비우면 다음 RequestMove 가 새 경로를 발행하고,
	// 그 사이에도 유닛은 기존 속도로 계속 나아간다.
	LastGoalActor = nullptr;
	bHasLocationGoal = false;
	LastRequestTime = -1000.f;
}
