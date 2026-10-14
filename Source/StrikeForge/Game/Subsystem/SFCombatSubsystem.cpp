#include "SFCombatSubsystem.h"
#include "Units/Unit/SFNeutralSlime.h"

// GFrameCounter — 격자를 프레임당 한 번만 다시 채우기 위해 쓴다
#include "CoreGlobals.h"

#include "ActorComponent/Combat/CombatTargetComponent.h"

// 팀 조회의 단일 창구를 만들기 위해 읽기 전용으로 참조한다.
// 두 파일 모두 수정하지 않는다.
#include "Buildings/SFRootBase.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "Units/Lane/LanePath.h"
#include "GameFramework/Actor.h"
#include "Logging/SFLog.h"

/**
 * 포위 성향 조절.
 *
 * 슬롯을 고를 때 두 가지가 경쟁한다.
 *   - 이미 찬 자리에서 최대한 멀리 (감싸는 모양)
 *   - 내가 지금 선 방향에서 최대한 가까이 (덜 걷는다)
 *
 * 이 값이 작을수록 감싸는 쪽을 택하고, 클수록 가까운 자리를 택한다.
 * 0 이면 이동 거리를 아예 무시하고 무조건 반대편으로 돈다.
 *
 *   sf.SlotEncircleWeight 0.2   더 크게 감싼다
 *   sf.SlotEncircleWeight 1.5   가까운 자리를 우선한다
 */
/**
 * 대상 하나에 붙을 수 있는 공격자 수 상한. 0 이면 슬롯 개수(=기하학적 한계)만 본다.
 *
 * 예전 기본값은 5였다. NavMesh/DetourCrowd 로 붙던 시절에는 링에 자리가
 * 남아 있어도 뒤에서 오는 유닛이 앞줄 캡슐을 통과하지 못해 실제로 도달하는
 * 것이 4기 남짓이었고, 시스템이 현실보다 자리가 많다고 믿으면 갈 수 없는
 * 곳으로 향하는 유닛이 계속 생겼기 때문이다.
 *
 * 그 전제가 지금은 사라졌다.
 *   - 유닛끼리 기본적으로 통과한다(ApplyUnitCollisionMode 의 ECR_Ignore)
 *   - 근접 구간은 NavMesh 가 아니라 SteerTowardSlot 의 직접 조향이 몬다
 *     (분리력 회전 바이어스 + 정체 탈출로 앞줄에 막혀도 감아 돌아간다)
 *
 * 그래서 상한을 끈다. 링 반지름이 정하는 칸 수가 곧 정원이다 — 사거리 270
 * 기준으로 유닛 대상에 15칸쯤 나오고, 이웃 슬롯 간격은 94cm 로 캡슐이 거의
 * 맞닿는다. 스타크래프트처럼 들어갈 수 있는 만큼 들어간다.
 *
 *   sf.MaxAttackersPerTarget 0   상한 없음 (기본. 슬롯 개수만 본다)
 *   sf.MaxAttackersPerTarget 5   예전 동작으로 되돌리기
 */
static TAutoConsoleVariable<int32> CVarMaxAttackersPerTarget(
	TEXT("sf.MaxAttackersPerTarget"),
	0,
	TEXT("대상 하나에 붙을 수 있는 공격자 수 상한. 0 이하면 슬롯 개수만 본다."),
	ECVF_Cheat);

/**
 * 슬롯 사이 최소 간격(cm). 한 대상에 몇 기가 붙을 수 있는지를 정하는 값이다.
 *
 * 유닛 지름(캡슐 반지름 45 x 2 = 90)이 기본이다. 이보다 촘촘하게 잡으면
 * 이웃한 두 슬롯의 점유자가 물리적으로 겹치기 시작한다 — 유닛끼리 통과하도록
 * 둔 상태(sf.UnitsBlockEachOther 0)에서는 그래도 그림이 깨지지 않으므로,
 * 더 빽빽하게 붙이고 싶으면 낮춰도 된다.
 *
 * 이미 만들어진 링에는 적용되지 않는다. 칸 수가 바뀌면 각도(2πk/칸수)가 전부
 * 달라져서, 붙어 싸우던 유닛이 한꺼번에 링 위를 돌게 되기 때문이다.
 * 다음에 새로 만들어지는 링부터 반영된다.
 *
 *   sf.SlotSpacing 90   기본 (캡슐 지름)
 *   sf.SlotSpacing 70   더 빽빽하게 — 한 대상에 더 많이 붙는다
 */
static TAutoConsoleVariable<float> CVarSlotSpacing(
	TEXT("sf.SlotSpacing"),
	90.f,
	TEXT("공격 슬롯 사이 최소 간격(cm). 낮출수록 한 대상에 많이 붙는다. 새 링부터 적용."),
	ECVF_Cheat);

static TAutoConsoleVariable<float> CVarSlotEncircleWeight(
	TEXT("sf.SlotEncircleWeight"),
	0.45f,
	TEXT("공격 슬롯 배정 시 이동 거리에 주는 가중치. 낮을수록 크게 감싼다."),
	ECVF_Cheat);

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

