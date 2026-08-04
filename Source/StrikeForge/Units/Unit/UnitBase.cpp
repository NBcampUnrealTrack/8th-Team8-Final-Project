#include "UnitBase.h"

#include "UnitAIController.h"
#include "ActorComponent/HP/HealthComponent.h"
#include "Game/Subsystem/SFCombatSubsystem.h"
#include "Game/Subsystem/SFDataTableManager.h"
#include "Units/Lane/LanePath.h"

#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"

AUnitBase::AUnitBase()
	: bRequireLineOfSight(true)
	, bDrawCombatDebug(false)
	, bStatInitialized(false)
{
	PrimaryActorTick.bCanEverTick = true;

	// 1 프로토 유닛 = 150cm 기준. 일반 유닛 신장 약 180cm
	GetCapsuleComponent()->InitCapsuleSize(45.f, 90.f);

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));

	StatusWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("StatusWidget"));
	StatusWidget->SetupAttachment(RootComponent);
	StatusWidget->SetWidgetSpace(EWidgetSpace::Screen);
	StatusWidget->SetRelativeLocation(FVector(0.f, 0.f, 130.f));

	// 스폰되든 레벨에 배치되든 자동으로 AI 가 붙는다
	AIControllerClass = AUnitAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bUseRVOAvoidance = false;		// DetourCrowd 를 쓰므로 RVO 는 끈다
		Move->bOrientRotationToMovement = true;
		Move->RotationRate = FRotator(0.f, 540.f, 0.f);
		Move->MaxWalkSpeed = 400.f;
	}

	bUseControllerRotationYaw = false;
}

void AUnitBase::BeginPlay()
{
	Super::BeginPlay();

	// 레벨에 직접 배치한 유닛은 스포너를 거치지 않으므로 여기서 주입한다.
	// 스폰으로 이미 주입됐다면 건너뛴다.
	if (!bStatInitialized && !UnitRowName.IsNone())
	{
		InitStatFromTable(UnitRowName);
	}

	if (HealthComponent)
	{
		HealthComponent->OnHealthStateChanged.AddDynamic(this, &AUnitBase::HandleDeath);
	}

	// 전투 레지스트리에 등록 — 이래야 상대가 나를 찾을 수 있다
	if (USFCombatSubsystem* Combat = USFCombatSubsystem::Get(this))
	{
		Combat->RegisterCombatant(this);
	}

	RetargetTimer = 0.f;	// 첫 틱에 바로 탐색이 돌게 한다
}

void AUnitBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 레벨 전환·강제 파괴 등 어떤 경로로 사라져도 반드시 해제한다
	if (USFCombatSubsystem* Combat = USFCombatSubsystem::Get(this))
	{
		Combat->UnregisterCombatant(this);
	}

	Super::EndPlay(EndPlayReason);
}

// ─────────────────────────────────────────────────────────
// 초기화
// ─────────────────────────────────────────────────────────
void AUnitBase::InitStat(const FUnitStatData& InRow)
{
	UnitRole		= InRow.Role;
	Armor			= InRow.Armor;
	Attack			= InRow.Attack;
	AttackRange		= InRow.AttackRange;
	AttackCooldown	= InRow.AttackCooldown;
	Penetration		= InRow.Penetration;
	SplashRadius	= InRow.SplashRadius;

	if (HealthComponent)
	{
		HealthComponent->InitHealth(InRow.MaxHealth);
	}

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = InRow.MoveSpeed;
	}

	bStatInitialized = true;
}

bool AUnitBase::InitStatFromTable(FName InRowName)
{
	if (InRowName.IsNone()) return false;

	const UWorld* World = GetWorld();
	UGameInstance* GI = IsValid(World) ? World->GetGameInstance() : nullptr;
	if (!IsValid(GI)) return false;

	USFDataTableManager* TableManager = GI->GetSubsystem<USFDataTableManager>();
	if (!IsValid(TableManager)) return false;

	const FUnitStatData* Row = TableManager->GetDataRow<FUnitStatData>(EDataTableType::Unit, InRowName, TEXT("AUnitBase::InitStatFromTable"));
	if (!Row) return false;

	UnitRowName = InRowName;
	InitStat(*Row);
	return true;
}

