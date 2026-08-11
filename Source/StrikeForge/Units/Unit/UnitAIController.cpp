#include "UnitAIController.h"

#include "Navigation/CrowdFollowingComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "Logging/SFLog.h"

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

	FAIMoveRequest Request(InGoal);
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

	// 이미 도달했다고 응답하면 이동이 시작되지 않는다.
	// 목표를 비워 다음 틱에 새 경로로 다시 시도하게 한다.
	if (Result.Code != EPathFollowingRequestResult::RequestSuccessful)
	{
		LastGoalActor = nullptr;
	}

	if (bLogMoveRequests)
	{
		const UPathFollowingComponent* PFC = GetPathFollowingComponent();
		const bool bPartial = PFC && PFC->GetPath().IsValid() && PFC->GetPath()->IsPartial();

		SF_LOG(Warning, "[%s] MoveTo(%s) -> %s / PathValid %d / Partial %d",
			GetPawn() ? *GetPawn()->GetName() : TEXT("NoPawn"),
			*InGoal->GetName(),
			Result.Code == EPathFollowingRequestResult::RequestSuccessful ? TEXT("Success")
				: Result.Code == EPathFollowingRequestResult::AlreadyAtGoal ? TEXT("AlreadyAtGoal")
				: TEXT("Failed"),
			PFC && PFC->HasValidPath() ? 1 : 0,
			bPartial ? 1 : 0);
	}
}

void AUnitAIController::RequestMoveToLocation(const FVector& InLocation, float InAcceptanceRadius)
{
	if (!GetPawn()) return;

	const bool bSameSpot = bHasLocationGoal
		&& FVector::DistSquared(LastGoalLocation, InLocation) < FMath::Square(100.f);

	if (bSameSpot && IsMoving()) return;

	const float Now = GetWorld()->GetTimeSeconds();
	if (bSameSpot && (Now - LastRequestTime) < MinRequestInterval) return;

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