int32 USFCombatSubsystem::GetTeamOf(const AActor* InActor)
{
	if (!IsValid(InActor)) return Targetable::NeutralTeam;

	// 전투 컴포넌트가 팀의 실제 저장소다. 건물의 ASFRootBase::GetTeamIndex 도
	// 내부에서 이 컴포넌트를 읽으므로, 여기 하나만 보면 유닛·건물이 모두 맞는다.
	if (const UCombatTargetComponent* Combat = FindTargetComponent(InActor))
	{
		return Combat->GetTeamIndex();
	}

	// 컴포넌트가 없는 액터(스포너, 커맨더 등)를 위한 경로.
	if (const ASFRootBase* RootBase = Cast<ASFRootBase>(InActor))
	{
		return RootBase->GetTeamIndex();
	}

	// 0(플레이어 팀)이 아니라 중립을 돌려준다.
	// 0 을 돌려주면 팀 주입을 빠뜨린 액터가 조용히 아군이 되어,
	// 아군끼리 안 싸우는 것으로만 나타나 원인 추적이 어렵다.
	// 중립은 아무와도 싸우지 않으므로 "아무것도 안 한다" 로 눈에 띈다.
	SF_LOG(Warning,
		"[Combat] %s : 팀 번호를 찾지 못해 중립(-1)으로 봅니다. "
		"CombatTarget 컴포넌트가 붙어 있는지 확인하세요.",
		*InActor->GetName());

	return Targetable::NeutralTeam;
}

int32 USFCombatSubsystem::GetLocalPlayerTeam() const
{
	const UWorld* World = GetWorld();
	if (!IsValid(World)) return Targetable::NeutralTeam;

	const ASFInGamePlayerController* PC = World->GetFirstPlayerController<ASFInGamePlayerController>();
	if (!IsValid(PC)) return Targetable::NeutralTeam;

	return PC->GetMyTeamIndex();
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
	GridBuiltFrame = TNumericLimits<uint64>::Max();
}

