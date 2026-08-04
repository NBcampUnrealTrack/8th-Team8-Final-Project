#include "SFCombatSubsystem.h"

#include "GameFramework/Actor.h"

USFCombatSubsystem* USFCombatSubsystem::Get(const UObject* WorldContext)
{
	if (!WorldContext) return nullptr;

	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return IsValid(World) ? World->GetSubsystem<USFCombatSubsystem>() : nullptr;
}

bool USFCombatSubsystem::IsHostileTeam(int32 InTeamA, int32 InTeamB)
{
	if (InTeamA == Targetable::NeutralTeam || InTeamB == Targetable::NeutralTeam)
	{
		return false;
	}
	return InTeamA != InTeamB;
}

void USFCombatSubsystem::RegisterCombatant(AActor* InActor)
{
	if (!IsValid(InActor) || !InActor->Implements<UTargetable>()) return;

	const int32 TeamIndex = ITargetable::Execute_GetTeamIndex(InActor);

	TArray<TWeakObjectPtr<AActor>>& Bucket = TeamBuckets.FindOrAdd(TeamIndex);
	Bucket.AddUnique(InActor);
}

void USFCombatSubsystem::UnregisterCombatant(AActor* InActor)
{
	if (!InActor) return;

	// 사망 시점에 팀이 바뀌었을 가능성까지 고려해 전 버킷을 훑는다.
	// 무효화된 약참조도 이 기회에 같이 정리한다.
	for (TPair<int32, TArray<TWeakObjectPtr<AActor>>>& Pair : TeamBuckets)
	{
		Pair.Value.RemoveAllSwap([InActor](const TWeakObjectPtr<AActor>& InWeak)
		{
			return !InWeak.IsValid() || InWeak.Get() == InActor;
		});
	}
}

bool USFCombatSubsystem::IsUsableTarget(const TWeakObjectPtr<AActor>& InWeak, AActor*& OutActor)
{
	AActor* Actor = InWeak.Get();
	if (!IsValid(Actor)) return false;

	// 죽었지만 아직 연출 중인 대상을 걸러낸다
	if (!ITargetable::Execute_IsTargetable(Actor)) return false;

	OutActor = Actor;
	return true;
}

AActor* USFCombatSubsystem::FindNearestHostile(const FVector& InOrigin, int32 InSeekerTeam, ETargetType InTargetType, float InMaxRange) const
{
	AActor* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();

	const bool bLimitRange = InMaxRange > 0.f;
	const float MaxRangeSq = InMaxRange * InMaxRange;

	for (const TPair<int32, TArray<TWeakObjectPtr<AActor>>>& Pair : TeamBuckets)
	{
		if (!IsHostileTeam(InSeekerTeam, Pair.Key)) continue;

		for (const TWeakObjectPtr<AActor>& Weak : Pair.Value)
		{
			AActor* Candidate = nullptr;
			if (!IsUsableTarget(Weak, Candidate)) continue;

			if (ITargetable::Execute_GetTargetType(Candidate) != InTargetType) continue;

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

void USFCombatSubsystem::GetHostilesInRadius(const FVector& InOrigin, int32 InSeekerTeam, float InRadius, TArray<AActor*>& OutActors) const
{
	OutActors.Reset();

	if (InRadius <= 0.f) return;

	const float RadiusSq = InRadius * InRadius;

	for (const TPair<int32, TArray<TWeakObjectPtr<AActor>>>& Pair : TeamBuckets)
	{
		if (!IsHostileTeam(InSeekerTeam, Pair.Key)) continue;

		for (const TWeakObjectPtr<AActor>& Weak : Pair.Value)
		{
			AActor* Candidate = nullptr;
			if (!IsUsableTarget(Weak, Candidate)) continue;

			if (FVector::DistSquared2D(InOrigin, Candidate->GetActorLocation()) <= RadiusSq)
			{
				OutActors.Add(Candidate);
			}
		}
	}
}

int32 USFCombatSubsystem::GetCombatantCount(int32 InTeamIndex) const
{
	const TArray<TWeakObjectPtr<AActor>>* Bucket = TeamBuckets.Find(InTeamIndex);
	if (!Bucket) return 0;

	int32 Count = 0;
	for (const TWeakObjectPtr<AActor>& Weak : *Bucket)
	{
		AActor* Actor = nullptr;
		if (IsUsableTarget(Weak, Actor))
		{
			++Count;
		}
	}
	return Count;
}
