#include "UnitBase.h"

#include "UnitAIController.h"
#include "ActorComponent/Stat/HealthComponent.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "ActorComponent/Buff/SFBuffComponent.h"
#include "Game/Subsystem/SFCombatSubsystem.h"
#include "Game/Subsystem/SFDataTableManager.h"
#include "InGame/SFSelectINfo/SFSelectionUIDataRow.h"
#include "Units/Lane/LanePath.h"
#include "InGame/HUD/Tutorial/SFTutorialSubsystem.h"

#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "Game/GameState/SFGameState.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "EngineUtils.h"
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

/**
 * 이동 명령 응답 소리의 최소 간격(초).
 *
 * 여러 기를 선택해 한 번에 움직이면 명령이 유닛 수만큼 들어와 소리가 그만큼
 * 겹친다. 이 간격 안에 들어온 두 번째 이후를 버려서 한 기만 대답하게 한다.
 *
 *   sf.CommandVoiceInterval 0.2   기본. 한 기만 대답
 *   sf.CommandVoiceInterval 0     제한 없음. 선택한 전원이 대답
 */
static TAutoConsoleVariable<float> CVarCommandVoiceInterval(
	TEXT("sf.CommandVoiceInterval"),
	0.2f,
	TEXT("이동 명령 응답 소리의 최소 간격(초). 0 이하면 선택한 전원이 대답한다."),
	ECVF_Cheat);

namespace
{
	/**
	 * 마지막으로 명령 응답 소리를 낸 월드와 시각.
	 *
	 * 월드를 함께 들고 있는 이유는 PIE 를 껐다 켜면 월드가 바뀌기 때문이다.
	 * 새 세션의 첫 명령이 직전 세션 기록에 막히면 안 된다.
	 *
	 * 시각은 월드 시간이 아니라 실시간을 쓴다. 월드 시간은 새 세션에서 0 으로
	 * 돌아가 직전 기록보다 작아지고, 그러면 간격 계산이 음수가 된다.
	 */
	TWeakObjectPtr<UWorld> GLastCommandVoiceWorld;
	double GLastCommandVoiceTime = 0.0;
}

AUnitBase::AUnitBase()
	: bRequireLineOfSight(true)
	, bDrawCombatDebug(false)
	, bStatInitialized(false)
	, bNoTargetReported(false)
	, bAdvanceTargetReported(false)
	, bWaitingForGroupMoveRelease(false)
	, bShowCommandPath(false)
	, bHasCommandedDestination(false)
	, bCommandIsAttackMove(false)
	, bHoldGround(false)
	, bRangeUnreachableReported(false)
	, bCommandRejectedReported(false)
	, bPopulationApplied(false)
	, bAdvanceModeInjected(false)
	, bBlockOtherUnits(true)
	, bRequireLineOfSightForBuildings(false)
	, bSightBlockedReported(false)
	, bReturnToHoldLocation(false)
	, bHoldLocationSet(false)
	, bReturningToHold(false)
	, bHoldReported(false)
{
	PrimaryActorTick.bCanEverTick = true;

	// 1번 팀만 스스로 진격한다. 0번(플레이어) 팀은 명령을 기다린다.
	// 팀 번호를 코드가 직접 비교하지 않고 목록 조회로만 쓰기 위한 기본값이다.
	AutoAdvanceTeams.Add(1);

	// 1 프로토 유닛 = 150cm 기준. 일반 유닛 신장 약 180cm
	GetCapsuleComponent()->InitCapsuleSize(45.f, 90.f);

	// 유닛끼리 통과시킬지는 BeginPlay 의 ApplyUnitCollisionMode 에서 정한다.
	// 생성자에서 확정하지 않는 이유는, BP 값과 콘솔 전환을 같은 경로로
	// 처리해야 런타임 A/B 가 가능하기 때문이다.

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

	BuffComponent = CreateDefaultSubobject<USFBuffComponent>(TEXT("BuffComponent"));

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


	if (UCapsuleComponent* Capsule =
		GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(
			ECC_Visibility,
			ECR_Block
		);
	}

	if (USkeletalMeshComponent* CharacterMesh =
		GetMesh())
	{
		CharacterMesh->SetCollisionResponseToChannel(
			ECC_Visibility,
			ECR_Ignore
		);
	}

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

	// 생산 시점이 아니라 BeginPlay 에서 올린다.
	// 스포너·생산 건물·레벨 직접 배치 어느 경로로 생겨도 동일하게 반영된다.
	if (!bPopulationApplied)
	{
		bPopulationApplied = true;
		ApplyPopulationDelta(+PopulationCost);
	}

	if (IsValid(CombatTarget))
	{
		CombatTarget->OnCombatDamaged.AddUniqueDynamic(this, &AUnitBase::HandleCombatDamaged);
	}

	// 유닛끼리 서로를 막을지 반영한다.
	ApplyUnitCollisionMode();

	// 진격 여부를 팀 번호로 정한다.
	// 스폰 쪽에서 SetAdvanceMode 로 이미 주입했다면 그 값을 존중한다.
	ResolveAdvanceModeFromTeam();

	// 스폰된 자리가 기본 대기 지점이다.
	// 제자리 유지 모드에서 교전에 끌려 나가면 여기로 돌아온다.
	SetHoldLocationToCurrent();

	RetargetTimer = 0.f;	// 첫 틱에 바로 탐색이 돌게 한다
}

void AUnitBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 사망을 거치지 않고 사라지는 경로(강제 파괴 등)에서도 인구를 되돌린다.
	// HandleDeath 를 이미 탔으면 플래그가 꺼져 있어 중복되지 않는다.
	//
	// 레벨 전환 시에는 GameState 도 함께 파괴되므로 굳이 되돌릴 필요가 없고,
	// 오히려 파괴 중인 객체를 건드리게 되므로 제외한다.
	if (bPopulationApplied && EndPlayReason != EEndPlayReason::LevelTransition
		&& EndPlayReason != EEndPlayReason::EndPlayInEditor
		&& EndPlayReason != EEndPlayReason::Quit)
	{
		bPopulationApplied = false;
		ApplyPopulationDelta(-PopulationCost);
	}

	if (IsValid(CombatTarget))
	{
		CombatTarget->OnCombatDamaged.RemoveDynamic(this, &AUnitBase::HandleCombatDamaged);
	}

	// 잡아둔 공격 슬롯을 놓는다.
	// 남겨두면 사라진 유닛이 자리를 차지해 다른 유닛이 못 들어간다.
	ReleaseAttackSlot();

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
	PopulationCost	= InRow.PopulationCost;

	if (HealthComponent)
	{
		HealthComponent->InitializeHealth(InRow.MaxHealth);
	}

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = InRow.MoveSpeed;
	}

	// 선택음은 여기서 한 번만 로드한다.
	// 지정하지 않은 유닛은 nullptr 로 남고, 그러면 선택해도 소리가 나지 않는다.
	CachedSelectionSound = InRow.SelectionSound.LoadSynchronous();

	// 이동 명령 응답음도 같은 이유로 여기서 한 번만 로드한다.
	CachedMoveCommandSound = InRow.MoveCommandSound.LoadSynchronous();

	bStatInitialized = true;

	NotifyUnitStatsChanged();
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

// ─────────────────────────────────────────────────────────
// 자동 진격 여부
//
// 팀 분기 대신 주입으로 처리한다.
// 아군·적군이 같은 클래스와 같은 BP 를 쓰므로, 코드가 "이 유닛이 아군인가" 를
// 물을 수 있는 지점이 없다. TeamIndex 를 스폰 시점에 넣어주는 것과 같은 방식으로
// 이 값도 스폰하는 쪽이 넣어준다.
// ─────────────────────────────────────────────────────────
void AUnitBase::ResolveAdvanceModeFromTeam()
{
	// 직접 주입받았으면 손대지 않는다.
	if (bAdvanceModeInjected) return;

	// 팀 번호를 코드가 직접 비교하지 않는다. 목록에 있는지만 본다.
	// 진영 분기 없이 "이 팀은 스스로 진격한다" 를 데이터로 표현하는 방식이다.
	AdvanceMode = AutoAdvanceTeams.Contains(GetTeamIndex())
		? EUnitAdvanceMode::PushToCore
		: EUnitAdvanceMode::HoldPosition;

	bHoldReported = false;
}

void AUnitBase::SetAdvanceMode(EUnitAdvanceMode InMode)
{
	// 직접 주입은 팀 기준 판정보다 우선한다.
	bAdvanceModeInjected = true;

	AdvanceMode = InMode;

	// 모드가 바뀌면 지금 자리를 기준으로 다시 잡는다.
	SetHoldLocationToCurrent();

	bReturningToHold = false;
	bHoldReported = false;
}

void AUnitBase::SetHoldLocationToCurrent()
{
	HoldLocation = GetActorLocation();
	bHoldLocationSet = true;
	bReturningToHold = false;
	bHoldReported = false;
}

int32 AUnitBase::GetTeamIndex() const
{
	return CombatTarget ? CombatTarget->GetTeamIndex() : Targetable::NeutralTeam;
}

float AUnitBase::GetGroundSpeed() const
{
	// Z 를 빼는 이유: 낙하 중에도 Walk 가 재생되지 않게 한다.
	return GetVelocity().Size2D();
}

float AUnitBase::GetCurrentAttackPower() const
{
	return Attack * AttackMultiplier;
}

float AUnitBase::GetCurrentMoveSpeed() const
{
	const UCharacterMovementComponent* Move =
		GetCharacterMovement();

	return Move
		? Move->MaxWalkSpeed
		: 0.f;
}