void AUnitBase::SetAssignedLane(ALanePath* InLane)
{
	AssignedLane = InLane;
	LaneAlpha = 0.f;
}

bool AUnitBase::IsTargetable_Implementation() const
{
	return IsValid(HealthComponent) && !HealthComponent->IsDead();
}

bool AUnitBase::PrefersWorkerOverBuilding() const
{
	// 돌격 · 암살 · 광역은 경제 견제 역할을 갖는다
	return UnitRole == EUnitRole::Charge
		|| UnitRole == EUnitRole::Assassin
		|| UnitRole == EUnitRole::Splash;
}

AUnitAIController* AUnitBase::GetUnitAI() const
{
	return Cast<AUnitAIController>(GetController());
}

// ─────────────────────────────────────────────────────────
// 피해 수신
// ─────────────────────────────────────────────────────────
void AUnitBase::TakeDamaged_Implementation(float InDamage)
{
	if (!IsValid(HealthComponent)) return;

	// 방어 감산은 공격자가 관통을 반영해 이미 계산했다.
	// 여기서는 최종 피해를 그대로 적용한다.
	HealthComponent->ApplyDamage(InDamage);
}

// ─────────────────────────────────────────────────────────
// 사망 — HealthComponent 델리게이트
// ─────────────────────────────────────────────────────────
void AUnitBase::HandleDeath()
{
	if (CombatState == EUnitCombatState::Dead) return;

	CurrentTarget = nullptr;
	SetCombatState(EUnitCombatState::Dead);

	// 1) 레지스트리에서 즉시 뺀다.
	//    이걸 먼저 해야 다른 유닛이 시체를 계속 때리지 않는다.
	if (USFCombatSubsystem* Combat = USFCombatSubsystem::Get(this))
	{
		Combat->UnregisterCombatant(this);
	}

	// 2) AI 정지 및 컨트롤러 정리.
	//    AIController 는 폰이 죽어도 자동 소멸하지 않으므로 직접 없앤다.
	if (AController* MyController = GetController())
	{
		MyController->StopMovement();

		if (AAIController* AI = Cast<AAIController>(MyController))
		{
			AI->UnPossess();
			AI->Destroy();
		}
	}

	// 3) 이동·충돌 차단. 시체가 살아있는 유닛의 길을 막지 않게 한다.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorEnableCollision(false);

	if (StatusWidget)
	{
		StatusWidget->SetVisibility(false);
	}

	// 4) 연출 훅. 래그돌·디졸브는 BP 에서.
	OnUnitDied();

	if (DeathLifeSpan > 0.f)
	{
		SetLifeSpan(DeathLifeSpan);
	}
	else
	{
		Destroy();
	}
}

// ─────────────────────────────────────────────────────────
// 거리 · 시야
// ─────────────────────────────────────────────────────────
bool AUnitBase::IsTargetUsable(const AActor* InTarget) const
{
	if (!IsValid(InTarget) || !InTarget->Implements<UTargetable>()) return false;

	// 인터페이스 호출은 const 를 받지 않으므로 여기서만 벗겨낸다
	AActor* Mutable = const_cast<AActor*>(InTarget);

	return ITargetable::Execute_IsTargetable(Mutable)
		&& USFCombatSubsystem::IsHostileTeam(TeamIndex, ITargetable::Execute_GetTeamIndex(Mutable));
}

float AUnitBase::GetDistanceToTargetEdge(const AActor* InTarget) const
{
	if (!IsValid(InTarget)) return TNumericLimits<float>::Max();

	const FVector Self = GetActorLocation();

	// 건물은 메시가 크기 때문에 원점 거리로 재면 사거리 안에 들어와도
	// 계속 접근하려 든다. 바운딩 박스 표면까지의 거리를 쓴다.
	//
	// false = 콜리전 없는 컴포넌트 제외. StatusWidget 같은 위젯이
	// 박스를 부풀려 거리를 실제보다 짧게 만드는 것을 막는다.
	const FBox Box = InTarget->GetComponentsBoundingBox(false);
	if (!Box.IsValid)
	{
		return FVector::Dist2D(Self, InTarget->GetActorLocation());
	}

	return FVector::Dist2D(Self, Box.GetClosestPointTo(Self));
}