void USFCombatSubsystem::UnregisterTarget(UCombatTargetComponent* InTarget)
{
	if (!InTarget) return;
	GridBuiltFrame = TNumericLimits<uint64>::Max();

	// 팀이 바뀐 뒤 해제될 가능성까지 고려해 전 버킷을 훑는다.
	// 무효화된 약참조도 이 기회에 같이 정리한다.
	for (TPair<int32, FSFTargetBucket>& Pair : TeamBuckets)
	{
		Pair.Value.Targets.RemoveAllSwap([InTarget](const TWeakObjectPtr<UCombatTargetComponent>& InWeak)
		{
			return !InWeak.IsValid() || InWeak.Get() == InTarget;
		});
	}

	// 이 대상을 둘러싸고 있던 공격 슬롯 링도 함께 버린다.
	// 남겨두면 죽은 대상의 링에 자리를 잡은 유닛이 계속 그쪽으로 걸어간다.
	if (AActor* Owner = InTarget->GetOwner())
	{
		SlotRings.Remove(TWeakObjectPtr<AActor>(Owner));
	}

	// 사망은 자주 일어나는 이벤트다. 무효화된 링 키를 여기서 같이 걷어낸다.
	for (TMap<TWeakObjectPtr<AActor>, FSFAttackSlotRing>::TIterator It(SlotRings); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

// ─────────────────────────────────────────────────────────
// 진격로 코리도
//
// 라인은 레벨에 놓인 정적 액터다. 개수가 한 자릿수라 선형 탐색으로 충분하다.
//
// 판정의 기본값은 전부 "제한 없음" 쪽이다. 라인을 아직 배치하지 않은 레벨에서
// 유닛이 전부 굳거나 건물을 어디에도 못 짓는 사태가 나면, 원인이 코리도라는
// 것을 알아채기까지 한참 걸린다. 없으면 없는 대로 예전처럼 동작해야 한다.
// ─────────────────────────────────────────────────────────
void USFCombatSubsystem::RegisterLane(ALanePath* InLane)
{
	if (!IsValid(InLane)) return;

	Lanes.AddUnique(TWeakObjectPtr<ALanePath>(InLane));

	// 라인이 생겼으니 "없다" 경고를 다시 낼 수 있게 되돌린다.
	bNoLaneReported = false;

	SF_LOG(Log, "[Lane] %s 등록 (팀 %d, 반폭 %.0f)",
		*InLane->GetName(), InLane->GetTeamIndex(), InLane->GetLaneHalfWidth());
}

void USFCombatSubsystem::UnregisterLane(ALanePath* InLane)
{
	if (!InLane) return;

	// 무효화된 약참조도 이 기회에 같이 걷어낸다.
	Lanes.RemoveAllSwap([InLane](const TWeakObjectPtr<ALanePath>& InWeak)
	{
		return !InWeak.IsValid() || InWeak.Get() == InLane;
	});
}

bool USFCombatSubsystem::HasAnyLane() const
{
	for (const TWeakObjectPtr<ALanePath>& Weak : Lanes)
	{
		if (Weak.IsValid()) return true;
	}

	// 조용히 false 를 돌려주면 "유닛이 안 움직인다" 나 "건물이 안 지어진다" 의
	// 원인을 추적할 수가 없다. 한 번은 반드시 알린다.
	if (!bNoLaneReported)
	{
		bNoLaneReported = true;

		SF_LOG(Warning,
			"[Lane] 배치된 ALanePath 가 없어 코리도 제한(길 밖 추격 금지 / 건물 배치 제한)이 "
			"꺼진 상태로 동작합니다. 흙길 위에 LanePath 를 배치하고 LaneHalfWidth 를 맞추세요.");
	}

	return false;
}

float USFCombatSubsystem::GetDistanceOutsideLane(const FVector& InLocation) const
{
	float Best = TNumericLimits<float>::Max();

	for (const TWeakObjectPtr<ALanePath>& Weak : Lanes)
	{
		const ALanePath* Lane = Weak.Get();
		if (!IsValid(Lane)) continue;

		// 갈래길에서는 어느 한 라인 안이기만 하면 안으로 친다.
		// 0 이 나온 시점에 더 볼 이유가 없다.
		const float Outside = Lane->GetDistanceOutsideLane(InLocation);

		if (Outside <= 0.f) return 0.f;

		Best = FMath::Min(Best, Outside);
	}

	// 라인이 하나도 없다 = 제한 없음 = 어디에 있든 "안"
	return (Best == TNumericLimits<float>::Max()) ? 0.f : Best;
}

bool USFCombatSubsystem::IsInsideAnyLane(const FVector& InLocation) const
{
	return GetDistanceOutsideLane(InLocation) <= 0.f;
}

ALanePath* USFCombatSubsystem::FindNearestLane(const FVector& InLocation, int32 InTeamIndex) const
{
	ALanePath* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();

	for (const TWeakObjectPtr<ALanePath>& Weak : Lanes)
	{
		ALanePath* Lane = Weak.Get();
		if (!IsValid(Lane)) continue;

		if (!Lane->IsUsableByTeam(InTeamIndex)) continue;

		// 코리도 중심선까지의 수평 거리로 고른다. 이미 어느 라인 안에 서 있으면
		// 그 라인이 0 으로 나와 자연스럽게 선택된다.
		const float Dist = FVector::Dist2D(InLocation, Lane->GetClosestLanePoint(InLocation));

		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = Lane;
		}
	}

	return Best;
}

// ─────────────────────────────────────────────────────────
// 공격 슬롯
//
// 링은 각도만 배분한다.
//   슬롯 개수  = floor(2π(대상반경 + SlotRingGap) / SlotSpacing)
//   k번 각도   = 2πk / 슬롯개수   (월드 기준 고정)
//
// 각도를 대상 회전에 맞추면 적이 몸을 돌릴 때마다 아군 전체가 같이 돌게 된다.
// 그래서 월드 기준으로 고정한다.
// ─────────────────────────────────────────────────────────
/**
 * 이 점유자가 아직 자리를 지키고 있는가.
 *
 * IsValid 만으로는 부족하다. Destroy() 가 예약된 액터는 DeathLifeSpan 동안
 * 유효한 채로 남아 자리를 계속 차지한다.
 *
 * 전투 컴포넌트까지 뒤져 생존을 확인하지는 않는다 — 링 순회는 자주 도는데
 * FindComponentByClass 는 액터의 컴포넌트 배열을 선형으로 훑기 때문이다.
 * 정상 사망 경로는 AUnitBase::HandleDeath 가 직접 반납하므로,
 * 여기서는 그 경로를 놓친 경우만 걷어내면 된다.
 */
static bool IsOccupantHolding(const TWeakObjectPtr<AActor>& InOccupant)
{
	const AActor* Actor = InOccupant.Get();
	return IsValid(Actor) && !Actor->IsActorBeingDestroyed();
}

void USFCombatSubsystem::MeasureTargetBounds(const AActor* InTarget, float& OutRadius, FVector2D& OutExtent)
{
	OutRadius = 45.f;						// 캡슐 기본 반지름
	OutExtent = FVector2D(45.f, 45.f);

	if (!IsValid(InTarget)) return;

	// false = 콜리전 없는 컴포넌트 제외.
	// StatusWidget 같은 위젯이 박스를 부풀려 링이 필요 이상으로 커지는 것을 막는다.
	const FBox Box = InTarget->GetComponentsBoundingBox(false);
	if (!Box.IsValid) return;

	const FVector Extent = Box.GetExtent();

	OutExtent = FVector2D(Extent.X, Extent.Y);
	OutRadius = FMath::Max(Extent.X, Extent.Y);
}

int32 USFCombatSubsystem::ComputeSlotCount(float InRingRadius) const
{
	// 반지름 R 인 원 위에 간격 S 로 놓을 수 있는 점의 개수는
	//   N = floor( π / asin(S / 2R) )
	// 둘레를 간격으로 나누는 근사(2πR/S)는 값을 조금 크게 잡아서
	// 이웃한 슬롯이 미세하게 겹친다. 정확한 식을 쓴다.
	const float Spacing = CVarSlotSpacing.GetValueOnGameThread();

	const float Ratio = FMath::Max(1.f, Spacing) / (2.f * FMath::Max(1.f, InRingRadius));

	// 링이 간격보다 작으면 원 위에 두 점도 못 놓는다
	if (Ratio >= 1.f) return MinSlotsPerTarget;

	const int32 Count = FMath::FloorToInt(PI / FMath::Asin(Ratio));

	return FMath::Clamp(Count, MinSlotsPerTarget, MaxSlotsPerTarget);
}

const FSFAttackSlotRing* USFCombatSubsystem::FindRing(const AActor* InTarget) const
{
	if (!InTarget) return nullptr;

	// 조회에만 쓰는 const_cast 다. 대상을 수정하지 않는다.
	return SlotRings.Find(TWeakObjectPtr<AActor>(const_cast<AActor*>(InTarget)));
}

FSFAttackSlotRing* USFCombatSubsystem::FindOrCreateRing(AActor* InTarget, float InApproachDistance)
{
	if (!IsValid(InTarget)) return nullptr;

	const TWeakObjectPtr<AActor> Key(InTarget);

	if (FSFAttackSlotRing* Existing = SlotRings.Find(Key))
	{
		// 사라졌거나 파괴 예약된 점유자를 비운다.
		//
		// 예전에는 !IsValid() 일 때만 Reset() 했는데, 무효 약참조를 무효로
		// 만드는 것이라 아무 효과가 없었다. 실제로 자리를 붙잡고 있는 것은
		// "아직 유효하지만 Destroy() 가 예약된" 시체 쪽이다.
		for (TWeakObjectPtr<AActor>& Occupant : Existing->Occupants)
		{
			if (!IsOccupantHolding(Occupant))
			{
				Occupant.Reset();
			}
		}
		return Existing;
	}

	FSFAttackSlotRing NewRing;
	MeasureTargetBounds(InTarget, NewRing.TargetRadius, NewRing.BoxExtent);

	// 칸 수는 "실제로 서게 될 원" 의 크기로 정한다.
	//
	// 대상 크기만으로 세면 근접 사거리에서 링이 실제보다 크게 잡혀,
	// 들어갈 수 없는 자리까지 배정된다. 그 자리를 받은 유닛은 이웃에 밀려
	// 끝내 도착하지 못하고 공격도 못 한다.
	NewRing.Occupants.SetNum(ComputeSlotCount(NewRing.TargetRadius + FMath::Max(0.f, InApproachDistance)));

	// 세대를 붙인다. 이 링이 나중에 버려졌다 다시 만들어지면 값이 달라지고,
	// 옛 자리를 들고 있던 공격자들이 그것을 보고 스스로 다시 잡는다.
	NewRing.Generation = NextRingGeneration++;

	// 랩어라운드 방어. 0 은 "링 없음" 을 뜻하므로 건너뛴다.
	if (NextRingGeneration == 0) NextRingGeneration = 1;

	return &SlotRings.Add(Key, MoveTemp(NewRing));
}

int32 USFCombatSubsystem::ClaimAttackSlot(AActor* InTarget, AActor* InAttacker, float InApproachDistance)
{
	if (!IsValid(InTarget) || !IsValid(InAttacker)) return INDEX_NONE;

	++StatSlotClaims;

	FSFAttackSlotRing* Ring = FindOrCreateRing(InTarget, InApproachDistance);
	if (!Ring) return INDEX_NONE;

	const int32 SlotCount = Ring->Occupants.Num();
	if (SlotCount <= 0) return INDEX_NONE;

	// 이미 이 대상에 자리를 갖고 있으면 그대로 쓴다.
	// 정원 검사보다 먼저 봐야 한다. 나중에 보면 정원을 채운 장본인이
	// 자기 자리를 빼앗기고 매 틱 다시 잡으려 든다.
	int32 Occupied = 0;

	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		if (!IsOccupantHolding(Ring->Occupants[Index]))
		{
			// 이 자리는 비었다. 훑는 김에 정리해 둔다.
			Ring->Occupants[Index].Reset();
			continue;
		}

		++Occupied;

		if (Ring->Occupants[Index].Get() == InAttacker)
		{
			return Index;
		}
	}

	// 정원을 넘겼으면 자리를 주지 않는다.
	//
	// 빈 칸이 남아 있어도 마찬가지다. 남은 칸은 대개 대상 반대편이라
	// 뒤에서 온 유닛이 앞줄을 통과해 갈 수가 없다. 갈 수 없는 자리를 쥐여주면
	// 그 유닛은 계속 헤매기만 하고 아무것도 못 한다.
	//
	// 아래 O(N²) 배치 계산을 건너뛰는 조기 탈출이기도 하다.
	const int32 Limit = CVarMaxAttackersPerTarget.GetValueOnGameThread();

	if (Limit > 0 && Occupied >= Limit)
	{
		return INDEX_NONE;
	}

	// 지금 서 있는 방향을 각도로 바꾼다.
	FVector ToAttacker = InAttacker->GetActorLocation() - InTarget->GetActorLocation();
	ToAttacker.Z = 0.f;

	const float Angle = ToAttacker.IsNearlyZero()
		? 0.f
		: FMath::Atan2(ToAttacker.Y, ToAttacker.X);

	const int32 Preferred = ((FMath::RoundToInt((Angle / (2.f * PI)) * SlotCount)) % SlotCount + SlotCount) % SlotCount;

	// 첫 공격자는 그냥 자기가 선 방향을 쓴다. 비교할 상대가 없다.
	if (!Ring->Occupants[Preferred].IsValid() && GetAttackerCount(InTarget) == 0)
	{
		Ring->Occupants[Preferred] = InAttacker;
		return Preferred;
	}

	// 빈 슬롯마다 점수를 매겨 가장 좋은 자리를 고른다.
	//
	//   점수 = 가장 가까운 점유 슬롯까지의 칸수          (얼마나 벌어지는가)
	//        + 이 슬롯이 낀 빈 구간의 폭 × GapWeight     (가장 넓은 틈을 메우는가)
	//        − 내가 선 방향까지의 칸수 × TravelWeight    (얼마나 걷는가)
	//
	// 앞 두 항이 "감싸는 모양", 마지막 항이 "덜 걷기" 다.
	//
	// 가장 가까운 빈 칸을 그냥 주면 같은 방향에서 온 유닛이 전부 옆자리에 붙어
	// 한쪽 면에만 늘어선다. 그렇다고 거리를 무시하면 코앞의 적을 두고
	// 반대편까지 크게 돌아가느라 그동안 아무것도 못 한다.
	//
	// 두 번째 항이 없으면 "가장 가까운 점유 슬롯까지의 칸수" 가 쉽게 동점이 되어
	// 결국 이동 거리로만 갈리고, 세 기가 붙었을 때 반원에만 늘어선다.
	// 빈 구간의 폭을 함께 보면 넓게 벌어진 쪽을 메우러 간다.
	//
	// 성향은 sf.SlotEncircleWeight 로 게임 중에 바꿔가며 맞춘다.
	const float TravelWeight = FMath::Max(0.f, CVarSlotEncircleWeight.GetValueOnGameThread());
	const float GapWeight = 0.25f;

	int32 BestIndex = INDEX_NONE;
	float BestScore = -TNumericLimits<float>::Max();

	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		if (Ring->Occupants[Index].IsValid()) continue;

		// 양쪽으로 훑어 가장 가까운 점유 슬롯까지의 칸수를 잰다.
		// 둘을 더하면 이 슬롯이 낀 빈 구간의 폭이 된다.
		int32 Clockwise = SlotCount;
		int32 CounterClockwise = SlotCount;

		for (int32 Step = 1; Step <= SlotCount; ++Step)
		{
			if (Clockwise == SlotCount
				&& Ring->Occupants[(Index + Step) % SlotCount].IsValid())
			{
				Clockwise = Step;
			}

			if (CounterClockwise == SlotCount
				&& Ring->Occupants[((Index - Step) % SlotCount + SlotCount) % SlotCount].IsValid())
			{
				CounterClockwise = Step;
			}

			if (Clockwise < SlotCount && CounterClockwise < SlotCount) break;
		}

		// 내가 지금 선 방향에서 몇 칸 떨어졌는가
		const int32 RawFromMe = FMath::Abs(Index - Preferred);
		const int32 FromMe = FMath::Min(RawFromMe, SlotCount - RawFromMe);

		const float Spread = static_cast<float>(FMath::Min(Clockwise, CounterClockwise));
		const float GapSpan = static_cast<float>(Clockwise + CounterClockwise);

		const float Score = Spread
			+ GapSpan * GapWeight
			- static_cast<float>(FromMe) * TravelWeight;

		if (Score > BestScore)
		{
			BestScore = Score;
			BestIndex = Index;
		}
	}

	// 자리가 없다. 호출한 쪽이 다른 대상을 고르거나 뒤에서 대기한다.
	if (BestIndex == INDEX_NONE) return INDEX_NONE;

	Ring->Occupants[BestIndex] = InAttacker;
	return BestIndex;
}

