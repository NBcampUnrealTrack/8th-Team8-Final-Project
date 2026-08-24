#include "SFCombatSubsystem.h"

#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "GameFramework/Actor.h"
#include "Logging/SFLog.h"

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

	FSFTargetBucket& Bucket = TeamBuckets.FindOrAdd(InTarget->GetTeamIndex());
	Bucket.Targets.AddUnique(InTarget);
}

void USFCombatSubsystem::UnregisterTarget(UCombatTargetComponent* InTarget)
{
	if (!InTarget) return;

	// 팀이 바뀐 뒤 해제될 가능성까지 고려해 전 버킷을 훑는다.
	// 무효화된 약참조도 이 기회에 같이 정리한다.
	for (TPair<int32, FSFTargetBucket>& Pair : TeamBuckets)
	{
		Pair.Value.Targets.RemoveAllSwap([InTarget](const TWeakObjectPtr<UCombatTargetComponent>& InWeak)
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

	for (const TPair<int32, FSFTargetBucket>& Pair : TeamBuckets)
	{
		if (!IsHostileTeam(InSeekerTeam, Pair.Key)) continue;

		for (const TWeakObjectPtr<UCombatTargetComponent>& Weak : Pair.Value.Targets)
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

	for (const TPair<int32, FSFTargetBucket>& Pair : TeamBuckets)
	{
		if (!IsHostileTeam(InSeekerTeam, Pair.Key)) continue;

		for (const TWeakObjectPtr<UCombatTargetComponent>& Weak : Pair.Value.Targets)
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

	for (const TPair<int32, FSFTargetBucket>& Pair : TeamBuckets)
	{
		if (!IsHostileTeam(InSeekerTeam, Pair.Key)) continue;

		for (const TWeakObjectPtr<UCombatTargetComponent>& Weak : Pair.Value.Targets)
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

void USFCombatSubsystem::GetTeamTargets(int32 InTeamIndex, TArray<AActor*>& OutActors) const
{
	OutActors.Reset();

	const FSFTargetBucket* Bucket = TeamBuckets.Find(InTeamIndex);
	if (!Bucket) return;

	for (const TWeakObjectPtr<UCombatTargetComponent>& Weak : Bucket->Targets)
	{
		AActor* Actor = nullptr;

		if (ResolveTarget(Weak, Actor))
		{
			OutActors.Add(Actor);
		}
	}
}

int32 USFCombatSubsystem::GetTargetCount(int32 InTeamIndex) const
{
	const FSFTargetBucket* Bucket = TeamBuckets.Find(InTeamIndex);
	if (!Bucket) return 0;

	int32 Count = 0;
	for (const TWeakObjectPtr<UCombatTargetComponent>& Weak : Bucket->Targets)
	{
		AActor* Actor = nullptr;
		if (ResolveTarget(Weak, Actor))
		{
			++Count;
		}
	}
	return Count;
}

// ─────────────────────────────────────────────────────────
// 디버그 — 등록 현황 덤프
//
// 통합 레벨에서 "아군끼리 싸운다" 가 가장 흔한 사고다.
// 어느 액터가 어느 팀으로 등록됐는지 즉시 확인할 수 있어야 한다.
//
//   콘솔:  sf.DumpCombatTargets
// ─────────────────────────────────────────────────────────
void USFCombatSubsystem::DumpTargets() const
{
	SF_LOG(Warning, "===== 전투 대상 등록 현황 =====");

	if (TeamBuckets.Num() == 0)
	{
		SF_LOG(Warning, "  등록된 대상이 없습니다.");
		return;
	}

	// 팀 번호 순으로 보기 좋게 정렬한다
	TArray<int32> TeamKeys;
	TeamBuckets.GetKeys(TeamKeys);
	TeamKeys.Sort();

	for (const int32 TeamIndex : TeamKeys)
	{
		const FSFTargetBucket& Bucket = TeamBuckets[TeamIndex];

		int32 AliveCount = 0;
		for (const TWeakObjectPtr<UCombatTargetComponent>& Weak : Bucket.Targets)
		{
			AActor* Dummy = nullptr;
			if (ResolveTarget(Weak, Dummy)) ++AliveCount;
		}

		SF_LOG(Warning, "[Team %d] 생존 %d / 등록 %d", TeamIndex, AliveCount, Bucket.Targets.Num());

		for (const TWeakObjectPtr<UCombatTargetComponent>& Weak : Bucket.Targets)
		{
			const UCombatTargetComponent* Comp = Weak.Get();
			if (!IsValid(Comp))
			{
				SF_LOG(Warning, "    (무효화된 참조)");
				continue;
			}

			const AActor* Owner = Comp->GetOwner();

			SF_LOG(Warning, "    %-28s | %-18s | %s",
				Owner ? *Owner->GetName() : TEXT("NoOwner"),
				*UEnum::GetDisplayValueAsText(Comp->GetTargetType()).ToString(),
				Comp->IsAlive() ? TEXT("생존") : TEXT("사망"));
		}
	}
}

#if !UE_BUILD_SHIPPING
static void SFDumpCombatTargets(const TArray<FString>& Args, UWorld* World)
{
	const USFCombatSubsystem* Combat = IsValid(World) ? World->GetSubsystem<USFCombatSubsystem>() : nullptr;

	if (!IsValid(Combat))
	{
		SF_LOG(Error, "USFCombatSubsystem 을 찾지 못했습니다. 플레이 중인지 확인하세요.");
		return;
	}

	Combat->DumpTargets();
}

static FAutoConsoleCommandWithWorldAndArgs GSFDumpCombatTargets(
	TEXT("sf.DumpCombatTargets"),
	TEXT("팀별 전투 대상 등록 현황을 출력한다."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SFDumpCombatTargets));
#endif