bool AUnitBase::HasLineOfSight(const AActor* InTarget) const
{
	if (!bRequireLineOfSight) return true;		// 곡사 유닛은 시야를 요구하지 않는다
	if (!IsValid(InTarget)) return false;

	const FVector Start = GetActorLocation() + FVector(0.f, 0.f, EyeHeight);
	const FVector End   = InTarget->GetActorLocation() + FVector(0.f, 0.f, EyeHeight);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SFLineOfSight), false, this);
	Params.AddIgnoredActor(InTarget);

	// 선이 아니라 구체를 굴린다.
	//
	// 선으로 검사하면 벽 모서리를 스치듯 통과하는 순간 시야가 열려버려서,
	// 유닛이 모서리에 바짝 붙은 채 코너 너머를 찌르는 그림이 나온다.
	// 구체는 모서리에 걸리므로, 몸이 실제로 코너를 빠져나와야 시야가 열린다.
	//
	// WorldStatic 만 검사한다. Visibility 로 하면 다른 유닛까지 시야를 막아
	// 앞줄에 가린 뒷줄이 공격을 못 하게 된다.
	const FCollisionShape Probe = FCollisionShape::MakeSphere(LineOfSightRadius);

	FHitResult Hit;
	const bool bBlocked = GetWorld()->SweepSingleByChannel(
		Hit, Start, End, FQuat::Identity, ECC_WorldStatic, Probe, Params);

#if ENABLE_DRAW_DEBUG
	if (bDrawCombatDebug)
	{
		DrawDebugLine(GetWorld(), Start, End, bBlocked ? FColor::Black : FColor::Green, false, -1.f, 0, 2.f);

		if (bBlocked)
		{
			DrawDebugSphere(GetWorld(), Hit.Location, LineOfSightRadius, 12, FColor::Black, false, -1.f, 0, 1.f);
		}
	}
#endif

	return !bBlocked;
}

// ─────────────────────────────────────────────────────────
// 타겟 선정
//
// "가장 가까운 적"이 아니라 "가장 때리기 좋은 적"을 고른다.
//  - 벽 너머라 시야가 안 열리는 대상은 후순위로 밀어낸다
//  - 지금 잡고 있는 대상에 가산점을 줘 타겟이 매번 뒤바뀌는 것을 막는다
//
// 이 두 가지가 없으면 아군 전원이 같은 적 하나로 몰려가고,
// 뒤엉킨 상태에서 최근접 대상이 계속 바뀌면서 제자리 밀치기가 된다.
// ─────────────────────────────────────────────────────────
AActor* AUnitBase::FindBestTarget()
{
	USFCombatSubsystem* Combat = USFCombatSubsystem::Get(this);
	if (!Combat) return nullptr;

	const FVector Origin = GetActorLocation();

	TArray<AActor*> Candidates;
	Combat->GetHostilesInRadius(Origin, TeamIndex, AggroRadius, Candidates);

	if (Candidates.Num() == 0) return nullptr;

	// 우선순위표. 클래스 캐스팅 없이 ETargetType 만으로 판정한다.
	TArray<ETargetType, TInlineAllocator<5>> Priority;

	Priority.Add(ETargetType::CombatUnit);		// 1) 적 전투 유닛
	Priority.Add(ETargetType::Commander);		// 2) 적 지휘관

	if (PrefersWorkerOverBuilding())			// 3) 역할에 따라 순서가 뒤바뀐다
	{
		Priority.Add(ETargetType::Worker);
		Priority.Add(ETargetType::ProductionBuilding);
	}
	else
	{
		Priority.Add(ETargetType::ProductionBuilding);
		Priority.Add(ETargetType::Worker);
	}

	Priority.Add(ETargetType::CoreBuilding);	// 5) 최후

	// 거리순으로 미리 정렬해 둔다.
	// 시야 검사는 비싸므로 가까운 쪽부터 제한된 횟수만 검사한다.
	Candidates.Sort([&Origin](const AActor& A, const AActor& B)
	{
		return FVector::DistSquared2D(Origin, A.GetActorLocation())
			 < FVector::DistSquared2D(Origin, B.GetActorLocation());
	});

	for (const ETargetType Type : Priority)
	{
		AActor* Best = nullptr;
		float BestScore = TNumericLimits<float>::Max();
		int32 LosChecksLeft = MaxLineOfSightChecks;

		for (AActor* Candidate : Candidates)
		{
			if (ITargetable::Execute_GetTargetType(Candidate) != Type) continue;

			float Score = FVector::Dist2D(Origin, Candidate->GetActorLocation());

			// 시야가 막힌 대상은 점수를 불린다.
			// 완전히 배제하지는 않는다 — 전부 벽 뒤라면 그쪽으로라도 가야 한다.
			if (LosChecksLeft > 0)
			{
				--LosChecksLeft;

				if (!HasLineOfSight(Candidate))
				{
					Score *= BlockedTargetPenalty;
				}
			}

			if (Candidate == CurrentTarget)
			{
				Score *= TargetStickiness;
			}

			if (Score < BestScore)
			{
				BestScore = Score;
				Best = Candidate;
			}
		}

		if (Best) return Best;
	}

	// 교전 반경 안에 아무것도 없다 → Advance() 가 전진을 맡는다
	return nullptr;
}