void USFCombatSubsystem::ReleaseAttackSlot(AActor* InTarget, AActor* InAttacker)
{
	if (!InAttacker) return;

	// 대상이 이미 파괴 중일 수 있으므로 IsValid 로 거르지 않는다.
	// 약참조 키로 찾기만 하면 되고, 못 찾으면 링이 이미 사라진 것이다.
	FSFAttackSlotRing* Ring = SlotRings.Find(TWeakObjectPtr<AActor>(InTarget));
	if (!Ring) return;

	for (TWeakObjectPtr<AActor>& Occupant : Ring->Occupants)
	{
		if (Occupant.Get() == InAttacker)
		{
			Occupant.Reset();
			break;
		}
	}
}

bool USFCombatSubsystem::GetAttackSlotLocation(const AActor* InTarget, int32 InSlotIndex, float InApproachDistance, FVector& OutLocation) const
{
	if (!IsValid(InTarget) || InSlotIndex < 0) return false;

	const FSFAttackSlotRing* Ring = FindRing(InTarget);
	if (!Ring || !Ring->Occupants.IsValidIndex(InSlotIndex)) return false;

	const int32 SlotCount = Ring->Occupants.Num();
	const float Angle = (2.f * PI * InSlotIndex) / SlotCount;

	const FVector Dir(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
	const FVector Center = InTarget->GetActorLocation();

	// 대상 "표면" 에서 접근 거리만큼 떨어진 지점을 잡는다.
	//
	// 중심에서 최대 반경으로 재면 긴 직사각형 건물의 짧은 변 쪽 슬롯이
	// 사거리 밖에 놓인다. 유닛이 자리에 도착하고도 때리지 못하고,
	// 이동은 이미 끝나서 그대로 굳어버린다.
	//
	// AUnitBase::GetDistanceToTargetEdge 가 박스 표면까지를 재므로
	// 슬롯도 같은 기준을 써야 도착 = 사거리 안이 보장된다.
	FVector Surface = Center + Dir * FMath::Max(1.f, Ring->TargetRadius);

	if (Ring->BoxExtent.X > KINDA_SMALL_NUMBER && Ring->BoxExtent.Y > KINDA_SMALL_NUMBER)
	{
		// 박스는 대상의 현재 위치를 중심으로 다시 세운다.
		// 크기만 저장해 두면 움직이는 대상도 그대로 따라간다.
		const FVector Extent(Ring->BoxExtent.X, Ring->BoxExtent.Y, 1.f);
		const FBox Box(Center - Extent, Center + Extent);

		// 그 방향으로 충분히 멀리 나간 점의 최근접점 = 그 방향의 표면
		const FVector Far = Center + Dir * (Extent.Size2D() + 1000.f);
		Surface = Box.GetClosestPointTo(Far);
	}

	Surface.Z = Center.Z;

	// Z 는 대상 높이를 그대로 쓴다. NavMesh 투영은 이동 요청 쪽에서 처리한다.
	OutLocation = Surface + Dir * FMath::Max(0.f, InApproachDistance);
	return true;
}

int32 USFCombatSubsystem::GetSlotCapacity(const AActor* InTarget) const
{
	const FSFAttackSlotRing* Ring = FindRing(InTarget);
	if (!Ring) return 0;

	const int32 SlotCount = Ring->Occupants.Num();
	const int32 Limit = CVarMaxAttackersPerTarget.GetValueOnGameThread();

	// 상한이 꺼져 있으면 슬롯 개수가 곧 정원이다.
	return (Limit > 0) ? FMath::Min(SlotCount, Limit) : SlotCount;
}

bool USFCombatSubsystem::IsSlotRingFull(const AActor* InTarget) const
{
	// 링이 없다 = 아무도 안 붙었다. 당연히 가득 차지 않았다.
	// 여기서 링을 만들면 후보를 훑을 때마다 바운딩 박스를 재게 되어 비싸다.
	const FSFAttackSlotRing* Ring = FindRing(InTarget);
	if (!Ring) return false;

	// 빈 칸이 남아 있어도 정원을 넘겼으면 꽉 찬 것으로 본다.
	// 이 판정이 FindBestTarget 의 혼잡도 페널티를 깨우고,
	// 자리를 못 받은 유닛은 줄 서는 대신 다른 적으로 흩어진다.
	return GetAttackerCount(InTarget) >= GetSlotCapacity(InTarget);
}

int32 USFCombatSubsystem::GetAttackerCount(const AActor* InTarget) const
{
	const FSFAttackSlotRing* Ring = FindRing(InTarget);
	if (!Ring) return 0;

	int32 Count = 0;
	for (const TWeakObjectPtr<AActor>& Occupant : Ring->Occupants)
	{
		if (IsOccupantHolding(Occupant)) ++Count;
	}
	return Count;
}

uint32 USFCombatSubsystem::GetRingGeneration(const AActor* InTarget) const
{
	const FSFAttackSlotRing* Ring = FindRing(InTarget);
	return Ring ? Ring->Generation : 0;
}

// ─────────────────────────────────────────────────────────
// 공간 격자
//
// 프레임당 한 번만 채우고, 반경 질의는 겹치는 칸만 본다.
// 액터·위치·팀·분류를 채울 때 한 번 확인해 복사해 두므로, 질의 중에는
// ResolveTarget(유효성 + 생존 + GetOwner) 을 다시 돌지 않는다.
// ─────────────────────────────────────────────────────────
FIntPoint USFCombatSubsystem::ToCell(const FVector& InLocation)
{
	return FIntPoint(
		FMath::FloorToInt(InLocation.X / GridCellSize),
		FMath::FloorToInt(InLocation.Y / GridCellSize));
}

void USFCombatSubsystem::EnsureGridCurrent() const
{
	// GFrameCounter 는 프레임마다 증가한다.
	// 한 프레임 안에서 몇 번을 물어도 실제 재구성은 한 번뿐이다.
	if (GridBuiltFrame == GFrameCounter) return;

	GridBuiltFrame = GFrameCounter;
	RebuildGrid();
}

void USFCombatSubsystem::RebuildGrid() const
{
	// 칸 배열은 비우되 키는 남긴다. 다음 프레임에 같은 칸이 다시 쓰일 확률이
	// 높아서, 매번 TMap 을 비우면 해시 재할당이 반복된다.
	for (TPair<FIntPoint, TArray<FSFGridEntry>>& Pair : Grid)
	{
		Pair.Value.Reset();
	}

	GridEntryCount = 0;

	for (const TPair<int32, FSFTargetBucket>& Bucket : TeamBuckets)
	{
		for (const TWeakObjectPtr<UCombatTargetComponent>& Weak : Bucket.Value.Targets)
		{
			AActor* Actor = nullptr;
			if (!ResolveTarget(Weak, Actor)) continue;

			const FVector Location = Actor->GetActorLocation();

			FSFGridEntry Entry;
			Entry.Actor = Actor;
			Entry.Location = FVector2D(Location.X, Location.Y);
			Entry.TeamIndex = Bucket.Key;
			Entry.TargetType = Weak->GetTargetType();

			Grid.FindOrAdd(ToCell(Location)).Add(MoveTemp(Entry));
			++GridEntryCount;
		}
	}

	++StatGridRebuilds;
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
	const FVector2D Origin2D(InOrigin.X, InOrigin.Y);

	// 범위가 있으면 격자로 좁힌다.
	// 범위 제한이 없는 호출(진격 목표 탐색)은 맵 전체가 대상이라
	// 격자로 좁혀지지 않으므로 그대로 전체를 훑는다. 자주 불리지도 않는다.
	if (bLimitRange)
	{
		ForEachEntryInRadius(InOrigin, InMaxRange,
			[&](const FSFGridEntry& Entry)
			{
				if (Entry.TargetType != InTargetType) return;
				if (!IsHostileTeam(InSeekerTeam, Entry.TeamIndex)) return;

				const float DistSq = FVector2D::DistSquared(Origin2D, Entry.Location);
				if (DistSq > MaxRangeSq || DistSq >= BestDistSq) return;

				AActor* Candidate = Entry.Actor.Get();
				if (!IsValid(Candidate)) return;
				if (const auto* Slime = Cast<ASFNeutralSlime>(Candidate))
					if (!Slime->IsProvokedByTeam(InSeekerTeam)) return;

				BestDistSq = DistSq;
				Best = Candidate;
			});

		return Best;
	}

	for (const TPair<int32, FSFTargetBucket>& Pair : TeamBuckets)
	{
		if (!IsHostileTeam(InSeekerTeam, Pair.Key)) continue;

		for (const TWeakObjectPtr<UCombatTargetComponent>& Weak : Pair.Value.Targets)
		{
			// 싼 판별부터 한다. ResolveTarget 은 유효성·생존·소유 액터를 모두 본다.
			if (Weak->GetTargetType() != InTargetType) continue;

			AActor* Candidate = nullptr;
			if (!ResolveTarget(Weak, Candidate)) continue;
			if (const auto* Slime = Cast<ASFNeutralSlime>(Candidate))
				if (!Slime->IsProvokedByTeam(InSeekerTeam)) continue;

			// 수평 거리로 판정한다. 언덕 높이 차로 사거리가 어긋나지 않게.
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
			if (const auto* Slime = Cast<ASFNeutralSlime>(Candidate))
				if (!Slime->IsProvokedByTeam(InSeekerTeam)) continue;

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
	const FVector2D Origin2D(InOrigin.X, InOrigin.Y);

	ForEachEntryInRadius(InOrigin, InRadius,
		[&](const FSFGridEntry& Entry)
		{
			if (!IsHostileTeam(InSeekerTeam, Entry.TeamIndex)) return;

			if (FVector2D::DistSquared(Origin2D, Entry.Location) > RadiusSq) return;

			// 격자를 채운 뒤 이 프레임 안에서 파괴됐을 수 있다.
			AActor* Candidate = Entry.Actor.Get();
			if (!IsValid(Candidate)) return;
			if (const auto* Slime = Cast<ASFNeutralSlime>(Candidate))
				if (!Slime->IsProvokedByTeam(InSeekerTeam)) return;

			OutActors.Add(Candidate);
		});
}

void USFCombatSubsystem::GetNearbyAlliesForSeparation(const AActor* InSelf, int32 InTeamIndex, float InRadius, TArray<AActor*>& OutActors) const
{
	OutActors.Reset();

	if (!IsValid(InSelf) || InRadius <= 0.f) return;

	const FVector Origin = InSelf->GetActorLocation();
	const FVector2D Origin2D(Origin.X, Origin.Y);
	const float RadiusSq = InRadius * InRadius;

	// 이 함수는 근접 조향에서 유닛당 매 틱 불린다. 전체 순회로 두면
	// 프레임당 O(T²) 가 되므로 격자에서 겹치는 칸만 본다.
	ForEachEntryInRadius(Origin, InRadius,
		[&](const FSFGridEntry& Entry)
		{
			if (Entry.TeamIndex != InTeamIndex) return;

			// 건물은 뺀다. 크고 고정된 대상이라 분리 대상에 넣으면 오히려
			// 그 근처에서 밀려나 대상에게 못 붙는 원인이 된다.
			if (Entry.TargetType != ETargetType::CombatUnit) return;

			if (FVector2D::DistSquared(Origin2D, Entry.Location) > RadiusSq) return;

			AActor* Candidate = Entry.Actor.Get();
			if (!IsValid(Candidate) || Candidate == InSelf) return;

			OutActors.Add(Candidate);
		});
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

			// 슬롯 점유 현황을 같이 찍는다.
			// "적은 있는데 아무도 안 때린다" 가 링이 꽉 찬 탓인지 바로 보인다.
			const FSFAttackSlotRing* Ring = FindRing(Owner);

			SF_LOG(Warning, "    %-28s | %-18s | %s | 교전 %d/%d (슬롯 %d칸)%s",
				Owner ? *Owner->GetName() : TEXT("NoOwner"),
				*UEnum::GetDisplayValueAsText(Comp->GetTargetType()).ToString(),
				Comp->IsAlive() ? TEXT("생존") : TEXT("사망"),
				GetAttackerCount(Owner),
				GetSlotCapacity(Owner),
				Ring ? Ring->Occupants.Num() : 0,
				IsSlotRingFull(Owner) ? TEXT(" [정원]") : TEXT(""));
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

void USFCombatSubsystem::DumpStats() const
{
	// 격자를 최신으로 맞춰야 항목 수가 지금 값으로 나온다.
	EnsureGridCurrent();

	const UWorld* World = GetWorld();
	const double Now = IsValid(World) ? World->GetTimeSeconds() : 0.0;
	const double Window = FMath::Max(0.0001, Now - StatWindowStart);

	SF_LOG(Warning, "===== 전투 계측 (최근 %.1f초) =====", Window);

	// ── 등록 현황 ───────────────────────────────────
	TArray<int32> TeamKeys;
	TeamBuckets.GetKeys(TeamKeys);
	TeamKeys.Sort();

	for (const int32 TeamIndex : TeamKeys)
	{
		SF_LOG(Warning, "  Team %-3d 생존 %d / 등록 %d",
			TeamIndex, GetTargetCount(TeamIndex), TeamBuckets[TeamIndex].Targets.Num());
	}

	// ── 격자 ────────────────────────────────────────
	int32 UsedCells = 0;
	int32 MaxCellEntries = 0;

	for (const TPair<FIntPoint, TArray<FSFGridEntry>>& Pair : Grid)
	{
		if (Pair.Value.Num() == 0) continue;

		++UsedCells;
		MaxCellEntries = FMath::Max(MaxCellEntries, Pair.Value.Num());
	}

	SF_LOG(Warning, "  격자        항목 %d / 사용 칸 %d (보유 %d) / 가장 붐비는 칸 %d / 칸크기 %.0f",
		GridEntryCount, UsedCells, Grid.Num(), MaxCellEntries, GridCellSize);

	// ── 링 ──────────────────────────────────────────
	int32 LiveRings = 0;
	int32 TotalAttackers = 0;
	int32 FullRings = 0;

	for (const TPair<TWeakObjectPtr<AActor>, FSFAttackSlotRing>& Pair : SlotRings)
	{
		const AActor* Target = Pair.Key.Get();
		if (!IsValid(Target)) continue;

		++LiveRings;

		const int32 Attackers = GetAttackerCount(Target);
		TotalAttackers += Attackers;

		if (Attackers >= GetSlotCapacity(Target)) ++FullRings;
	}

	SF_LOG(Warning, "  공격 슬롯   링 %d (정원참 %d) / 총 공격자 %d / 평균 %.1f",
		LiveRings, FullRings, TotalAttackers,
		LiveRings > 0 ? static_cast<float>(TotalAttackers) / LiveRings : 0.f);

	// ── 질의 부하 ───────────────────────────────────
	//
	// 훑은 항목 수가 격자 효과를 직접 보여준다.
	// 격자 이전에는 질의 한 번마다 등록 대상 전체를 훑었다.
	SF_LOG(Warning, "  반경 질의   %d회 (초당 %.0f) / 훑은 항목 %d (질의당 %.1f)",
		StatRadiusQueries,
		StatRadiusQueries / Window,
		StatEntriesVisited,
		StatRadiusQueries > 0 ? static_cast<float>(StatEntriesVisited) / StatRadiusQueries : 0.f);

	SF_LOG(Warning, "  슬롯 배정   %d회 (초당 %.0f) / 격자 재구성 %d회 (초당 %.0f)",
		StatSlotClaims, StatSlotClaims / Window,
		StatGridRebuilds, StatGridRebuilds / Window);

	// 창을 비운다. 다음 호출은 이 시점부터의 값을 보여준다.
	StatRadiusQueries = 0;
	StatEntriesVisited = 0;
	StatGridRebuilds = 0;
	StatSlotClaims = 0;
	StatWindowStart = Now;
}

#if !UE_BUILD_SHIPPING
static void SFDumpCombatStats(const TArray<FString>& Args, UWorld* World)
{
	const USFCombatSubsystem* Combat = IsValid(World) ? World->GetSubsystem<USFCombatSubsystem>() : nullptr;

	if (!IsValid(Combat))
	{
		SF_LOG(Error, "USFCombatSubsystem 을 찾지 못했습니다. 플레이 중인지 확인하세요.");
		return;
	}

	Combat->DumpStats();
}

static FAutoConsoleCommandWithWorldAndArgs GSFDumpCombatStats(
	TEXT("sf.CombatStats"),
	TEXT("전투 시스템 계측값을 출력하고 측정 창을 초기화한다."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SFDumpCombatStats));
#endif

static FAutoConsoleCommandWithWorldAndArgs GSFDumpCombatTargets(
	TEXT("sf.DumpCombatTargets"),
	TEXT("팀별 전투 대상 등록 현황을 출력한다."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SFDumpCombatTargets));
#endif