void AUnitBase::NotifyUnitStatsChanged()
{
	const float NewAttackPower =
		GetCurrentAttackPower();

	const float NewMoveSpeed =
		GetCurrentMoveSpeed();

	if (
		FMath::IsNearlyEqual(
			LastNotifiedAttackPower,
			NewAttackPower
		) &&
		FMath::IsNearlyEqual(
			LastNotifiedMoveSpeed,
			NewMoveSpeed
		)
		)
	{
		return;
	}

	LastNotifiedAttackPower =
		NewAttackPower;

	LastNotifiedMoveSpeed =
		NewMoveSpeed;

	OnUnitStatsChanged.Broadcast(
		NewAttackPower,
		NewMoveSpeed
	);
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
	ForcedTarget = nullptr;
	CommandQueue.Reset();
	bShowCommandPath = false;
	OnCommandPathChanged.Broadcast();
	bHasCommandedDestination = false;
	bCommandIsAttackMove = false;
	ReleaseAttackSlot();
	SetCombatState(EUnitCombatState::Dead);

	// 인구 반환. EndPlay 가 아니라 여기서 하는 이유는
	// 사망 연출 동안(DeathLifeSpan) 인구를 계속 차지하면
	// "죽었는데 왜 못 뽑지" 가 되기 때문이다.
	if (bPopulationApplied)
	{
		bPopulationApplied = false;
		ApplyPopulationDelta(-PopulationCost);
	}

	SpawnEffectAt(DeathEffect, GetActorLocation() + FVector(0.f, 0.f, EffectHeightOffset));

	// 자신이 죽는 경우에는 보장 시간과 무관하게 즉시 끊는다.
	// Tick 이 Dead 에서 조기 return 하므로 여기서 처리하지 않으면
	// 시체가 사망 모션 아래에서 계속 칼을 휘두른다.
	AttackAnimHoldRemaining = 0.f;
	StopAttackMontage();

	PendingAttackTarget.Reset();
	AppliedAttackHitCount = 0;

	// 쿨다운도 함께 초기화한다.
	AttackCooldownRemaining = 0.f;

	// 사망 모션. DeathLifeSpan 안에 끝나야 중간에 사라지지 않는다.
	if (DeathMontage)
	{
		PlayAnimMontage(DeathMontage);
	}

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

void AUnitBase::HandleDeathPose()
{
	// 살아 있는 유닛에게 잘못 실행된 Notify는 무시한다.
	if (CombatState != EUnitCombatState::Dead)
	{
		return;
	}

	// Notify가 실행된 현재 사망 자세에서 애니메이션을 정지한다.
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->bPauseAnims = true;
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
	Info.PortraitAnimation = PortraitIdleAnimation;

	// ── 런타임 값 ───────────────────────────────────────
	if (IsValid(HealthComponent))
	{
		Info.CurrentHealth = HealthComponent->GetCurrentHealth();
		Info.MaximumHealth = HealthComponent->GetMaxHealth();
	}

	// 지휘관 버프 등이 적용된 실제 공격력을 보여준다 성윤 선택정보도 Getter를 쓰도록 변경
	Info.AttackPower = GetCurrentAttackPower();
	Info.MoveSpeed = GetCurrentMoveSpeed();

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

// const FHealthData AUnitBase::GetHealthData_Implementation()
// {
// 	FHealthData Data;
// 	Data.DisplayName = FText::FromString(TEXT("NeedSetName"));
// 	Data.CurrentTier = -1;
// 	Data.HealthComponent = HealthComponent;
// 	return Data;
// }

USoundBase* AUnitBase::GetSelectionSound_Implementation()
{
	// InitStat 이 미리 로드해 둔 것을 그대로 돌려준다.
	// 비어 있으면 호출부(ASFInGamePlayerController::PlaySelectionSound)가
	// 조용히 넘어간다 — 소리를 지정하지 않은 유닛은 무음이 정상이다.
	return CachedSelectionSound;
}

// ─────────────────────────────────────────────────────────
// IMovable — 플레이어 이동 명령
//
// 명령은 "진격 목표 교체" 다. 전투를 멈추지 않는다.
// 가는 길에 적을 만나면 Tick 의 교전 분기가 먼저 걸리고,
// 정리되면 Advance() 로 돌아와 목적지를 향한다.
// ─────────────────────────────────────────────────────────
void AUnitBase::MoveToLocation_Implementation(const FVector& InDestination)
{
	// 여기가 플레이어 이동 명령의 진짜 진입점이다.
	//
	// 컨트롤러는 IMovable 인터페이스로 이 함수를 직접 부른다. CommandMoveTo 를
	// 거치지 않으므로, 그쪽에만 검사를 걸어두면 상대 유닛이 그대로 명령을 받는다.
	if (!IsCommandableByPlayer())
	{
		if (!bCommandRejectedReported)
		{
			bCommandRejectedReported = true;

			SF_LOG(Log,
				"[Unit] %s (Team %d) : 플레이어 명령을 받지 않는 유닛입니다. 이동 명령을 무시합니다. "
				"의도와 다르면 AutoAdvanceTeams 목록과 SetAdvanceMode 주입을 확인하세요.",
				*GetName(), GetTeamIndex());
		}

		return;
	}

	CommandedDestination = InDestination;
	bHasCommandedDestination = true;
	CommandStallTimer = 0.f;

	// 이 함수는 강제 이동이다. 공격 이동은 CommandAttackMove 가
	// 이 함수를 부른 뒤 플래그를 다시 켠다.
	bCommandIsAttackMove = false;
	ForcedTarget = nullptr;

	// 플레이어 명령은 목줄과 제자리 고수보다 우선한다.
	bHoldGround = false;
	bReturningToHold = false;
	bHoldReported = false;

	// 현재 타겟을 놓아 즉시 새 목적지로 향하게 한다.
	// 놓지 않으면 사거리 안의 적을 계속 때리느라 명령이 무시된다.
	CurrentTarget = nullptr;

	// 남아 있는 공격 모션을 끊는다.
	// 그대로 두면 칼을 휘두르면서 이동하는 그림이 된다.
	// 플레이어 명령은 아래 보장 시간보다 우선한다.
	AttackAnimHoldRemaining = 0.f;
	StopAttackMontage();

	// 쿨다운도 함께 초기화한다.
	// 남겨두면 도착 직후 첫 공격이 지연된다.
	AttackCooldownRemaining = 0.f;

	if (AUnitAIController* AI = GetUnitAI())
	{
		AI->ForceRepath();
	}

	// 다음 진격 로그가 다시 찍히도록 초기화
	bAdvanceTargetReported = false;
	bNoTargetReported = false;

	OnCommandPathChanged.Broadcast();

	SF_LOG(Log, "[Unit] %s : 이동 명령 수신 (%s)", *GetName(), *InDestination.ToString());
}

void AUnitBase::StopMove_Implementation()
{
	// 이동 명령과 같은 경로로 들어온다. 같은 기준으로 막는다.
	if (!IsCommandableByPlayer()) return;

	bHasCommandedDestination = false;
	bCommandIsAttackMove = false;
	bHoldGround = false;
	ForcedTarget = nullptr;

	// 정지 명령에도 공격 모션이 남아 있으면 어색하다
	AttackAnimHoldRemaining = 0.f;
	StopAttackMontage();

	// 세운 자리가 새 대기 지점이 된다.
	// 안 그러면 정지시켜 둔 유닛이 슬금슬금 원래 자리로 돌아간다.
	SetHoldLocationToCurrent();

	if (CommandQueue.IsEmpty())
	{
		bShowCommandPath = false;
		OnCommandPathChanged.Broadcast();
	}

	if (AUnitAIController* AI = GetUnitAI())
	{
		AI->ClearMoveRequest();
	}

	SF_LOG(Log, "[Unit] %s : 이동 명령 해제", *GetName());
}

// ─────────────────────────────────────────────────────────
// 전투 명령 — Input Action 이 붙는 지점
//
// PerformAttack 을 직접 부르면 안 된다.
// 쿨다운·사거리·시야 판정을 전부 건너뛰고 즉시 피해가 들어간다.
// 입력은 반드시 아래 세 함수를 거친다.
// ─────────────────────────────────────────────────────────
bool AUnitBase::IsCommandableByPlayer() const
{
	// 자동 진격 팀(AutoAdvanceTeams)은 스스로 움직인다.
	// 드래그 선택에 상대 유닛까지 같이 담겨 명령이 넘어와도 여기서 막는다.
	if (AutoAdvanceTeams.Contains(GetTeamIndex())) return false;

	// 스포너가 SetAdvanceMode 로 진격을 직접 주입한 유닛도 마찬가지다.
	// 팀 목록에 없더라도 스스로 진격하는 쪽이면 플레이어 명령 대상이 아니다.
	if (bAdvanceModeInjected && AdvanceMode == EUnitAdvanceMode::PushToCore) return false;

	return true;
}

// ─────────────────────────────────────────────────────────
// 이동 명령 응답음
//
// 한 번의 우클릭이 선택한 유닛 수만큼 명령을 낸다. 전원이 대답하면 같은
// 소리가 겹쳐 뭉개지므로, 짧은 간격 안의 중복을 버려 한 기만 통과시킨다.
// 같은 프레임에 들어오는 명령들이라 "첫 한 기" 를 고르는 것과 같다.
//
// 어느 유닛이 대답할지는 정하지 않는다. 선택 순서가 곧 호출 순서이므로
// 대체로 먼저 뽑은 유닛이 대답한다.
// ─────────────────────────────────────────────────────────
void AUnitBase::PlayMoveCommandVoice()
{
	// 소리를 지정하지 않은 유닛은 조용한 것이 정상이다.
	if (!IsValid(CachedMoveCommandSound)) return;

	UWorld* World = GetWorld();
	if (!World) return;

	const float Interval = CVarCommandVoiceInterval.GetValueOnGameThread();

	if (Interval > 0.f)
	{
		const double Now = FPlatformTime::Seconds();

		if (GLastCommandVoiceWorld.Get() == World &&
			(Now - GLastCommandVoiceTime) < static_cast<double>(Interval))
		{
			return;
		}

		GLastCommandVoiceWorld = World;
		GLastCommandVoiceTime = Now;
	}

	// 선택음(ASFInGamePlayerController::PlaySelectionSound)과 같은 방식으로 낸다.
	// 위치 기반이므로 감쇠(Attenuation)가 걸린 에셋은 카메라 높이에서 작게 들린다.
	UGameplayStatics::PlaySoundAtLocation(this, CachedMoveCommandSound, GetActorLocation());
}

bool AUnitBase::HasActivePlayerCommand() const
{
	return bHasCommandedDestination || IsValid(ForcedTarget);
}

void AUnitBase::GetCommandPathLocations(
	TArray<FVector>& OutLocations
) const
{
	OutLocations.Reset();

	if (!bShowCommandPath || CombatState == EUnitCombatState::Dead)
	{
		return;
	}

	if (bHasCommandedDestination)
	{
		OutLocations.Add(CommandedDestination);
	}
	else if (IsValid(ForcedTarget))
	{
		OutLocations.Add(
			ForcedTarget->GetActorLocation()
		);
	}

	for (const FSFQueuedUnitCommand& QueuedCommand : CommandQueue)
	{
		if (QueuedCommand.Command == ESFUnitCommand::Move || QueuedCommand.Command == ESFUnitCommand::AttackMove)
		{
			OutLocations.Add(
				QueuedCommand.Location
			);
		}

		else if (QueuedCommand.Command == ESFUnitCommand::Attack)
		{
			if (QueuedCommand.bTargetRequired &&
				!IsValid(QueuedCommand.Target.Get()))
			{
				continue;
			}

			OutLocations.Add(
				IsValid(QueuedCommand.Target.Get())
				? QueuedCommand.Target->GetActorLocation()
				: QueuedCommand.Location
			);
		}
	}
}

void AUnitBase::ExecuteCommand(
	ESFUnitCommand InCommand,
	const FVector& InLocation,
	AActor* InTarget
)
{
	ExecuteCommandWithQueue(
		InCommand,
		InLocation,
		InTarget,
		false
	);
}

void AUnitBase::ExecuteCommandWithQueue(
	ESFUnitCommand InCommand,
	const FVector& InLocation,
	AActor* InTarget,
	bool bAppendToQueue
)
{
	if (!IsCommandableByPlayer())
	{
		return;
	}

	const bool bHasCurrentCommand =
		HasActivePlayerCommand() ||
		CommandQueue.Num() > 0;

	if (bAppendToQueue && bHasCurrentCommand)
	{
		FSFQueuedUnitCommand& QueuedCommand =
			CommandQueue.Emplace_GetRef();

		QueuedCommand.Command = InCommand;
		QueuedCommand.Location = InLocation;
		QueuedCommand.Target = InTarget;
		QueuedCommand.bTargetRequired =
			InCommand == ESFUnitCommand::Attack &&
			IsValid(InTarget);

		bShowCommandPath = true;

		OnCommandPathChanged.Broadcast();

		SF_LOG(
			Log,
			"[Unit] %s : 명령 예약 | Command: %d | QueueCount: %d",
			*GetName(),
			static_cast<int32>(InCommand),
			CommandQueue.Num()
		);

		return;
	}

	// 일반 명령은 기존 예약을 모두 지운다. 성윤 Shift 명령은 예약을 유지한다. 예약이 있으면 즉시 실행하지 않고 큐에 넣는다.
	if (!bAppendToQueue)
	{
		CommandQueue.Reset();

		bShowCommandPath = false;
		OnCommandPathChanged.Broadcast();
	}

	else
	{
		// 현재 명령이 없어도 첫 Shift 명령은 표시
		bShowCommandPath = true;
	}

	// 현재 명령이 없으면 Shift 명령도 즉시 실행한다.
	ExecuteCommandImmediately(
		InCommand,
		InLocation,
		InTarget
	);
}

void AUnitBase::ExecuteCommandImmediately(
	ESFUnitCommand InCommand,
	const FVector& InLocation,
	AActor* InTarget
)
{
	switch (InCommand)
	{
	case ESFUnitCommand::Move:
		CommandMoveTo(InLocation);
		break;

	case ESFUnitCommand::AttackMove:
		CommandAttackMove(InLocation);
		break;

	case ESFUnitCommand::Attack:
		if (IsValid(InTarget))
		{
			CommandAttackTarget(InTarget);
		}
		else
		{
			// 땅을 클릭한 A 명령은 공격 이동으로 처리
			CommandAttackMove(InLocation);
		}
		break;

	case ESFUnitCommand::Stop:
		CommandStop();
		break;

	case ESFUnitCommand::HoldGround:
		CommandHoldGround();
		break;

	default:
		SF_LOG(
			Warning,
			"[Unit] %s : 처리하지 않은 명령입니다 (%d)",
			*GetName(),
			static_cast<int32>(InCommand)
		);
		break;
	}
}

bool AUnitBase::ExecuteNextQueuedCommand()
{
	while (CommandQueue.Num() > 0)
	{
		const FSFQueuedUnitCommand NextCommand =
			CommandQueue[0];

		CommandQueue.RemoveAt(0);

		// 예약해둔 공격 대상이 이미 죽었다면 건너뛴다.
		if (
			NextCommand.bTargetRequired &&
			!IsTargetUsable(NextCommand.Target.Get())
			)
		{
			SF_LOG(
				Log,
				"[Unit] %s : 이미 사라진 예약 공격 대상을 건너뜁니다.",
				*GetName()
			);

			continue;
		}

		ExecuteCommandImmediately(
			NextCommand.Command,
			NextCommand.Location,
			NextCommand.Target.Get()
		);

		SF_LOG(
			Log,
			"[Unit] %s : 다음 예약 명령 실행 | 남은 큐: %d",
			*GetName(),
			CommandQueue.Num()
		);

		return true;
	}

	bShowCommandPath = false;
	OnCommandPathChanged.Broadcast();

	return false;
}
void AUnitBase::CommandMoveTo(const FVector& InDestination)
{
	if (!IsCommandableByPlayer()) return;

	// IMovable 인터페이스와 같은 동작이다.
	// 컨트롤러가 모든 명령을 Command 계열 이름으로 통일해 부를 수 있게 둔다.
	MoveToLocation_Implementation(InDestination);

	PlayMoveCommandVoice();
}

void AUnitBase::CommandAttackMove(const FVector& InDestination)
{
	if (!IsCommandableByPlayer()) return;

	// 목적지 설정·몽타주 중단·경로 재계산은 강제 이동과 완전히 같다.
	// 다른 점은 Tick 에서 교전 판정을 건너뛰지 않는다는 것뿐이다.
	MoveToLocation_Implementation(InDestination);

	// 공격을 취소하고 잠시 목적지 이동을 우선
	CommandAttackMoveGraceRemaining = 0.3f;

	// 위 호출이 꺼놓은 플래그를 다시 켠다. 순서를 뒤집으면 꺼진다.
	bCommandIsAttackMove = true;

	PlayMoveCommandVoice();

	SF_LOG(Log, "[Unit] %s : 공격 이동 명령 (%s)", *GetName(), *InDestination.ToString());
}

void AUnitBase::CommandAttackTarget(AActor* InTarget)
{
	if (!IsCommandableByPlayer()) return;

	// 조용히 빠져나가면 "공격키를 눌렀는데 아무 일도 없다" 가 된다.
	if (!IsValid(InTarget))
	{
		SF_LOG(Warning, "[Unit] %s : 강제 공격 대상이 유효하지 않습니다.", *GetName());
		return;
	}

	if (!IsTargetUsable(InTarget))
	{
		SF_LOG(Warning,
			"[Unit] %s : %s 은(는) 공격할 수 없는 대상입니다. "
			"CombatTarget 컴포넌트 유무와 TeamIndex 를 확인하세요. (sf.DumpCombatTargets)",
			*GetName(), *InTarget->GetName());
		return;
	}

	// 강제 공격은 이동 명령과 제자리 고수를 덮어쓴다. 대상에게 붙는 것이 목적이다.
	bHasCommandedDestination = false;
	bCommandIsAttackMove = false;
	bHoldGround = false;

	ForcedTarget = InTarget;
	CurrentTarget = InTarget;

	// 다음 틱에 재탐색이 돌아 이 지정을 덮지 않도록 주기를 되감는다.
	RetargetTimer = RetargetInterval;

	if (AUnitAIController* AI = GetUnitAI())
	{
		AI->ForceRepath();
	}

	bAdvanceTargetReported = false;
	bNoTargetReported = false;

	OnCommandPathChanged.Broadcast();

	SF_LOG(Log, "[Unit] %s : 강제 공격 명령 — 대상 %s", *GetName(), *InTarget->GetName());
}

void AUnitBase::CommandStop()
{
	if (!IsCommandableByPlayer()) return;

	CommandQueue.Reset();

	// StopMove 가 ForcedTarget·공격 이동·제자리 고수를 함께 정리한다.
	StopMove_Implementation();

	// 들고 있던 타겟도 놓는다. 다음 틱에 평소대로 다시 찾는다.
	CurrentTarget = nullptr;
	RetargetTimer = 0.f;
}

void AUnitBase::CommandHoldGround()
{
	if (!IsCommandableByPlayer()) return;

	// 이동에 관한 모든 것을 놓는다. 대상은 놓지 않는다 —
	// 사거리 안에서 이미 때리는 중이라면 그대로 이어서 때려야 한다.
	bHasCommandedDestination = false;
	bCommandIsAttackMove = false;
	ForcedTarget = nullptr;

	CommandQueue.Reset();

	bShowCommandPath = false;
	OnCommandPathChanged.Broadcast();

	bHoldGround = true;

	// 선 자리가 곧 지킬 자리다.
	SetHoldLocationToCurrent();

	// 공격 몽타주는 끊지 않는다. 고수는 교전을 멈추라는 명령이 아니다.
	if (AUnitAIController* AI = GetUnitAI())
	{
		AI->ClearMoveRequest();
	}

	SF_LOG(Log, "[Unit] %s : 제자리 고수 — 사거리 안의 적만 공격합니다.", *GetName());
}

// ─────────────────────────────────────────────────────────
// 인구수
//
// 생산 시 +, 사망 시 - 한 번씩만 반영한다.
// 같은 프레임에 광역 피해를 여러 번 맞아도 중복 차감되지 않게
// bPopulationApplied 로 막는다.
// ─────────────────────────────────────────────────────────
void AUnitBase::ApplyPopulationDelta(int32 InDelta)
{
	if (PopulationCost <= 0 || InDelta == 0) return;

	const UWorld* World = GetWorld();
	ASFGameState* GameState = IsValid(World) ? World->GetGameState<ASFGameState>() : nullptr;

	// 테스트 레벨처럼 GameState 가 없는 경우도 있으므로 조용히 넘어간다
	if (!IsValid(GameState)) return;
	GameState->AddPopulation(GetTeamIndex(), InDelta);
}

void AUnitBase::StopAttackMontage()
{
	if (!AttackMontage) return;

	const USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* Anim = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (!Anim) return;

	// 지금 돌고 있는 게 공격 몽타주일 때만 끊는다.
	// 조건 없이 StopAllMontages 를 부르면 사망 모션까지 끊긴다.
	if (Anim->Montage_IsPlaying(AttackMontage))
	{
		Anim->Montage_Stop(MontageBlendOutTime, AttackMontage);
	}
}

void AUnitBase::SpawnEffectAt(UNiagaraSystem* InSystem, const FVector& InLocation) const
{
	if (!InSystem) return;

	UWorld* World = GetWorld();
	if (!IsValid(World)) return;

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World,
		InSystem,
		InLocation,
		FRotator::ZeroRotator,
		FVector::OneVector,
		true,   // bAutoDestroy — 재생이 끝나면 알아서 정리된다
		true);  // bAutoActivate
}

// ─────────────────────────────────────────────────────────
// 거리 · 시야
// ─────────────────────────────────────────────────────────
bool AUnitBase::IsTargetUsable(const AActor* InTarget) const
{
	if (!CombatTarget || !CombatTarget->IsWithinSight(InTarget)) return false;
	UCombatTargetComponent* Comp = USFCombatSubsystem::FindTargetComponent(InTarget);
	if (!IsValid(Comp) || !Comp->IsAlive()) return false;

	return USFCombatSubsystem::IsHostileTeam(GetTeamIndex(), Comp->GetTeamIndex());
}

void AUnitBase::SetWaitingForGroupMove(bool bWaiting)
{
	bWaitingForGroupMoveRelease = bWaiting;

	if (!bWaiting)
	{
		return;
	}

	// 그룹 이동 중에는 기존 전투를 완전히 해제한다.
	ForcedTarget = nullptr;
	CurrentTarget = nullptr;
	ReleaseAttackSlot();

	AttackAnimHoldRemaining = 0.f;
	StopAttackMontage();
}

bool AUnitBase::IsBuildingTarget(const AActor* InTarget) const
{
	const UCombatTargetComponent* Comp = USFCombatSubsystem::FindTargetComponent(InTarget);
	if (!IsValid(Comp)) return false;

	switch (Comp->GetTargetType())
	{
	case ETargetType::CoreBuilding:
	case ETargetType::ProductionBuilding:
	case ETargetType::ResourceBuilding:
		return true;
	default:
		return false;
	}
}

float AUnitBase::GetBaseRange(const AActor* InTarget) const
{
	// 건물은 자기 주변 NavMesh 를 넓게 파먹는다.
	// Agent 반지름 + 콜리전 크기만큼 접근 한계가 생겨,
	// 유닛이 아무리 붙어도 사거리 안으로 들어가지 못하는 경우가 있다.
	//
	// 그래서 건물 대상에는 접근 비율 대신 여유를 더한다.
	// AttackRange 자체를 늘리면 유닛 간 교전 사거리까지 함께 늘어난다.
	return IsBuildingTarget(InTarget)
		? AttackRange + BuildingRangeBonus
		: AttackRange * AttackApproachRatio;
}

float AUnitBase::GetApproachDistance(const AActor* InTarget) const
{
	const float BaseRange = GetBaseRange(InTarget);

	// 슬롯은 도착해야 하는 자리가 아니라 "그 방향으로 파고들라" 는 지시다.
	// 유닛은 자리에 닿기 전에 사거리에 들어오고, 그 순간 교전 분기가 걸려 멈춘다.
	// 그래서 자리는 사거리 경계 바로 안쪽에 둔다.
	//
	// 더 안쪽으로 밀어 넣으면 링이 대상 캡슐에 달라붙는다.
	// 사거리 105 · 캡슐 반지름 45 기준으로 링 반지름이 90 이 되면
	// 두 캡슐이 맞닿는 거리와 같아진다. DetourCrowd 분리력이 정확히 그 자리를
	// 밀어내므로 유닛은 자리에 닿지 못하고 그 바깥에서 굳는다.
	//
	// 하한은 사거리가 아주 짧은 유닛에서 0 이나 음수가 나오지 않게 하려는 것이다.
	return FMath::Max(BaseRange * 0.3f, BaseRange - SlotRangeMargin);
}

// ─────────────────────────────────────────────────────────
// 공격 슬롯
//
// 전원이 대상의 같은 한 점을 향하면 서로 밀치느라 아무도 사거리에 못 든다.
// 대상 주위 각도를 나눠 갖고 각자 자기 방향에서 접근한다.
//
// 슬롯은 접근 경로만 정한다. 사거리에 들어오는 순간 교전 분기가 걸려
// 그 자리에서 때린다 — 자리를 맞추겠다고 더 움직이지 않는다.
// 그 한 줄이 "공격 안 하고 이동만 하는 유닛" 을 막는다.
// ─────────────────────────────────────────────────────────
void AUnitBase::UpdateAttackSlot()
{
	USFCombatSubsystem* Combat = USFCombatSubsystem::Get(this);
	if (!Combat) return;

	// 대상이 바뀌었으면 이전 자리를 먼저 놓는다.
	if (SlotTarget && SlotTarget != CurrentTarget)
	{
		Combat->ReleaseAttackSlot(SlotTarget, this);

		SlotTarget = nullptr;
		AttackSlotIndex = INDEX_NONE;

		// 새 대상으로 바뀌었으니 이전 대상 기준으로 쌓인 정체 시간도 버린다.
		CloseQuartersStuckTimer = 0.f;
	}

	if (!CurrentTarget) return;

	// 이미 이 대상에 자리를 갖고 있으면 그대로 쓴다.
	// 매 틱 다시 잡으면 대상이 조금 움직일 때마다 자리가 바뀌어
	// 유닛이 링 위를 빙빙 돌기만 한다.
	if (SlotTarget == CurrentTarget && AttackSlotIndex != INDEX_NONE) return;

	AttackSlotIndex = Combat->ClaimAttackSlot(
		CurrentTarget, this, GetApproachDistance(CurrentTarget));

	// 자리를 못 받았으면 SlotTarget 을 비워둔다.
	// 다음 틱에 다시 시도하고, 그동안은 예전처럼 대상에게 직접 접근한다.
	SlotTarget = (AttackSlotIndex != INDEX_NONE) ? CurrentTarget : nullptr;
}

// ─────────────────────────────────────────────────────────
// 근접 조향 — NavMesh 대신 직접 계산
//
// SlotActivationRange 안에서는 AIController 에게 MoveTo 를 넘기지 않는다.
// 슬롯(또는 접근 지점)으로 당기는 힘과 근처 아군에게서 밀려나는 힘을
// 매 틱 더해 CharacterMovement 에 직접 넣는다.
//
// 슬롯 링(각도 배분·정원 제한)은 그대로 쓴다. 여기서 바뀐 것은 "그 자리까지
// 어떻게 가는가" 뿐이다 — 정확히 슬롯에 닿을 필요도 없다. 사거리 판정은
// 대상 표면까지의 거리로 따로 하므로(GetDistanceToTargetEdge), 슬롯이 살짝
// 어긋난 채로 접근하다가도 사거리에 들어오는 순간 위쪽 교전 분기가 멈춰
// 세운다. 그래서 "슬롯에 도달하지 못해 정체" 라는 상태 자체가 없어진다.
// ─────────────────────────────────────────────────────────
void AUnitBase::SteerTowardSlot(float DeltaSeconds)
{
	if (!CurrentTarget) return;

	USFCombatSubsystem* Combat = USFCombatSubsystem::Get(this);
	if (!Combat) return;

	// 1. 추격력 — 슬롯이 있으면 슬롯 위치로, 없으면 대상에서 접근 거리만큼
	//    떨어진 채 지금 서 있는 방향으로 향한다. 슬롯이 아직 없어도 이 방향이
	//    대상 쪽을 가리키므로, 분리력에 밀려도 결국 대상 주변으로 모인다.
	FVector DesiredLocation;
	bool bHaveDesired = false;

	if (AttackSlotIndex != INDEX_NONE)
	{
		bHaveDesired = Combat->GetAttackSlotLocation(
			CurrentTarget, AttackSlotIndex, GetApproachDistance(CurrentTarget), DesiredLocation);
	}

	if (!bHaveDesired)
	{
		// 자리를 못 받았다. 지금 서 있는 방향 그대로 파고든다.
		//
		// 거리는 반드시 대상 "표면" 기준으로 잡아야 한다. 예전에는 중심에서
		// 접근 거리만큼 떨어진 점을 썼는데, 슬롯은 표면 + 접근 거리에 놓이므로
		// 그 점은 앞줄이 서 있는 원보다 대상 반경만큼 안쪽이었다. 자리를 못 받은
		// 유닛이 앞줄 한복판을 목표로 삼고 계속 파고들며 갈리는 원인이었다.
		FVector ToTarget = CurrentTarget->GetActorLocation() - GetActorLocation();
		ToTarget.Z = 0.f;

		const FVector InwardDir = ToTarget.IsNearlyZero()
			? GetActorForwardVector()
			: ToTarget.GetSafeNormal();

		// 표면까지 남은 거리에서 접근 거리를 빼면 "앞으로 더 가야 할 만큼" 이 나온다.
		// 이미 너무 붙었으면 음수가 되어 물러나는 방향이 된다.
		const float StepIn = GetDistanceToTargetEdge(CurrentTarget) - GetApproachDistance(CurrentTarget);

		DesiredLocation = GetActorLocation() + InwardDir * StepIn;
	}

	FVector Seek = DesiredLocation - GetActorLocation();
	Seek.Z = 0.f;
	Seek = Seek.GetSafeNormal();

	// 2. 분리력 — 근처 아군(전투 유닛만, 건물 제외)에게서 밀려난다.
	//    슬롯이 각도를 나눠줘도, 그 자리로 가는 도중에는 여러 유닛이 잠깐
	//    겹칠 수 있다. 그 순간을 이 힘이 풀어준다.
	//
	//    정반대로만 밀면 추격력과 일직선으로 맞부딪힌다 — 앞을 막은 아군을
	//    정면으로 밀고 들어가려는 상황이라 합력이 0에 수렴해 멈춰 선다.
	//    이건 세기(Weight)를 아무리 조절해도 "방향이 정반대"라는 사실 자체는
	//    바뀌지 않아서 해결되지 않는다. 그래서 밀어내는 방향을 항상 같은
	//    쪽으로 SeparationDeflectionAngle 만큼 틀어서 적용한다 — 마주 선 두
	//    유닛이 서로 반대쪽으로 감아 돌며 지나가는 흐름이 생긴다.
	FVector Separation = FVector::ZeroVector;

	TArray<AActor*> Neighbors;
	Combat->GetNearbyAlliesForSeparation(this, GetTeamIndex(), SeparationRadius, Neighbors);

	for (const AActor* Other : Neighbors)
	{
		FVector AwayFromOther = GetActorLocation() - Other->GetActorLocation();
		AwayFromOther.Z = 0.f;

		const float Dist = AwayFromOther.Size();
		if (Dist < KINDA_SMALL_NUMBER) continue;

		const FVector Deflected = AwayFromOther.RotateAngleAxis(SeparationDeflectionAngle, FVector::UpVector);

		// 가까울수록 세게 민다. SeparationRadius 경계에서 0으로 줄어든다.
		Separation += Deflected.GetSafeNormal() * (SeparationRadius - Dist) / SeparationRadius;
	}

	if (!Separation.IsNearlyZero())
	{
		Separation = Separation.GetSafeNormal();
	}

	// 3. 정체 탈출력 — 회전 바이어스로도 못 뚫을 만큼 사방이 막히면(실제
	//    속도가 한동안 거의 0이면) 추격 방향에 수직으로 미는 힘을 더한다.
	//    유닛마다 고정된 쪽(짝/홀)으로 갈라서, 탈출하는 유닛끼리 또
	//    마주치지 않게 한다. 평소에는 0이라 개입하지 않는다.
	FVector StuckEscape = FVector::ZeroVector;

	if (GetVelocity().SizeSquared2D() < FMath::Square(CloseQuartersStuckSpeed))
	{
		CloseQuartersStuckTimer += DeltaSeconds;
	}
	else
	{
		CloseQuartersStuckTimer = 0.f;
	}

	if (CloseQuartersStuckTimer >= CloseQuartersStuckDuration && !Seek.IsNearlyZero())
	{
		const FVector PerpAxis = (Seek ^ FVector::UpVector).GetSafeNormal();
		const bool bDeflectRight = (GetUniqueID() % 2) == 0;

		StuckEscape = (bDeflectRight ? PerpAxis : -PerpAxis) * StuckEscapeWeight;
	}

	// 4. 합성 — 방향이 프레임마다 튀지 않게 살짝 보간한다.
	// 분리력만 있고 추격력이 없는 경우(대상을 잃기 직전 등)는 없다 —
	// 이 함수는 CurrentTarget 이 있을 때만 불린다.
	const FVector RawDirection = (Seek * SeekWeight + Separation * SeparationWeight + StuckEscape).GetSafeNormal();

	SmoothedSteerDirection = FMath::VInterpTo(SmoothedSteerDirection, RawDirection, DeltaSeconds, SteerInterpSpeed);

	if (!SmoothedSteerDirection.IsNearlyZero())
	{
		AddMovementInput(SmoothedSteerDirection, 1.f);
	}
}

// ─────────────────────────────────────────────────────────
// 명령 지점 마지막 접근 — NavMesh 대신 직접 계산
//
// 분리력을 넣지 않는 것이 근접 전투 조향과 다른 점이다. 여기서는 반드시
// 지정한 한 점에 닿아야 하는데, 아군이 밀어내면 도착 반경 안으로 못 들어와
// 명령이 영영 안 풀린다. 대신 못 닿는 경우는 Advance 의 정체 타이머가 받는다.
// ─────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────
// 튜토리얼 — 명령 지점 도착 통지
//
// 명령을 내린 순간이 아니라 실제로 닿았을 때 알린다.
// 클릭만 하고 넘어가면 플레이어가 유닛이 움직이는 것을 못 보고 지나간다.
//
// bRecordWhenNotWaiting 을 false 로 준다. 지금 이 행동을 요구하고 있지
// 않다면 없던 일로 쳐야, 튜토리얼이 시키기 전에 한 번 움직였다는 이유로
// 이동 단계가 통째로 건너뛰어지지 않는다.
//
// 여러 기를 한꺼번에 보내면 도착할 때마다 들어오지만, 첫 신호가 대기를
// 풀고 나면 나머지는 서브시스템에서 조용히 버려진다.
// ─────────────────────────────────────────────────────────
void AUnitBase::NotifyTutorialCommandArrived()
{
	USFTutorialSubsystem::NotifyObjective(this, TAG_SF_Tutorial_Command_Move, false);
}

void AUnitBase::SteerTowardCommandPoint(float DeltaSeconds)
{
	FVector ToDestination = CommandedDestination - GetActorLocation();
	ToDestination.Z = 0.f;

	const FVector Desired = ToDestination.GetSafeNormal();
	if (Desired.IsNearlyZero()) return;

	SmoothedSteerDirection = FMath::VInterpTo(SmoothedSteerDirection, Desired, DeltaSeconds, SteerInterpSpeed);

	if (!SmoothedSteerDirection.IsNearlyZero())
	{
		const float InputStrength =
			FMath::GetMappedRangeValueClamped(
				FVector2D(
					CommandArrivalRadius,
					CommandFinalApproachRange
				),
				FVector2D(
					0.25f,
					1.f
				),
				ToDestination.Size2D()
			);

		AddMovementInput(
			SmoothedSteerDirection,
			InputStrength
		);
	}
}

// ─────────────────────────────────────────────────────────
// 유닛 간 통과
//
// 대상에 붙은 앞줄이 물리적인 벽이 되면, 뒤에서 오는 유닛은 자리가 남아 있어도
// 그리로 갈 수가 없다. 공격 슬롯을 아무리 잘 나눠줘도 도달을 못 하니 소용이 없다.
//
// 통과시키고, 겹치지 않게 벌리는 일은 DetourCrowd 분리력과 슬롯에 맡긴다.
// 슬롯이 각자 다른 각도를 주므로 최종 위치가 갈린다.
// ─────────────────────────────────────────────────────────
void AUnitBase::SetBlockOtherUnits(bool bInBlock)
{
	bBlockOtherUnits = bInBlock ? 1 : 0;
	ApplyUnitCollisionMode();
}

void AUnitBase::ApplyUnitCollisionMode()
{
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!IsValid(Capsule)) return;

	// Overlap 이 아니라 Ignore 다.
	// 겹침 이벤트를 쓰지 않는데 Overlap 으로 두면 밀집 시 유닛 수의 제곱만큼
	// 이벤트가 발생한다.
	//
	// 한쪽만 Ignore 여도 충돌은 성립하지 않으므로, 모든 유닛이 같은 설정이면 통과한다.
	Capsule->SetCollisionResponseToChannel(
		ECC_Pawn, bBlockOtherUnits ? ECR_Block : ECR_Ignore);
}

