#include "UnitBase.h"

#include "UnitAIController.h"
#include "ActorComponent/HP/HealthComponent.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "Game/Subsystem/SFCombatSubsystem.h"
#include "Game/Subsystem/SFDataTableManager.h"
#include "InGame/SFSelectINfo/SFSelectionUIDataRow.h"
#include "Units/Lane/LanePath.h"

#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "Logging/SFLog.h"

/**
 * 전투 디버그 표시 전역 스위치.
 *
 * 유닛이 수십 기가 되면 개별 bDrawCombatDebug 를 켜고 끄는 게 불가능하다.
 * 콘솔에서 한 번에 제어한다.
 *
 *   sf.DrawCombatDebug 1   전체 켜기
 *   sf.DrawCombatDebug 0   전체 끄기 (유닛별 설정은 그대로 유지)
 */
static TAutoConsoleVariable<int32> CVarDrawCombatDebug(
	TEXT("sf.DrawCombatDebug"),
	0,
	TEXT("전투 AI 디버그 표시 (0=off, 1=on). 유닛별 설정과 OR 로 동작한다."),
	ECVF_Cheat);

AUnitBase::AUnitBase()
	: bRequireLineOfSight(true)
	, bDrawCombatDebug(false)
	, bStatInitialized(false)
	, bNoTargetReported(false)
	, bAdvanceTargetReported(false)
{
	PrimaryActorTick.bCanEverTick = true;

	// 1 프로토 유닛 = 150cm 기준. 일반 유닛 신장 약 180cm
	GetCapsuleComponent()->InitCapsuleSize(45.f, 90.f);

	// HUD 클릭 선택은 ECC_Visibility 트레이스를 쓴다.
	// 그런데 ACharacter 기본 Pawn 프로파일은 Visibility 를 Ignore 하므로
	// 커서 트레이스가 유닛을 그대로 통과해 버린다.
	// (건물은 StaticMeshComponent 라 BlockAll 기본값이라 문제가 없었다)
	//
	// 다른 채널 응답은 건드리지 않고 Visibility 만 Block 으로 바꾼다.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));

	CombatTarget = CreateDefaultSubobject<UCombatTargetComponent>(TEXT("CombatTarget"));
	CombatTarget->SetTargetType(ETargetType::CombatUnit);

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

	// 레지스트리 등록·해제는 CombatTargetComponent 가 담당한다.
	// 여기서는 파괴 통지만 받는다.
	if (CombatTarget)
	{
		CombatTarget->OnTargetDestroyed.AddDynamic(this, &AUnitBase::HandleDeath);
	}

	RetargetTimer = 0.f;	// 첫 틱에 바로 탐색이 돌게 한다
}

void AUnitBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 레지스트리 해제는 CombatTargetComponent::EndPlay 가 처리한다
	Super::EndPlay(EndPlayReason);
}

// ─────────────────────────────────────────────────────────
// 초기화
// ─────────────────────────────────────────────────────────
void AUnitBase::InitStat(const FUnitStatData& InRow)
{
	UnitRole		= InRow.Role;

	if (CombatTarget)
	{
		CombatTarget->SetArmor(InRow.Armor);
	}
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

	// 조회 성공 여부와 무관하게 정체성은 먼저 기록한다.
	// 실패해도 이 유닛이 무엇이었는지는 남아야 디버그·UI 에서 추적이 된다.
	UnitRowName = InRowName;

	const UWorld* World = GetWorld();
	UGameInstance* GI = IsValid(World) ? World->GetGameInstance() : nullptr;
	if (!IsValid(GI)) return false;

	USFDataTableManager* TableManager = GI->GetSubsystem<USFDataTableManager>();
	if (!IsValid(TableManager)) return false;

	const FUnitStatData* Row = TableManager->GetDataRow<FUnitStatData>(EDataTableType::Unit, InRowName, TEXT("AUnitBase::InitStatFromTable"));

	if (!Row)
	{
		// 능력치가 BP 기본값으로 남는다. 조용히 넘어가면
		// "왜 체력이 100 이지" 로 한참 헤매게 된다.
		SF_LOG(Warning,
			"[Unit] %s : DT_Units 에서 '%s' 행을 찾지 못했습니다. BP 기본 능력치로 동작합니다.",
			*GetName(), *InRowName.ToString());
		return false;
	}

	InitStat(*Row);
	return true;
}

void AUnitBase::SetAssignedLane(ALanePath* InLane)
{
	AssignedLane = InLane;
	LaneAlpha = 0.f;
}

int32 AUnitBase::GetTeamIndex() const
{
	return CombatTarget ? CombatTarget->GetTeamIndex() : Targetable::NeutralTeam;
}