// ─────────────────────────────────────────────────────────
// 공격 실행
// ─────────────────────────────────────────────────────────
void AUnitBase::PerformAttack(AActor* InTarget)
{
	if (!IsTargetUsable(InTarget)) return;
	if (!InTarget->Implements<UDamagable>()) return;

	const float RawDamage = Attack * AttackMultiplier;

	// 최종 피해 = 공격력 * (1 - max(0, 방어 - 관통))
	// 방어 감산을 공격자가 계산하는 이유는 IDamagable 이 인자 하나만 받기 때문이다.
	auto ComputeDamage = [this, RawDamage](AActor* InVictim) -> float
	{
		const float VictimArmor = ITargetable::Execute_GetArmor(InVictim);
		const float Effective = FMath::Clamp(VictimArmor - Penetration, 0.f, 0.9f);
		return RawDamage * (1.f - Effective);
	};

	IDamagable::Execute_TakeDamaged(InTarget, ComputeDamage(InTarget));

	if (SplashRadius > 0.f)
	{
		if (USFCombatSubsystem* Combat = USFCombatSubsystem::Get(this))
		{
			TArray<AActor*> Splashed;
			Combat->GetHostilesInRadius(InTarget->GetActorLocation(), TeamIndex, SplashRadius, Splashed);

			for (AActor* Other : Splashed)
			{
				if (Other == InTarget) continue;			// 주 대상은 이미 맞았다

				// 앞선 피해로 이미 파괴됐을 수 있으므로 매번 다시 확인한다
				if (!IsValid(Other) || !Other->Implements<UDamagable>()) continue;

				IDamagable::Execute_TakeDamaged(Other, ComputeDamage(Other) * SplashDamageRatio);
			}
		}
	}

	OnAttackPerformed(InTarget);
}