void AUnitBase::ReleaseAttackSlot()
{
	if (!SlotTarget) return;

	if (USFCombatSubsystem* Combat = USFCombatSubsystem::Get(this))
	{
		Combat->ReleaseAttackSlot(SlotTarget, this);
	}

	SlotTarget = nullptr;
	AttackSlotIndex = INDEX_NONE;
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
	// Artillery/building obstacle exceptions never bypass personal sight range.
	if (!CombatTarget || !CombatTarget->IsWithinSight(InTarget)) return false;
	if (!bRequireLineOfSight) return true;		// 곡사 유닛은 시야를 요구하지 않는다
	if (!IsValid(InTarget)) return false;

	// 건물은 시야를 따지지 않는다.
	//
	// 사거리 판정이 이미 바운딩 박스 표면 기준이라, 사거리 안에 들었다는 것은
	// 사실상 건물에 붙었다는 뜻이다. 그 상태에서 시야를 물을 이유가 없다.
	//
	// 그리고 물으면 문제가 생긴다. 광물 위에 세운 채집 건물처럼 대상 바로 옆에
	// 다른 WorldStatic 액터가 붙어 있으면, 구체 스윕(반지름 40)이 그것에 걸려
	// 시야가 영구히 막힌 것으로 나온다.
	// Params 는 대상 액터만 무시하므로 옆에 붙은 광물은 걸러지지 않는다.
	//
	// 그러면 유닛은 사거리 안에서 건물을 바라보고 서 있기만 하고 피해는
	// 전혀 들어가지 않는다. 로그도 없어 원인을 찾을 수가 없다.
	if (!bRequireLineOfSightForBuildings && IsBuildingTarget(InTarget)) return true;

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
	// 오브젝트 타입이 실제로 WorldStatic인 대상만 검사한다.
	// SweepSingleByChannel(ECC_WorldStatic)을 사용하면
	// WorldStatic 채널을 Block하는 Pawn까지 검사에 걸릴 수 있다.
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);

	const bool bBlocked = GetWorld()->SweepSingleByObjectType(
		Hit,
		Start,
		End,
		FQuat::Identity,
		ObjectQueryParams,
		Probe,
		Params);

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
AActor* AUnitBase::FindBestTarget(const AActor* InExcludedTarget)
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
		if (!IsTargetUsable(Candidate)) continue;
		// 슬롯 접근에 실패한 기존 타겟은 이번 재탐색에서 제외한다.
		if (Candidate == InExcludedTarget)
		{
			continue;
		}

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

		// 자리가 다 찬 대상은 점수를 불린다.
		//
		// 예외는 "내가 그 자리를 갖고 있을 때" 뿐이다.
		// 단순히 현재 타겟이라는 이유로 봐주면, 자리를 못 받은 유닛이
		// 꽉 찬 대상을 계속 붙들고 바깥에서 밀기만 하다가 영영 공격을 못 한다.
		// 그 유닛은 다른 적으로 흩어져야 한다.
		const bool bHoldsSlotHere = (Candidate == SlotTarget) && (AttackSlotIndex != INDEX_NONE);

		if (!bHoldsSlotHere && Combat->IsSlotRingFull(Candidate))
		{
			Score *= OvercrowdedTargetPenalty;
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
	/**
	 * PerformAttack에서는 공격 대상 설정과 몽타주 재생만 처리한다.
	 * 기존의 즉시 피해 계산은 Notify 피해와 중복될 수 있으므로 제거하고,
	 * 실제 피해는 공격 애니메이션의 타격 Notify 시점에 적용한다.
	 */
	if (!IsTargetUsable(InTarget) || !HasLineOfSight(InTarget))
	{
		return;
	}

	// 이번 공격 모션에서 사용할 대상과 타격 횟수를 초기화한다.
	PendingAttackTarget = InTarget;
	AppliedAttackHitCount = 0;

	// 공격 몽타주가 있으면 애니메이션을 먼저 재생한다.
	// 실제 피해는 몽타주의 AttackHit Notify에서 적용한다.
	if (AttackMontage)
	{
		const float MontageDuration = PlayAnimMontage(AttackMontage);

		if (MontageDuration > 0.f)
		{
			AttackAnimHoldRemaining = FMath::Min(MontageDuration, AttackCooldown);

			OnAttackPerformed(InTarget);
			return;
		}
	}

	// 몽타주가 없거나 재생에 실패한 경우의 안전장치.
	// 이 경우에는 즉시 한 번의 전체 피해를 적용한다.
	HandleAttackHit(1.f);

	OnAttackPerformed(InTarget);
}