AUnitAIController* AUnitBase::GetUnitAI() const
{
	return Cast<AUnitAIController>(GetController());
}

bool AUnitBase::ShouldDrawDebug() const
{
#if ENABLE_DRAW_DEBUG
	return bDrawCombatDebug || CVarDrawCombatDebug.GetValueOnGameThread() > 0;
#else
	return false;
#endif
}

// ─────────────────────────────────────────────────────────
// 사망 — HealthComponent 델리게이트
// ─────────────────────────────────────────────────────────
void AUnitBase::HandleDeath(UCombatTargetComponent* InTarget)
{
	if (CombatState == EUnitCombatState::Dead) return;

	CurrentTarget = nullptr;
	SetCombatState(EUnitCombatState::Dead);

	// 레지스트리 해제는 CombatTargetComponent 가 이미 처리했다.

	// AI 정지 및 컨트롤러 정리.
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

	// 이동·충돌 차단. 시체가 살아있는 유닛의 길을 막지 않게 한다.
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

	// 연출 훅. 래그돌·디졸브는 BP 에서.
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
// HUD 선택 정보
//
// 런타임 값과 표시용 값의 출처가 다르다.
//   런타임  : HealthComponent, 현재 스탯      — 매 선택 시점의 실제 값
//   표시용  : EDataTableType::UI 테이블        — 초상화·설명·표시 여부
//
// 두 테이블 모두 UnitRowName 을 키로 쓴다.
// FSFSelectableInfo::DataRowName 주석에 적힌 "공통 키" 설계 그대로다.
// ─────────────────────────────────────────────────────────
FSFSelectableInfo AUnitBase::GetSelectableInfo_Implementation()
{
	FSFSelectableInfo Info;

	Info.DataRowName = UnitRowName;
	Info.SelectableType = ESFSelectableType::Unit;

	// ── 런타임 값 ───────────────────────────────────────
	if (IsValid(HealthComponent))
	{
		Info.CurrentHealth = HealthComponent->GetCurrentHealth();
		Info.MaximumHealth = HealthComponent->GetMaxHealth();
	}

	// 지휘관 버프 등이 적용된 실제 공격력을 보여준다
	Info.AttackPower = Attack * AttackMultiplier;

	if (const UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Info.MoveSpeed = Move->MaxWalkSpeed;
	}

	// ── 테이블 값 ───────────────────────────────────────
	const UWorld* World = GetWorld();
	UGameInstance* GI = IsValid(World) ? World->GetGameInstance() : nullptr;
	USFDataTableManager* TableManager = IsValid(GI) ? GI->GetSubsystem<USFDataTableManager>() : nullptr;

	// RowName 이 없으면 런타임 값만 담아 돌려준다.
	// 스포너를 거치지 않고 레벨에 직접 배치한 유닛이 여기 해당한다.
	if (!IsValid(TableManager) || UnitRowName.IsNone())
	{
		return Info;
	}

	if (const FUnitStatData* StatRow =
		TableManager->GetDataRow<FUnitStatData>(EDataTableType::Unit, UnitRowName, TEXT("AUnitBase::GetSelectableInfo")))
	{
		Info.DisplayName = StatRow->DisplayName;
	}

	if (const FSFSelectionUIDataRow* UIRow =
		TableManager->GetDataRow<FSFSelectionUIDataRow>(EDataTableType::UI, UnitRowName, TEXT("AUnitBase::GetSelectableInfo")))
	{
		// UI 테이블에 이름이 따로 있으면 그쪽을 우선한다.
		// 스탯 테이블 이름은 개발용, UI 이름은 현지화 대상인 경우가 있다.
		if (!UIRow->DisplayName.IsEmpty())
		{
			Info.DisplayName = UIRow->DisplayName;
		}

		// TSoftObjectPtr 이므로 여기서 로드한다.
		// 선택은 사용자 입력 시점에만 일어나므로 동기 로드로 충분하다.
		Info.PortraitImage = UIRow->PortraitImage.LoadSynchronous();
		Info.SelectionIcon = UIRow->SelectionIcon.LoadSynchronous();
		Info.Description = UIRow->Description;

		Info.bShowHealth = UIRow->bShowHealth;
		Info.bShowAttackPower = UIRow->bShowAttackPower;
		Info.bShowMoveSpeed = UIRow->bShowMoveSpeed;
	}

	return Info;
}

// ─────────────────────────────────────────────────────────
// 거리 · 시야
// ─────────────────────────────────────────────────────────
bool AUnitBase::IsTargetUsable(const AActor* InTarget) const
{
	UCombatTargetComponent* Comp = USFCombatSubsystem::FindTargetComponent(InTarget);
	if (!IsValid(Comp) || !Comp->IsAlive()) return false;

	return USFCombatSubsystem::IsHostileTeam(GetTeamIndex(), Comp->GetTeamIndex());
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
	if (ShouldDrawDebug())
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
// 타겟 선정 — 시야가 열린 가장 가까운 적
//
// 점수 = 거리 x 분류가중치 x 시야페널티 x 타겟고정력  (낮을수록 우선)
//
// 분류 가중치는 기본이 전부 1.0 이라 순수 최근접이 된다.
// 특정 분류를 우선하고 싶어지면 TargetTypeWeights 만 조정하면 되고
// 코드는 손대지 않는다.
//
// 시야 페널티와 타겟 고정력이 없으면 아군 전원이 같은 적 하나로 몰려가고,
// 뒤엉킨 상태에서 최근접 대상이 계속 바뀌면서 제자리 밀치기가 된다.
// ─────────────────────────────────────────────────────────
AActor* AUnitBase::FindBestTarget()
{
	USFCombatSubsystem* Combat = USFCombatSubsystem::Get(this);
	if (!Combat) return nullptr;

	const FVector Origin = GetActorLocation();
	const int32 MyTeam = GetTeamIndex();

	TArray<AActor*> Candidates;
	Combat->GetHostilesInRadius(Origin, MyTeam, AggroRadius, Candidates);

	if (Candidates.Num() == 0) return nullptr;

	// 거리순으로 미리 정렬해 둔다.
	// 시야 검사는 비싸므로 가까운 쪽부터 제한된 횟수만 검사한다.
	Candidates.Sort([&Origin](const AActor& A, const AActor& B)
	{
		return FVector::DistSquared2D(Origin, A.GetActorLocation())
			 < FVector::DistSquared2D(Origin, B.GetActorLocation());
	});

	AActor* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	int32 LosChecksLeft = MaxLineOfSightChecks;

	for (AActor* Candidate : Candidates)
	{
		const UCombatTargetComponent* Comp = USFCombatSubsystem::FindTargetComponent(Candidate);
		if (!IsValid(Comp)) continue;

		float Score = FVector::Dist2D(Origin, Candidate->GetActorLocation());

		// 분류 가중치. 지정되지 않은 분류는 1.0 (= 거리만 본다).
		if (const float* Weight = TargetTypeWeights.Find(Comp->GetTargetType()))
		{
			Score *= *Weight;
		}

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

		// 현재 타겟에 가산점. 난전에서 타겟이 매번 뒤바뀌는 것을 막는다.
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

	return Best;
}

// ─────────────────────────────────────────────────────────
// 공격 실행
// ─────────────────────────────────────────────────────────
void AUnitBase::PerformAttack(AActor* InTarget)
{
	if (!IsTargetUsable(InTarget)) return;

	const float RawDamage = Attack * AttackMultiplier;
	const int32 MyTeam = GetTeamIndex();

	// 최종 피해 = 공격력 x (1 - clamp(방어 - 관통, 0, 0.9))
	// 방어 감산을 공격자가 계산하는 이유는 피해 전달 경로가 값 하나뿐이기 때문이다.
	auto ApplyTo = [this, RawDamage](AActor* InVictim, float InRatio)
	{
		UCombatTargetComponent* Comp = USFCombatSubsystem::FindTargetComponent(InVictim);
		if (!IsValid(Comp) || !Comp->IsAlive()) return;

		const float Effective = FMath::Clamp(Comp->GetArmor() - Penetration, 0.f, 0.9f);
		Comp->ApplyCombatDamage(RawDamage * (1.f - Effective) * InRatio, this);
	};

	ApplyTo(InTarget, 1.f);

	if (SplashRadius > 0.f)
	{
		if (USFCombatSubsystem* Combat = USFCombatSubsystem::Get(this))
		{
			TArray<AActor*> Splashed;
			Combat->GetHostilesInRadius(InTarget->GetActorLocation(), MyTeam, SplashRadius, Splashed);

			for (AActor* Other : Splashed)
			{
				if (Other == InTarget) continue;		// 주 대상은 이미 맞았다

				// 앞선 피해로 이미 파괴됐을 수 있으므로 매번 다시 확인한다
				if (!IsValid(Other)) continue;

				ApplyTo(Other, SplashDamageRatio);
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

	// 조용히 빠져나가면 "왜 안 움직이지" 의 원인을 알 수 없다.
	// AutoPossessAI 가 동작하지 않으면 여기 걸린다.
	if (!AI)
	{
		if (!bNoTargetReported)
		{
			bNoTargetReported = true;
			SF_LOG(Error, "[Unit] %s : AIController 가 없습니다. AutoPossessAI 설정을 확인하세요.", *GetName());
		}
		return;
	}

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
		const int32 MyTeam = GetTeamIndex();

		// 1순위 — 적 핵심 건물 (거리 제한 없음)
		if (AActor* Core = Combat->FindNearestHostile(Origin, MyTeam, ETargetType::CoreBuilding, 0.f))
		{
			if (!bAdvanceTargetReported)
			{
				bAdvanceTargetReported = true;
				SF_LOG(Warning, "[Unit] %s : 진격 목표 = %s (거리 %.0f)",
					*GetName(), *Core->GetName(), FVector::Dist2D(Origin, Core->GetActorLocation()));
			}

			AI->RequestMoveToActor(Core, MoveAcceptanceRadius);
			bNoTargetReported = false;
			return;
		}

		// 2순위 — 핵심 건물이 없는 맵(테스트 레벨 등)에서는
		//         교전 반경 밖이라도 가장 가까운 적을 향해 전진한다.
		if (AActor* Nearest = Combat->FindNearestHostileAny(Origin, MyTeam))
		{
			if (!bAdvanceTargetReported)
			{
				bAdvanceTargetReported = true;
				SF_LOG(Warning, "[Unit] %s : 핵심 건물 없음. 대체 목표 = %s", *GetName(), *Nearest->GetName());
			}

			AI->RequestMoveToActor(Nearest, MoveAcceptanceRadius);
			bNoTargetReported = false;
			return;
		}
	}

	// 여기까지 왔다면 맵에 살아있는 적대 대상이 하나도 없다.
	//
	// 유닛이 굳은 것처럼 보이지만 정상 동작이다.
	// 다만 원인을 알 수 없으면 버그로 오인하기 쉬우므로 한 번은 알린다.
	if (!bNoTargetReported)
	{
		bNoTargetReported = true;

		SF_LOG(Warning,
			"[Unit] %s (Team %d) : 살아있는 적대 대상이 없어 정지합니다. 적 팀에 CombatTarget 컴포넌트를 가진 액터가 남아 있는지 확인하세요.",
			*GetName(), GetTeamIndex());
	}

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
// 디버그 표시
//
// Tick 의 어느 분기에서 빠져나가든 반드시 그려져야 한다.
// 예전에는 Tick 맨 아래에 있어서, 타겟이 없어 조기 return 하는
// Advancing 상태에서는 아무것도 표시되지 않았다.
// ─────────────────────────────────────────────────────────
void AUnitBase::DrawCombatDebug() const
{
#if ENABLE_DRAW_DEBUG
	if (!ShouldDrawDebug()) return;

	const UWorld* World = GetWorld();
	if (!IsValid(World)) return;

	DrawDebugCircle(World, GetActorLocation(), AggroRadius, 32, FColor::Yellow, false, -1.f, 0, 3.f,
		FVector(1, 0, 0), FVector(0, 1, 0), false);

	if (CurrentTarget)
	{
		const FColor LineColor = (CombatState == EUnitCombatState::Attacking) ? FColor::Red : FColor::Orange;
		DrawDebugLine(World, GetActorLocation(), CurrentTarget->GetActorLocation(), LineColor, false, -1.f, 0, 4.f);
	}

	const TCHAR* StateText =
		  CombatState == EUnitCombatState::Advancing ? TEXT("전진")
		: CombatState == EUnitCombatState::Chasing   ? TEXT("추격")
		: CombatState == EUnitCombatState::Attacking ? TEXT("교전")
		:                                              TEXT("사망");

	// RowName 이 None 이면 DT_Units 조회에 실패한 것이다.
	// 능력치가 BP 기본값으로 남아 있다는 뜻이라 함께 표시한다.
	const FString RowText = UnitRowName.IsNone() ? TEXT("RowName없음") : UnitRowName.ToString();

	const FString Info = FString::Printf(
		TEXT("Team%d | %s | %s | 대상:%s | 거리:%.0f | 속도:%.0f | HP:%.0f"),
		GetTeamIndex(),
		StateText,
		*RowText,
		CurrentTarget ? *CurrentTarget->GetName() : (bNoTargetReported ? TEXT("적 전멸") : TEXT("없음")),
		CurrentTarget ? GetDistanceToTargetEdge(CurrentTarget) : -1.f,
		GetVelocity().Size2D(),
		HealthComponent ? HealthComponent->GetCurrentHealth() : 0.f);

	DrawDebugString(World, FVector(0.f, 0.f, 140.f), Info, const_cast<AUnitBase*>(this), FColor::White, 0.f, true);
#endif
}

// ─────────────────────────────────────────────────────────
// 메인 루프
// ─────────────────────────────────────────────────────────
void AUnitBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 어느 분기로 빠지든 표시되도록 맨 앞에서 그린다
	DrawCombatDebug();

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

	bNoTargetReported = false;

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

}