// ─────────────────────────────────────────────────────────
// 전진 — 교전 대상이 없을 때
// ─────────────────────────────────────────────────────────
void AUnitBase::Advance()
{
	AUnitAIController* AI = GetUnitAI();
	if (!AI) return;

	// 라인이 지정돼 있으면 스플라인을 따라 단계적으로 전진한다.
	// 라인은 "대략적인 진격로"일 뿐이고, 벽 회피는 NavMesh 가 맡는다.
	if (AssignedLane)
	{
		const FVector Waypoint = AssignedLane->GetPointAtAlpha(LaneAlpha);

		if (FVector::Dist2D(GetActorLocation(), Waypoint) < AssignedLane->GetLaneHalfWidth())
		{
			LaneAlpha = FMath::Min(1.f, LaneAlpha + 0.1f);
		}

		AI->RequestMoveToLocation(AssignedLane->GetPointAtAlpha(LaneAlpha), MoveAcceptanceRadius);
		return;
	}

	// 주의: 여기서 멈춰 세우면(ClearMoveRequest) 유닛이 영구히 굳는다.
	// 반드시 갈 곳을 찾아내야 한다.
	if (USFCombatSubsystem* Combat = USFCombatSubsystem::Get(this))
	{
		const FVector Origin = GetActorLocation();

		// 1순위 — 적 핵심 건물 (거리 제한 없음)
		if (AActor* Core = Combat->FindNearestHostile(Origin, TeamIndex, ETargetType::CoreBuilding, 0.f))
		{
			AI->RequestMoveToActor(Core, MoveAcceptanceRadius);
			return;
		}

		// 2순위 — 핵심 건물이 없는 맵(테스트 레벨 등)에서는
		//         교전 반경 밖이라도 가장 가까운 적을 향해 전진한다.
		static const ETargetType FallbackOrder[] = {
			ETargetType::CombatUnit,
			ETargetType::Commander,
			ETargetType::ProductionBuilding,
			ETargetType::Worker
		};

		AActor* Nearest = nullptr;
		float NearestDistSq = TNumericLimits<float>::Max();

		for (const ETargetType Type : FallbackOrder)
		{
			if (AActor* Found = Combat->FindNearestHostile(Origin, TeamIndex, Type, 0.f))
			{
				const float DistSq = FVector::DistSquared2D(Origin, Found->GetActorLocation());
				if (DistSq < NearestDistSq)
				{
					NearestDistSq = DistSq;
					Nearest = Found;
				}
			}
		}

		if (Nearest)
		{
			AI->RequestMoveToActor(Nearest, MoveAcceptanceRadius);
			return;
		}
	}

	// 맵에 적이 하나도 남지 않은 경우에만 정지한다
	AI->ClearMoveRequest();
}

void AUnitBase::SetCombatState(EUnitCombatState InNewState)
{
	if (CombatState == InNewState) return;

	CombatState = InNewState;
	OnCombatStateChanged(InNewState);
}

void AUnitBase::FaceTarget(float DeltaSeconds)
{
	if (!CurrentTarget) return;

	FVector ToTarget = CurrentTarget->GetActorLocation() - GetActorLocation();
	ToTarget.Z = 0.f;

	if (ToTarget.IsNearlyZero()) return;

	SetActorRotation(FMath::RInterpTo(GetActorRotation(), ToTarget.Rotation(), DeltaSeconds, 10.f));
}