/** 공격 애니메이션의 타격 Notify에서 호출한다 */
void AUnitBase::HandleAttackHit(float DamageRatio)
{
	// 죽은 유닛의 늦은 Notify가 피해를 주는 것을 방지한다.
	if (CombatState == EUnitCombatState::Dead)
	{
		return;
	}

	// 잘못된 배율은 무시한다.
	if (DamageRatio <= 0.f)
	{
		return;
	}

	// 현재 공격에서 허용된 타격 횟수를 모두 사용했다.
	// Notify 중복 배치로 피해가 여러 번 들어가는 것도 막는다.
	if (AppliedAttackHitCount >= AttackHitCount)
	{
		return;
	}

	AActor* Target = PendingAttackTarget.Get();

	// 공격 애니메이션 도중 대상이 죽거나 제거될 수 있다.
	if (!IsTargetUsable(Target))
	{
		PendingAttackTarget.Reset();
		return;
	}

	++AppliedAttackHitCount;

	ApplyAttackDamage(Target, DamageRatio);

	// 마지막 타격까지 끝났다면 공격 대상 임시 참조를 해제한다.
	if (AppliedAttackHitCount >= AttackHitCount)
	{
		PendingAttackTarget.Reset();
	}
}

/** 공격 Notify가 발생했을 때 실제 피해를 계산하고 적용한다 */
void AUnitBase::ApplyAttackDamage(AActor* InTarget, float DamageRatio)
{
	if (!IsTargetUsable(InTarget) || !HasLineOfSight(InTarget))
	{
		return;
	}

	const float RawDamage = Attack * AttackMultiplier * DamageRatio;

	const int32 MyTeam = GetTeamIndex();

	// 대상의 방어력과 이 유닛의 관통력을 반영해 피해를 적용한다.
	auto ApplyTo = [this, RawDamage](AActor* InVictim, float InSplashRatio)
		{
			UCombatTargetComponent* Comp = USFCombatSubsystem::FindTargetComponent(InVictim);

			if (!IsValid(Comp) || !Comp->IsAlive())
			{
				return;
			}

			const float EffectiveArmor = FMath::Clamp(Comp->GetArmor() - Penetration, 0.f, 0.9f);

			const float FinalDamage = RawDamage * (1.f - EffectiveArmor) * InSplashRatio;

			Comp->ApplyCombatDamage(FinalDamage, this);
		};

	// 주 대상 피해
	ApplyTo(InTarget, 1.f);

	// 광역 피해
	if (SplashRadius > 0.f)
	{
		if (USFCombatSubsystem* Combat = USFCombatSubsystem::Get(this))
		{
			TArray<AActor*> Splashed;

			Combat->GetHostilesInRadius(InTarget->GetActorLocation(), MyTeam, SplashRadius, Splashed);

			for (AActor* Other : Splashed)
			{
				// 주 대상은 위에서 이미 피해를 받았다.
				if (Other == InTarget)
				{
					continue;
				}

				if (!IsValid(Other))
				{
					continue;
				}

				ApplyTo(Other, SplashDamageRatio);
			}
		}
	}

	// 이펙트도 실제 타격 Notify 시점에 출력한다.
	SpawnEffectAt(AttackEffect, InTarget->GetActorLocation() + FVector(0.f, 0.f, EffectHeightOffset));

	// 실제 피격 대상의 종류에 따라 타격 사운드를 선택한다.
	USoundBase* HitSound = IsBuildingTarget(InTarget) ? BuildingHitSound : UnitHitSound;

	if (IsValid(HitSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, HitSound, InTarget->GetActorLocation());
	}
}

