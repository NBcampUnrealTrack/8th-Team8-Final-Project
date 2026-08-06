#include "SFCombatSubsystem.h"

#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "GameFramework/Actor.h"

USFCombatSubsystem* USFCombatSubsystem::Get(const UObject* WorldContext)
{
	if (!WorldContext) return nullptr;

	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return IsValid(World) ? World->GetSubsystem<USFCombatSubsystem>() : nullptr;
}

UCombatTargetComponent* USFCombatSubsystem::FindTargetComponent(const AActor* InActor)
{
	if (!IsValid(InActor)) return nullptr;

	return InActor->FindComponentByClass<UCombatTargetComponent>();
}

bool USFCombatSubsystem::IsHostileTeam(int32 InTeamA, int32 InTeamB)
{
	if (InTeamA == Targetable::NeutralTeam || InTeamB == Targetable::NeutralTeam)
	{
		return false;
	}
	return InTeamA != InTeamB;
}

void USFCombatSubsystem::RegisterTarget(UCombatTargetComponent* InTarget)
{
	if (!IsValid(InTarget) || !IsValid(InTarget->GetOwner())) return;

	TArray<TWeakObjectPtr<UCombatTargetComponent>>& Bucket = TeamBuckets.FindOrAdd(InTarget->GetTeamIndex());
	Bucket.AddUnique(InTarget);
}

void USFCombatSubsystem::UnregisterTarget(UCombatTargetComponent* InTarget)
{
	if (!InTarget) return;

	// 팀이 바뀐 뒤 해제될 가능성까지 고려해 전 버킷을 훑는다.
	// 무효화된 약참조도 이 기회에 같이 정리한다.
	for (TPair<int32, TArray<TWeakObjectPtr<UCombatTargetComponent>>>& Pair : TeamBuckets)
	{
		Pair.Value.RemoveAllSwap([InTarget](const TWeakObjectPtr<UCombatTargetComponent>& InWeak)
		{
			return !InWeak.IsValid() || InWeak.Get() == InTarget;
		});
	}
}

bool USFCombatSubsystem::ResolveTarget(const TWeakObjectPtr<UCombatTargetComponent>& InWeak, AActor*& OutActor)
{
	UCombatTargetComponent* Comp = InWeak.Get();
	if (!IsValid(Comp) || !Comp->IsAlive()) return false;

	AActor* Owner = Comp->GetOwner();
	if (!IsValid(Owner)) return false;

	OutActor = Owner;
	return true;
}

AActor* USFCombatSubsystem::FindNearestHostile(const FVector& InOrigin, int32 InSeekerTeam, ETargetType InTargetType, float InMaxRange) const
{
	AActor* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();

	const bool bLimitRange = InMaxRange > 0.f;
	const float MaxRangeSq = InMaxRange * InMaxRange;

	for (const TPair<int32, TArray<TWeakObjectPtr<UCombatTargetComponent>>>& Pair : TeamBuckets)
	{
		if (!IsHostileTeam(InSeekerTeam, Pair.Key)) continue;

		for (const TWeakObjectPtr<UCombatTargetComponent>& Weak : Pair.Value)
		{
			AActor* Candidate = nullptr;
			if (!ResolveTarget(Weak, Candidate)) continue;

			if (Weak->GetTargetType() != InTargetType) continue;

			// 수평 거리로 판정한다. 언덕 높이 차로 사거리가 어긋나지 않게.
			const float DistSq = FVector::DistSquared2D(InOrigin, Candidate->GetActorLocation());

			if (bLimitRange && DistSq > MaxRangeSq) continue;

			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Best = Candidate;
			}
		}
	}

	return Best;
}

AActor* USFCombatSubsystem::FindNearestHostileAny(const FVector& InOrigin, int32 InSeekerTeam) const
{
	AActor* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();

	for (const TPair<int32, TArray<TWeakObjectPtr<UCombatTargetComponent>>>& Pair : TeamBuckets)
	{
		if (!IsHostileTeam(InSeekerTeam, Pair.Key)) continue;

		for (const TWeakObjectPtr<UCombatTargetComponent>& Weak : Pair.Value)
		{
			AActor* Candidate = nullptr;
			if (!ResolveTarget(Weak, Candidate)) continue;

			const float DistSq = FVector::DistSquared2D(InOrigin, Candidate->GetActorLocation());

			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Best = Candidate;
			}
		}
	}

	return Best;
}

void USFCombatSubsystem::GetHostilesInRadius(const FVector& InOrigin, int32 InSeekerTeam, float InRadius, TArray<AActor*>& OutActors) const
{
	OutActors.Reset();

	if (InRadius <= 0.f) return;

	const float RadiusSq = InRadius * InRadius;

	for (const TPair<int32, TArray<TWeakObjectPtr<UCombatTargetComponent>>>& Pair : TeamBuckets)
	{
		if (!IsHostileTeam(InSeekerTeam, Pair.Key)) continue;

		for (const TWeakObjectPtr<UCombatTargetComponent>& Weak : Pair.Value)
		{
			AActor* Candidate = nullptr;
			if (!ResolveTarget(Weak, Candidate)) continue;

			if (FVector::DistSquared2D(InOrigin, Candidate->GetActorLocation()) <= RadiusSq)
			{
				OutActors.Add(Candidate);
			}
		}
	}
}

int32 USFCombatSubsystem::GetTargetCount(int32 InTeamIndex) const
{
	const TArray<TWeakObjectPtr<UCombatTargetComponent>>* Bucket = TeamBuckets.Find(InTeamIndex);
	if (!Bucket) return 0;

	int32 Count = 0;
	for (const TWeakObjectPtr<UCombatTargetComponent>& Weak : *Bucket)
	{
		AActor* Actor = nullptr;
		if (ResolveTarget(Weak, Actor))
		{
			++Count;
		}
	}
	return Count;
}