// ─────────────────────────────────────────────────────────
// 메인 루프
// ─────────────────────────────────────────────────────────
void AUnitBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (CombatState == EUnitCombatState::Dead) return;

	if (AttackCooldownRemaining > 0.f)
	{
		AttackCooldownRemaining -= DeltaSeconds;
	}

	// ── 1. 타겟 갱신 ────────────────────────────────────
	// 타겟이 죽었으면 즉시, 아니면 주기마다 재탐색한다.
	RetargetTimer -= DeltaSeconds;

	if (!IsTargetUsable(CurrentTarget) || RetargetTimer <= 0.f)
	{
		RetargetTimer = RetargetInterval;

		AActor* NewTarget = FindBestTarget();

		if (NewTarget != CurrentTarget)
		{
			CurrentTarget = NewTarget;

			// 여기서 StopMovement 를 부르지 않는다.
			// 타겟이 바뀔 때마다 멈춰 세우면, 뒤엉킨 난전에서 최근접 대상이
			// 계속 뒤바뀌며 제자리 밀치기가 된다.
			if (AUnitAIController* AI = GetUnitAI())
			{
				AI->ForceRepath();
			}
		}
	}

	// ── 2. 교전 대상 없음 → 전진 ────────────────────────
	if (!CurrentTarget)
	{
		SetCombatState(EUnitCombatState::Advancing);
		Advance();
		return;
	}

	// ── 3. 사거리 · 시야 판정 ───────────────────────────
	const float Distance = GetDistanceToTargetEdge(CurrentTarget);

	// 교전 시작 문턱과 유지 문턱을 분리한다.
	//   시작: AttackRange * AttackApproachRatio  — 제대로 붙어야 때린다
	//   유지: AttackRange + RangeHysteresis      — 한번 붙으면 조금 벌어져도 계속
	const bool bWasAttacking = (CombatState == EUnitCombatState::Attacking);

	const float EffectiveRange = bWasAttacking
		? AttackRange + RangeHysteresis
		: AttackRange * AttackApproachRatio;

	const bool bInRange = (Distance <= EffectiveRange);

	// 사거리 안일 때만 스윕한다. 밖이면 어차피 이동해야 하므로 낭비다.
	const bool bCanSee = bInRange ? HasLineOfSight(CurrentTarget) : false;

	if (bInRange && bCanSee)
	{
		// ── 교전 ────────────────────────────────────────
		SetCombatState(EUnitCombatState::Attacking);
		StuckTimer = 0.f;

		if (AUnitAIController* AI = GetUnitAI())
		{
			AI->ClearMoveRequest();
		}

		FaceTarget(DeltaSeconds);

		if (AttackCooldownRemaining <= 0.f)
		{
			PerformAttack(CurrentTarget);
			AttackCooldownRemaining = AttackCooldown;
		}
	}
	else
	{
		// ── 추격 ────────────────────────────────────────
		SetCombatState(EUnitCombatState::Chasing);

		if (AUnitAIController* AI = GetUnitAI())
		{
			// 도달 반경에 사거리를 넘기지 않는다.
			// 언리얼은 넘긴 값에 목표 반지름을 더해 도달을 판정하므로,
			// 사거리를 넘기면 벽을 돌기도 전에 "도착했다"고 판단해 멈춘다.
			AI->RequestMoveToActor(CurrentTarget, MoveAcceptanceRadius);

			// 끼임 감지 — 이동 명령은 살아 있는데 실제로 안 움직이는 경우.
			// 경로 캐시만 버리고 다음 틱에 새로 계산하게 한다.
			// StopMovement 를 쓰면 아군에 잠깐 막힌 것뿐인데도 멈춰 서서
			// 난전에서 전열이 통째로 굳는다.
			if (GetVelocity().SizeSquared2D() < FMath::Square(20.f))
			{
				StuckTimer += DeltaSeconds;

				if (StuckTimer >= StuckThreshold)
				{
					StuckTimer = 0.f;
					AI->ForceRepath();
				}
			}
			else
			{
				StuckTimer = 0.f;
			}
		}
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawCombatDebug)
	{
		DrawDebugCircle(GetWorld(), GetActorLocation(), AggroRadius, 32, FColor::Yellow, false, -1.f, 0, 3.f,
			FVector(1, 0, 0), FVector(0, 1, 0), false);

		if (CurrentTarget)
		{
			const FColor LineColor = (CombatState == EUnitCombatState::Attacking) ? FColor::Red : FColor::Orange;
			DrawDebugLine(GetWorld(), GetActorLocation(), CurrentTarget->GetActorLocation(), LineColor, false, -1.f, 0, 4.f);
		}

		// 상태를 머리 위에 띄운다. 어느 분기에 있는지 이게 없으면 알 수 없다.
		const TCHAR* StateText =
			  CombatState == EUnitCombatState::Advancing ? TEXT("전진")
			: CombatState == EUnitCombatState::Chasing   ? TEXT("추격")
			: CombatState == EUnitCombatState::Attacking ? TEXT("교전")
			:                                              TEXT("사망");

		const FString Info = FString::Printf(
			TEXT("Team%d | %s | 대상:%s | 거리:%.0f | 속도:%.0f | HP:%.0f"),
			TeamIndex,
			StateText,
			CurrentTarget ? *CurrentTarget->GetName() : TEXT("없음"),
			CurrentTarget ? GetDistanceToTargetEdge(CurrentTarget) : -1.f,
			GetVelocity().Size2D(),
			HealthComponent ? HealthComponent->GetCurrentHealth() : 0.f);

		DrawDebugString(GetWorld(), FVector(0.f, 0.f, 140.f), Info, this, FColor::White, 0.f, true);
	}
#endif
}