// ─────────────────────────────────────────────────────────
// 전진 — 교전 대상이 없을 때
// ─────────────────────────────────────────────────────────
void AUnitBase::Advance(float DeltaSeconds)
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

	// ── 제자리 고수 ─────────────────────────────────────
	//
	// 어떤 이유로도 움직이지 않는다. 이동 명령·라인·진격 목표를 전부 건너뛴다.
	// 이동 명령을 새로 받으면 플래그가 풀리므로 여기 걸리지 않는다.
	if (bHoldGround)
	{
		AI->ClearMoveRequest();
		return;
	}

	// ── 0순위 — 플레이어 명령 ───────────────────────────
	// 자동 판단보다 우선한다. 도착하면 스스로 해제되어 자동 진격으로 돌아간다.
	if (bHasCommandedDestination)
	{
		const float DistToCommand = FVector::Dist2D(GetActorLocation(), CommandedDestination);

		if (DistToCommand <= CommandArrivalRadius)
		{
			bHasCommandedDestination = false;
			bCommandIsAttackMove = false;
			CommandStallTimer = 0.f;

			// 명령 지점이 새 대기 자리가 된다.
			// 갱신하지 않으면 도착한 순간 원래 자리로 되돌아가 버린다.
			HoldLocation = CommandedDestination;
			bHoldLocationSet = true;
			bReturningToHold = false;
			bHoldReported = false;

			// 남은 이동 요청을 확실히 놓는다. 안 놓으면 도착 판정 뒤에도
			// NavMesh 가 계속 밀어 유닛이 지점을 지나쳐 간다.
			AI->ClearMoveRequest();

			if (UCharacterMovementComponent* Movement =
				GetCharacterMovement())
			{
				Movement->StopMovementImmediately();
			}

			SmoothedSteerDirection = FVector::ZeroVector;

			SF_LOG(Log, "[Unit] %s : 명령 지점 도착", *GetName());

			NotifyTutorialCommandArrived();

			if (ExecuteNextQueuedCommand())
			{
				return;
			}
		}
		else if (DistToCommand <= CommandFinalApproachRange)
		{
			// ── 마지막 구간 — NavMesh 를 놓고 직접 몬다 ──────
			//
			// RequestMoveToLocation 은 도달 반경에 캡슐 반지름을 더한 자리에서
			// 멈춘다 — 기본값이면 클릭 지점 95cm(50 + 45) 앞이다. 이 때문에
			// CommandArrivalRadius 를 95 밑으로 내릴 수가 없었고(내리면 도착
			// 판정이 영영 나지 않는다), 결국 "찍은 데 전에 멈추는" 그림이 됐다.
			//
			// 근접 전투에서 쓰는 방식 그대로, 이 구간만 직접 조향하면
			// 그 하한이 사라진다.
			AI->ClearMoveRequest();
			SteerTowardCommandPoint(DeltaSeconds);

			// 지형이나 다른 유닛에 막혀 끝내 못 닿는 경우의 안전장치.
			// 이게 없으면 명령 플래그가 안 풀려 그 유닛은 다시 싸우지 않는다.
			if (GetVelocity().SizeSquared2D() < FMath::Square(CloseQuartersStuckSpeed))
			{
				CommandStallTimer += DeltaSeconds;

				if (CommandStallTimer >= CommandStallDuration)
				{
					CommandStallTimer = 0.f;

					bHasCommandedDestination = false;
					bCommandIsAttackMove = false;

					// 선 자리가 새 대기 지점이다. 명령 지점을 넣으면
					// 못 간 곳으로 계속 되돌아가려 한다.
					HoldLocation = GetActorLocation();
					bHoldLocationSet = true;
					bReturningToHold = false;
					bHoldReported = false;

					SF_LOG(Log, "[Unit] %s : 명령 지점 %.0fcm 앞에서 막혀 정지합니다.",
						*GetName(), DistToCommand);

					// 막혀서 멈춘 것도 도착으로 친다.
					// 여기서 안 알리면 지형에 걸린 유닛 하나 때문에
					// 튜토리얼이 다음으로 넘어가지 못한다.
					NotifyTutorialCommandArrived();

					if (ExecuteNextQueuedCommand())
					{
						return;
					}
				}
			}
			else
			{
				CommandStallTimer = 0.f;
			}

			bNoTargetReported = false;
			return;
		}
		else
		{
			CommandStallTimer = 0.f;

			AI->RequestMoveToLocation(CommandedDestination, MoveAcceptanceRadius);
			bNoTargetReported = false;
			return;
		}
	}

	// ── 제자리 유지 ─────────────────────────────────────
	//
	// 스스로 진격하지 않는다. 아래 라인·핵심 건물 탐색을 전부 건너뛴다.
	//
	// 여기서 멈추는 것은 의도된 정지다. "갈 곳을 못 찾아 굳은" 것과 반드시
	// 구별되어야 하므로 한 번은 로그로 알린다.
	if (AdvanceMode == EUnitAdvanceMode::HoldPosition)
	{
		if (!bReturnToHoldLocation)
		{
			// 돌아가지 않는다. 싸움이 끝난 그 자리가 곧 새 대기 지점이다.
			//
			// 명령 지점은 "그때 가라고 한 곳" 일 뿐이다. 전투로 자리를 옮긴 뒤에도
			// 그리로 되돌아가면, 플레이어 눈에는 유닛이 제멋대로 후퇴하는 것으로 보인다.
			//
			// 플래그는 건드리지 않는다. SetHoldLocationToCurrent 를 쓰면
			// bHoldReported 가 매 틱 초기화되어 로그가 쏟아진다.
			HoldLocation = GetActorLocation();
			bHoldLocationSet = true;
		}
		else
		{
			const float ToHold = bHoldLocationSet
				? FVector::Dist2D(GetActorLocation(), HoldLocation)
				: 0.f;

			if (bHoldLocationSet && ToHold > HoldReturnAcceptRadius)
			{
				// 교전에 끌려 나갔다. 원래 자리로 돌아간다.
				AI->RequestMoveToLocation(HoldLocation, MoveAcceptanceRadius);

				bHoldReported = false;
				bNoTargetReported = false;
				return;
			}
		}

		// 자리에 도착했다. 복귀 완료.
		bReturningToHold = false;

		if (!bHoldReported)
		{
			bHoldReported = true;

			SF_LOG(Log,
				"[Unit] %s (Team %d) : 제자리 유지 — 자동 진격하지 않습니다. "
				"스스로 진격해야 하는 팀이라면 AutoAdvanceTeams 에 %d 을 넣으세요.",
				*GetName(), GetTeamIndex(), GetTeamIndex());
		}

		AI->ClearMoveRequest();
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

	// 교전에서 빠져나오는 순간 — 대상이 쓰러졌거나 사거리 밖으로 벗어난 때다.
	// 보장 시간이 이미 지났다면 남아 있는 공격 모션을 여기서 끊는다.
	// 아직 남아 있으면 Tick 이 만료 시점에 끊는다(킬 스윙 보호).
	if (CombatState == EUnitCombatState::Attacking && AttackAnimHoldRemaining <= 0.f)
	{
		StopAttackMontage();
	}

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

	// 배정받은 슬롯 위치를 함께 그린다.
	// "왜 저기로 가지" 의 답이 대부분 여기 있다.
	if (CurrentTarget && AttackSlotIndex != INDEX_NONE)
	{
		if (const USFCombatSubsystem* Combat = USFCombatSubsystem::Get(this))
		{
			FVector SlotLocation;

			if (Combat->GetAttackSlotLocation(
					CurrentTarget, AttackSlotIndex,
					GetApproachDistance(CurrentTarget), SlotLocation))
			{
				DrawDebugSphere(World, SlotLocation, 30.f, 12, FColor::Cyan, false, -1.f, 0, 2.f);
				DrawDebugLine(World, GetActorLocation(), SlotLocation, FColor::Cyan, false, -1.f, 0, 1.f);
			}
		}
	}

	const FString Info = FString::Printf(
		TEXT("Team%d | %s | %s | 대상:%s | 거리:%.0f | 슬롯:%d | 속도:%.0f | HP:%.0f"),
		GetTeamIndex(),
		StateText,
		*RowText,
		CurrentTarget ? *CurrentTarget->GetName() : (bNoTargetReported ? TEXT("적 전멸") : TEXT("없음")),
		CurrentTarget ? GetDistanceToTargetEdge(CurrentTarget) : -1.f,
		AttackSlotIndex,
		GetVelocity().Size2D(),
		HealthComponent ? HealthComponent->GetCurrentHealth() : 0.f);

	DrawDebugString(World, FVector(0.f, 0.f, 140.f), Info, const_cast<AUnitBase*>(this), FColor::White, 0.f, true);
#endif
}

void AUnitBase::HandleCombatDamaged(UCombatTargetComponent* InTarget, float InDamage, AActor* InInstigator)
{
	if (InDamage <= 0.f || !IsValid(DamagedSound))
	{
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(this, DamagedSound, GetActorLocation());
}

#if !UE_BUILD_SHIPPING
/**
 * 유닛 간 통과 여부를 즉시 전환한다.
 *
 * 살아 있는 유닛 전체에 한 번에 먹여야 A/B 비교가 되므로 여기서만 액터를 훑는다.
 * 치트 명령이고 호출 시점에 한 번만 도는 경로라, 평소의
 * "GetAllActorsOfClass 를 쓰지 않는다" 규칙에서 예외로 둔다.
 *
 *   sf.UnitsBlockEachOther 0   통과 (병목 완화)
 *   sf.UnitsBlockEachOther 1   막음 (기존 동작)
 *
 * 이후 새로 스폰되는 유닛은 BP 의 bBlockOtherUnits 값을 따른다.
 * 값을 확정했으면 BP 기본값도 같이 바꿔둘 것.
 */
static void SFSetUnitsBlockEachOther(const TArray<FString>& Args, UWorld* World)
{
	if (!IsValid(World)) return;

	if (Args.Num() == 0)
	{
		SF_LOG(Warning, "사용법: sf.UnitsBlockEachOther 0|1   (0=통과, 1=막음)");
		return;
	}

	const bool bBlock = FCString::Atoi(*Args[0]) != 0;
	int32 Count = 0;

	for (TActorIterator<AUnitBase> It(World); It; ++It)
	{
		It->SetBlockOtherUnits(bBlock);
		++Count;
	}

	SF_LOG(Warning, "[Unit] 유닛 간 충돌 = %s (%d 기에 적용). 새로 스폰되는 유닛은 BP 설정을 따릅니다.",
		bBlock ? TEXT("막음") : TEXT("통과"), Count);
}

static FAutoConsoleCommandWithWorldAndArgs GSFSetUnitsBlockEachOther(
	TEXT("sf.UnitsBlockEachOther"),
	TEXT("유닛끼리 서로를 막을지 즉시 전환한다. 0=통과, 1=막음."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SFSetUnitsBlockEachOther));
#endif

// ─────────────────────────────────────────────────────────
// 메인 루프
// ─────────────────────────────────────────────────────────
void AUnitBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 어느 분기로 빠지든 표시되도록 맨 앞에서 그린다
	DrawCombatDebug();

	NotifyUnitStatsChanged();

	if (CombatState == EUnitCombatState::Dead)
		return;

	if (AttackCooldownRemaining > 0.f)
	{
		AttackCooldownRemaining -= DeltaSeconds;
	}

	if (CommandAttackMoveGraceRemaining > 0.f)
	{
		CommandAttackMoveGraceRemaining =
			FMath::Max(
				0.f,
				CommandAttackMoveGraceRemaining - DeltaSeconds
			);
	}

	// 공격 모션 보장 시간.
	//
	// 만료됐는데 이미 교전 상태가 아니라면, 대상이 쓰러진 뒤에도 남아 도는
	// 모션이라는 뜻이므로 여기서 끊는다.
	// SetCombatState 만으로는 부족하다 — 상태가 바뀌는 그 시점에는 아직
	// 킬 스윙이 재생 중이라 끊을 수 없기 때문이다.
	if (AttackAnimHoldRemaining > 0.f)
	{
		AttackAnimHoldRemaining -= DeltaSeconds;

		if (AttackAnimHoldRemaining <= 0.f && CombatState != EUnitCombatState::Attacking)
		{
			StopAttackMontage();
		}
	}

	// ── 부대 강제 이동 완료 후 대기 ─────────────────────
	//
	// 자기 목적지에는 도착했지만 부대의 다른 유닛이 아직 이동 중이다.
	// 컨트롤러가 선택 부대 전체의 도착을 확인할 때까지 전투를 재개하지 않는다.
	if (bWaitingForGroupMoveRelease &&
		!bHasCommandedDestination)
	{
		CurrentTarget = nullptr;
		ForcedTarget = nullptr;
		ReleaseAttackSlot();

		SetCombatState(EUnitCombatState::Advancing);

		if (AUnitAIController* AI = GetUnitAI())
		{
			AI->ClearMoveRequest();
		}

		return;
	}

	// ── 0. 플레이어 명령 우선 ───────────────────────────
	//
	// 명령이 살아 있는 동안에는 교전 판정 자체를 하지 않는다.
	// 아래 타겟 갱신 이후로 내려가면, 사거리 안에 적이 있을 때
	// 교전 분기가 먼저 걸려 Advance() 에 도달하지 못한다.
	// (그래서 "전투 중에는 이동 명령이 무시되는" 문제가 있었다)
	//
	// 도착하면 bHasCommandedDestination 이 풀리고 다음 틱부터
	// 평소대로 적을 찾아 싸운다.
	//
	// 공격 이동(A)은 여기를 통과해 아래로 내려간다.
	// 평소대로 적을 찾고, 적이 없을 때만 Advance() 의 0순위가 목적지로 보낸다.
	if (bHasCommandedDestination &&	(!bCommandIsAttackMove || CommandAttackMoveGraceRemaining > 0.f))
	{
		// 명령 수행 중에는 타겟을 들고 있지 않는다.
		// 남겨두면 도착 직후 낡은 타겟으로 곧장 달려간다.
		CurrentTarget = nullptr;

		SetCombatState(EUnitCombatState::Advancing);
		Advance(DeltaSeconds);
		return;
	}

	// ── 1. 타겟 갱신 ────────────────────────────────────
	// 타겟이 죽었으면 즉시, 아니면 주기마다 재탐색한다.
	RetargetTimer -= DeltaSeconds;

	// 플레이어가 대상을 찍었으면 탐색하지 않는다.
	// 쓰러지거나 사라지면 지정을 놓고 평소 탐색으로 돌아간다.
	if (ForcedTarget)
	{
		if (IsTargetUsable(ForcedTarget))
		{
			CurrentTarget = ForcedTarget;
		}
		else
		{
			SF_LOG(Log, "[Unit] %s : 강제 공격 대상이 사라져 지정을 해제합니다.", *GetName());

			ForcedTarget = nullptr;
			CurrentTarget = nullptr;
			RetargetTimer = 0.f;

			if (ExecuteNextQueuedCommand())
			{
				return;
			}
		}
	}

	if (!ForcedTarget && (!IsTargetUsable(CurrentTarget) || RetargetTimer <= 0.f))
	{
		RetargetTimer = RetargetInterval;

		AActor* NewTarget = FindBestTarget();

		if (NewTarget != CurrentTarget)
		{
			CurrentTarget = NewTarget;

			if (IsValid(CurrentTarget))
			{
				AlertNearbyAllies(CurrentTarget);
			}

			// 대상이 바뀌었으니 진단 플래그를 되돌린다.
			// 안 그러면 새 대상에서 같은 문제가 생겨도 한 번도 알리지 못한다.
			bSightBlockedReported = false;
			bRangeUnreachableReported = false;

			// 여기서 StopMovement 를 부르지 않는다.
			// 타겟이 바뀔 때마다 멈춰 세우면, 뒤엉킨 난전에서 최근접 대상이
			// 계속 뒤바뀌며 제자리 밀치기가 된다.
			if (AUnitAIController* AI = GetUnitAI())
			{
				AI->ForceRepath();
			}
		}
	}

	// ── 1-1. 목줄 (복귀를 켠 경우에만) ──────────────────
	//
	// 교전 반경 안의 적은 상대하되, 대기 지점에서 너무 멀어지면 놓고 돌아간다.
	//
	// bReturnToHoldLocation 이 꺼져 있으면 이 블록 전체를 건너뛴다.
	// 복귀 없이 목줄만 걸면 경계선에서 대상을 놓았다 물었다 반복하며
	// 그 자리에서 덜컥거리기만 한다. 둘은 한 몸이다.
	//
	// 놓는 거리(LeashRadius)와 다시 무는 거리(CommandArrivalRadius)를 다르게 둔다.
	// 같게 두면 경계선에서 물었다 놓았다를 반복하며 그 자리에서 왔다 갔다 한다.
	if (bReturnToHoldLocation
		&& AdvanceMode == EUnitAdvanceMode::HoldPosition
		&& bHoldLocationSet
		&& !bHasCommandedDestination)
	{
		if (FVector::Dist2D(GetActorLocation(), HoldLocation) > LeashRadius)
		{
			bReturningToHold = true;
		}

		if (bReturningToHold)
		{
			CurrentTarget = nullptr;
			ForcedTarget = nullptr;
			ReleaseAttackSlot();

			SetCombatState(EUnitCombatState::Advancing);
			Advance(DeltaSeconds);
			return;
		}
	}

	// ── 1-1. 공격 슬롯 갱신 ─────────────────────────────
	// 타겟이 확정된 직후에 한 번만 부른다.
	// 대상이 없으면 잡고 있던 자리를 반납한다.
	UpdateAttackSlot();

	// ── 2. 교전 대상 없음 → 전진 ────────────────────────
	if (!CurrentTarget)
	{
		SetCombatState(EUnitCombatState::Advancing);
		Advance(DeltaSeconds);
		return;
	}

	bNoTargetReported = false;

	// ── 3. 사거리 · 시야 판정 ───────────────────────────
	const float Distance = GetDistanceToTargetEdge(CurrentTarget);

	// 교전 시작 문턱과 유지 문턱을 분리한다.
	//   시작: AttackRange * AttackApproachRatio  — 제대로 붙어야 때린다
	//   유지: AttackRange + RangeHysteresis      — 한번 붙으면 조금 벌어져도 계속
	const bool bWasAttacking = (CombatState == EUnitCombatState::Attacking);

	// 건물과 유닛의 규칙 차이는 GetBaseRange 에 모아뒀다.
	// 슬롯 위치 계산(GetApproachDistance)도 같은 함수를 쓰므로
	// 두 곳이 어긋날 일이 없다.
	const float BaseRange = GetBaseRange(CurrentTarget);

	const float EffectiveRange = bWasAttacking
		? FMath::Max(BaseRange, AttackRange + RangeHysteresis)
		: BaseRange;

	const bool bInRange = (Distance <= EffectiveRange);

	// 사거리 안일 때만 스윕한다. 밖이면 어차피 이동해야 하므로 낭비다.
	const bool bCanSee = bInRange ? HasLineOfSight(CurrentTarget) : false;

	// 사거리 안인데 시야가 막혔다.
	//
	// 이 조합은 "붙어 서서 아무것도 하지 않는" 증상으로만 나타나고,
	// 원래는 로그가 전혀 없어 추적이 불가능했다. 한 번은 반드시 알린다.
	//
	// 대상 옆에 붙은 WorldStatic(광물·바위·담장)이 구체 스윕에 걸리는 것이
	// 가장 흔한 원인이다.
	if (bInRange && !bCanSee)
	{
		if (!bSightBlockedReported)
		{
			bSightBlockedReported = true;

			SF_LOG(Warning,
				"[Unit] %s : %s 이(가) 사거리 안(%.0f / %.0f)인데 시야가 막혀 공격하지 못합니다. "
				"대상 옆에 WorldStatic 액터가 붙어 있는지 확인하세요. "
				"건물이라면 bRequireLineOfSightForBuildings 를 끄면 해결됩니다.",
				*GetName(), *CurrentTarget->GetName(), Distance, EffectiveRange);
		}
	}
	else
	{
		bSightBlockedReported = false;
	}

	if (bInRange && bCanSee)
	{
		// ── 교전 ────────────────────────────────────────
		SetCombatState(EUnitCombatState::Attacking);
		StuckTimer = 0.f;
		CloseQuartersStuckTimer = 0.f;
		bRangeUnreachableReported = false;

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
	else if (bHoldGround)
	{
		// ── 제자리 고수 ─────────────────────────────────
		//
		// 사거리 밖 대상은 쫓지 않는다. 놓고 그 자리를 지킨다.
		// 놓지 않으면 Chasing 상태로 굳은 채 계속 대상을 바라보기만 한다.
		CurrentTarget = nullptr;
		ReleaseAttackSlot();

		SetCombatState(EUnitCombatState::Advancing);

		if (AUnitAIController* AI = GetUnitAI())
		{
			AI->ClearMoveRequest();
		}
	}
	else
	{
		// ── 추격 ────────────────────────────────────────
		SetCombatState(EUnitCombatState::Chasing);

		// 타겟을 발견한 순간부터 적의 중심이 아닌
		// 자신에게 배정된 공격 슬롯을 향해 이동한다.
		//
		// 유닛마다 서로 다른 슬롯을 추적하므로 이동 경로가
		// 처음부터 좌우로 나뉘게 된다.
		AUnitAIController* AI = GetUnitAI();
		USFCombatSubsystem* Combat = USFCombatSubsystem::Get(this);

		if (AI)
		{
			FVector SlotLocation;

			const bool bHasValidSlot = Combat && AttackSlotIndex != INDEX_NONE && Combat->GetAttackSlotLocation(
					CurrentTarget,
					AttackSlotIndex,
					GetApproachDistance(CurrentTarget),
					SlotLocation);

			if (bHasValidSlot)
			{
				// 적 중심이 아니라 배정된 슬롯으로 이동한다.
				AI->RequestMoveToLocation(SlotLocation, SlotAcceptanceRadius, true);

				const float DistanceToSlot =
					FVector::Dist2D(
						GetActorLocation(),
						SlotLocation);

				// 아직 슬롯에서 멀리 떨어져 있는데 실제 이동 속도가 낮다면
				// 앞 유닛이나 장애물 때문에 접근하지 못하는 것으로 본다.
				const bool bSlotApproachBlocked =
					DistanceToSlot > SlotAcceptanceRadius + 50.f
					&& GetVelocity().SizeSquared2D()
					< FMath::Square(CloseQuartersStuckSpeed);

				if (bSlotApproachBlocked)
				{
					CloseQuartersStuckTimer += DeltaSeconds;

					if (CloseQuartersStuckTimer
						>= CloseQuartersStuckDuration)
					{
						if (!bRangeUnreachableReported)
						{
							bRangeUnreachableReported = true;

							SF_LOG(
								Warning,
								"[Unit] %s : 공격 슬롯 접근 정체. "
								"대상=%s / 슬롯=%d / 슬롯 거리=%.0f / 속도=%.0f",
								*GetName(),
								*CurrentTarget->GetName(),
								AttackSlotIndex,
								DistanceToSlot,
								GetVelocity().Size2D());
						}

						// 플레이어가 직접 지정한 공격 대상은 임의로 변경하지 않는다.
						if (!ForcedTarget)
						{
							AActor* BlockedTarget = CurrentTarget;

							// 접근하지 못한 슬롯을 먼저 반납한다.
							ReleaseAttackSlot();

							// 기존 타겟을 제외하고 주변의 다른 적을 탐색한다.
							AActor* AlternativeTarget =
								FindBestTarget(BlockedTarget);

							if (IsValid(AlternativeTarget))
							{
								CurrentTarget = AlternativeTarget;
								RetargetTimer = RetargetInterval;

								CloseQuartersStuckTimer = 0.f;
								bRangeUnreachableReported = false;

								AI->ForceRepath();

								SF_LOG(
									Log,
									"[Unit] %s : 슬롯 접근 실패로 타겟 변경. %s → %s",
									*GetName(),
									*BlockedTarget->GetName(),
									*AlternativeTarget->GetName());

								return;
							}

							// 다른 적이 없다면 기존 적을 유지한다.
							// 슬롯은 다음 틱의 UpdateAttackSlot()에서 다시 요청한다.
							CurrentTarget = BlockedTarget;
							RetargetTimer = 0.f;

							CloseQuartersStuckTimer = 0.f;
							bRangeUnreachableReported = false;
						}
					}
				}
				else
				{
					CloseQuartersStuckTimer = 0.f;
					bRangeUnreachableReported = false;
				}
			}
			else
			{
				// 공격 슬롯을 받지 못한 경우 적 중앙으로 들어가지 않는다.
				//
				// UpdateAttackSlot()이 다음 틱에도 호출되기 때문에
				// 빈 슬롯이 생기면 다시 슬롯을 받을 수 있다.
				AI->ClearMoveRequest();
			}
		}

		StuckTimer = 0.f;
	}
}

void AUnitBase::AlertNearbyAllies(AActor* InTarget)
{
	if (!IsTargetUsable(InTarget))
	{
		return;
	}

	USFCombatSubsystem* Combat = USFCombatSubsystem::Get(this);
	if (!IsValid(Combat))
	{
		return;
	}

	TArray<AActor*> NearbyAllies;

	Combat->GetNearbyAlliesForSeparation(
		this,
		GetTeamIndex(),
		AllyAssistRadius,
		NearbyAllies
	);

	for (AActor* AllyActor : NearbyAllies)
	{
		AUnitBase* AllyUnit = Cast<AUnitBase>(AllyActor);
		if (!IsValid(AllyUnit))
		{
			continue;
		}

		AllyUnit->ReceiveAssistTarget(InTarget);
	}
}

void AUnitBase::ReceiveAssistTarget(AActor* InTarget)
{
	// 죽은 유닛은 지원 요청을 받지 않는다.
	if (CombatState == EUnitCombatState::Dead)
	{
		return;
	}

	// 실제로 공격할 수 있는 적인지 검사한다.
	if (!IsTargetUsable(InTarget))
	{
		return;
	}

	// 플레이어가 직접 공격 대상을 지정했다면 그것을 우선한다.
	if (IsValid(ForcedTarget))
	{
		return;
	}

	// 제자리 고수 명령 중이라면 주변 교전에 합류하지 않는다.
	if (bHoldGround)
	{
		return;
	}

	// 이미 정상적인 공격 대상이 있으면 기존 전투를 유지한다.
	if (IsTargetUsable(CurrentTarget))
	{
		return;
	}

	CurrentTarget = InTarget;
	RetargetTimer = RetargetInterval;

	bSightBlockedReported = false;
	bRangeUnreachableReported = false;
	bNoTargetReported = false;
	bAdvanceTargetReported = false;

	if (AUnitAIController* AI = GetUnitAI())
	{
		AI->ForceRepath();
	}
}
