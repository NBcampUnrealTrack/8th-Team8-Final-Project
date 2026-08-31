#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interfaces/Units/Damagable.h"
#include "InGame/SFSelectINfo/SFSelectable.h"
#include "Units/Interfaces/Movable.h"
#include "Interfaces/Units/Targetable.h"
#include "Datas/Unit/UnitStatData.h"
#include "UnitBase.generated.h"

class UHealthComponent;
class UCombatTargetComponent;
class USFBuffComponent;
class UWidgetComponent;
class AUnitAIController;
class UNiagaraSystem;
class UAnimMontage;
class ALanePath;

/** 전투 상태 머신 */
UENUM(BlueprintType)
enum class EUnitCombatState : uint8
{
	Advancing	UMETA(DisplayName = "전진"),		// 교전 대상 없음 — 라인/적 핵심 방향으로
	Chasing		UMETA(DisplayName = "추격"),		// 타겟 있음, 사거리 밖
	Attacking	UMETA(DisplayName = "교전"),		// 타겟 있음, 사거리 안 + 시야 열림
	Dead		UMETA(DisplayName = "사망")
};

/**
 * 플레이어 입력이 유닛에 내리는 명령.
 *
 * 컨트롤러는 눌린 키에 해당하는 값을 들고 있다가, 클릭 시점에
 * AUnitBase::ExecuteCommand 한 곳으로 넘기면 된다.
 * 명령이 늘어나도 컨트롤러 쪽 분기는 그대로다.
 */
UENUM(BlueprintType)
enum class ESFUnitCommand : uint8
{
	/** 이동(우클릭). 가는 동안 반격하지 않는다 */
	Move			UMETA(DisplayName = "이동"),

	/** 공격 이동(A + 클릭). 가는 길에 적을 만나면 교전한다 */
	AttackMove		UMETA(DisplayName = "공격 이동"),

	/** 대상 공격. 대상을 못 찍었으면 그 지점으로 공격 이동한다 */
	Attack			UMETA(DisplayName = "대상 공격"),

	/** 정지(S). 명령을 모두 놓는다. 목줄 안에서는 여전히 적을 쫓는다 */
	Stop			UMETA(DisplayName = "정지"),

	/** 제자리 고수(H). 사거리 안의 적만 치고 한 발짝도 움직이지 않는다 */
	HoldGround		UMETA(DisplayName = "제자리 고수")
};

/**
 * 명령이 없을 때 유닛이 스스로 무엇을 할지.
 *
 * 진영으로 갈리는 값이 아니다. 아군·적군이 같은 클래스와 같은 BP 를 공유하므로
 * 코드에는 팀 분기를 두지 않고, 스폰하는 쪽이 SetAdvanceMode 로 주입한다.
 * TeamIndex 를 스폰 시점에 넣어주는 것과 같은 방식이다.
 */
UENUM(BlueprintType)
enum class EUnitAdvanceMode : uint8
{
	/**
	 * 제자리 유지. 스스로 진격하지 않는다.
	 *
	 * 교전 반경 안의 적과는 싸우지만, 자리에서 너무 멀어지면 놓고 돌아온다.
	 * 플레이어가 조작하는 유닛에 쓴다 — 뽑아둔 유닛이 알아서 적진으로
	 * 걸어가 버리면 조작할 수가 없다.
	 */
	HoldPosition	UMETA(DisplayName = "제자리 유지"),

	/**
	 * 적 핵심 건물을 향해 스스로 진격한다. 라인이 지정돼 있으면 라인을 따른다.
	 * AI 가 굴리는 웨이브 유닛에 쓴다.
	 */
	PushToCore		UMETA(DisplayName = "핵심 건물로 진격")
};

/**
 * 자동 전투 유닛.
 *
 * 아군·적군이 이 클래스 하나를 공유한다.
 * 구분은 TeamIndex 값뿐이며 로직에는 어떤 진영 분기도 두지 않는다.
 * 체력은 UHealthComponent 가 전담한다.
 */
UCLASS()
class STRIKEFORGE_API AUnitBase : public ACharacter, public ISFSelectable, public IMovable
{
	GENERATED_BODY()

public:
	AUnitBase();
	
	void SetUnitRowName(const FName& InRowName) { UnitRowName = InRowName; };

	/** 데이터테이블 행 이름. "같은 종류" 판정의 기준이 된다 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Unit")
	FORCEINLINE FName GetUnitRowName() const { return UnitRowName; }
	
	// ── ISelectable ─────────────────────────────────────


	// ── ISFSelectable ───────────────────────────────────
	/**
	 * HUD 선택 패널에 넘길 정보를 만든다.
	 *
	 * 런타임 값(체력·공격력·이동속도)은 컴포넌트와 현재 스탯에서,
	 * 표시용 값(초상화·설명)은 UI 테이블에서 가져온다.
	 * 두 테이블 모두 UnitRowName 을 키로 쓴다.
	 */
	virtual FSFSelectableInfo GetSelectableInfo_Implementation() override;
	virtual const FHealthData GetHealthData_Implementation() override;
	/** 체력 변화를 구독해 선택 패널을 갱신하려면 이걸 쓴다 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Unit")
	FORCEINLINE UHealthComponent* GetHealthComponent() const { return HealthComponent; }

	/** 소속 팀. 전투 대상 컴포넌트가 단일 출처다 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Unit")
	int32 GetTeamIndex() const;

	// ── 외부 인터페이스 ─────────────────────────────────

	/** 데이터테이블 행으로 능력치를 주입한다. 스폰 직후 호출 */
	void InitStat(const FUnitStatData& InRow);

	/** EDataTableType::Unit 에서 RowName 을 찾아 주입한다 */
	UFUNCTION(BlueprintCallable, Category = "SF|Unit")
	bool InitStatFromTable(FName InRowName);

	UFUNCTION(BlueprintCallable, Category = "SF|Unit")
	void SetAssignedLane(ALanePath* InLane);

	// ── 자동 진격 여부 ──────────────────────────────────
	//
	// 팀으로 갈리는 값이 아니다. 스폰하는 쪽이 넣어준다.
	//   플레이어 생산 유닛 → HoldPosition (기본값이라 아무것도 안 해도 된다)
	//   AI 웨이브 유닛     → PushToCore   (스포너나 생산 BP 에서 주입)

	/**
	 * 명령이 없을 때의 행동을 직접 주입한다. 스폰 직후 호출한다.
	 *
	 * 한 번 호출하면 아래 팀 기준 자동 판정을 덮어쓴다.
	 * 특정 유닛만 예외로 두고 싶을 때 쓴다.
	 */
	UFUNCTION(BlueprintCallable, Category = "SF|Unit")
	void SetAdvanceMode(EUnitAdvanceMode InMode);

	/**
	 * 자기 팀 번호를 AutoAdvanceTeams 와 대조해 진격 여부를 정한다.
	 *
	 * BeginPlay 에서 자동으로 호출된다.
	 * 스폰 이후에 SetTeamIndex 로 팀을 바꿨다면 직접 다시 불러야 한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "SF|Unit")
	void ResolveAdvanceModeFromTeam();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Unit")
	FORCEINLINE EUnitAdvanceMode GetAdvanceMode() const { return AdvanceMode; }

	/**
	 * 지금 위치를 새 대기 지점으로 삼는다.
	 * 이동 명령을 완수하거나 정지 명령을 받으면 자동으로 호출된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "SF|Unit")
	void SetHoldLocationToCurrent();

	/** 역할별 공격 우선순위에 따라 대상 탐색 */
	UFUNCTION(BlueprintCallable, Category = "SF|Combat")
	AActor* FindBestTarget();

	UFUNCTION(BlueprintCallable, Category = "SF|Combat")
	void PerformAttack(AActor* InTarget);

	/** 타겟까지 시야가 열려 있는가. 벽 너머 공격을 막는다 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Combat")
	bool HasLineOfSight(const AActor* InTarget) const;

	// ── IMovable ────────────────────────────────────────
	//
	// 플레이어의 이동 명령을 받는다.
	// 명령은 "진격 목표를 교체" 하는 것이지 전투를 멈추는 게 아니다.
	// 가는 길에 적을 만나면 평소처럼 교전하고, 정리되면 다시 목적지로 향한다.

	virtual void MoveToLocation_Implementation(const FVector& InDestination) override;
	virtual void StopMove_Implementation() override;

	// ── 전투 명령 ───────────────────────────────────────
	//
	// IMovable 의 MoveToLocation 은 스타크래프트의 강제 이동(M)이다.
	// 가는 도중 적을 만나도 반격하지 않는다.
	// 아래 둘은 그 위에 얹는 전투 명령으로, Input Action 은 이쪽에 붙인다.

	/**
	 * 명령을 한 곳에서 받는 창구.
	 *
	 * 컨트롤러는 대기 중인 명령 종류와 클릭 결과(지점·액터)만 넘기면 된다.
	 * 명령별 분기는 여기서 처리하므로 입력 쪽 코드가 단순해진다.
	 *
	 * @param InLocation 클릭한 지면 좌표. 지점이 필요 없는 명령에서는 무시된다
	 * @param InTarget   클릭한 액터. 없으면 nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "SF|Command")
	void ExecuteCommand(ESFUnitCommand InCommand, const FVector& InLocation, AActor* InTarget);

	/**
	 * 이동(우클릭). IMovable::MoveToLocation 과 같은 동작이다.
	 * 컨트롤러가 세 명령을 같은 방식으로 부를 수 있도록 이름을 맞춰뒀다.
	 */
	UFUNCTION(BlueprintCallable, Category = "SF|Command")
	void CommandMoveTo(const FVector& InDestination);

	/**
	 * 공격 이동(A). 목적지로 향하되 도중에 적을 만나면 교전한다.
	 * 적을 정리하면 스스로 목적지로 다시 향한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "SF|Command")
	void CommandAttackMove(const FVector& InDestination);

	/**
	 * 지정 대상 강제 공격. 그 대상이 쓰러질 때까지 다른 적으로 갈아타지 않는다.
	 *
	 * TObjectPtr 은 UFUNCTION 인자로 쓸 수 없으므로 원시 포인터를 받는다.
	 * 현재는 적대 진영만 지정할 수 있다 — 중립(TeamIndex -1) 강제 공격은
	 * IsHostileTeam 우회가 필요하며 기획 확정 후 붙인다.
	 */
	UFUNCTION(BlueprintCallable, Category = "SF|Command")
	void CommandAttackTarget(AActor* InTarget);

	/** 모든 명령 해제(S). 선 자리를 대기 지점으로 삼고 평소대로 싸운다 */
	UFUNCTION(BlueprintCallable, Category = "SF|Command")
	void CommandStop();

	/**
	 * 제자리 고수(H).
	 *
	 * 정지(S)와 다르다. 정지는 목줄 안에서는 적을 쫓지만,
	 * 고수는 사거리 안에 들어온 적만 치고 한 발짝도 움직이지 않는다.
	 * 진격 모드(AdvanceMode)보다 우선한다.
	 *
	 * 이동·공격 명령을 받으면 자동으로 풀린다.
	 */
	UFUNCTION(BlueprintCallable, Category = "SF|Command")
	void CommandHoldGround();

	/** 제자리 고수 중인가. HUD 아이콘 표시 등에 쓴다 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Command")
	FORCEINLINE bool IsHoldingGround() const { return bHoldGround != 0; }

	/**
	 * 유닛끼리 캡슐로 서로를 막을지 전환한다.
	 *
	 * 런타임에 바꿀 수 있다. 콘솔 명령 sf.UnitsBlockEachOther 가 이 함수를 쓴다.
	 */
	UFUNCTION(BlueprintCallable, Category = "SF|Combat")
	void SetBlockOtherUnits(bool bInBlock);

	/** 현재 명령이 공격 이동인가. HUD 커서 표시 등에 쓴다 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Command")
	FORCEINLINE bool IsAttackMoving() const { return bHasCommandedDestination && bCommandIsAttackMove; }

	/**
	 * 현재 전투 상태. AnimBP 가 애니메이션을 고르는 기준이 된다.
	 *
	 *   Advancing / Chasing  이동  → Walk
	 *   Attacking            교전  → Attack 몽타주
	 *   Dead                 사망  → Dying
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Unit")
	FORCEINLINE EUnitCombatState GetCombatState() const { return CombatState; }

	/** 수평 이동 속도. Blend Space 의 입력값으로 쓴다 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Unit")
	float GetGroundSpeed() const;

	/** 플레이어 명령을 수행 중인가 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Unit")
	FORCEINLINE bool HasMoveCommand() const { return bHasCommandedDestination != 0; }

	/** 이 유닛이 차지하는 인구수. 데이터테이블에서 주입된다 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Unit")
	FORCEINLINE int32 GetPopulationCost() const { return PopulationCost; }

	/** 대상이 건물 계열인가. 사거리 여유를 줄지 판단한다 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Combat")
	bool IsBuildingTarget(const AActor* InTarget) const;

	/** 대상 표면까지의 수평 거리. 건물처럼 큰 대상에서 필수 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Combat")
	float GetDistanceToTargetEdge(const AActor* InTarget) const;

	// ── 연출 훅 (BP 에서 구현) ──────────────────────────

	/** 공격이 성립한 순간. 몽타주·이펙트·사운드를 여기서 */
	UFUNCTION(BlueprintImplementableEvent, Category = "SF|Combat")
	void OnAttackPerformed(AActor* InTarget);

	UFUNCTION(BlueprintImplementableEvent, Category = "SF|Combat")
	void OnCombatStateChanged(EUnitCombatState InNewState);

	UFUNCTION(BlueprintImplementableEvent, Category = "SF|Unit")
	void OnUnitDied();

#pragma region Properties

	UPROPERTY(BlueprintReadOnly, Category = "SF|Runtime")
	TObjectPtr<AActor> CurrentTarget;

	UPROPERTY(BlueprintReadOnly, Category = "SF|Runtime")
	EUnitCombatState CombatState = EUnitCombatState::Advancing;

	/** 지휘관 스킬 등으로 곱해지는 공격 배율 */
	UPROPERTY(BlueprintReadOnly, Category = "SF|Runtime")
	float AttackMultiplier = 1.f;

#pragma endregion

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** CombatTargetComponent 의 파괴 델리게이트에 연결된다 */
	UFUNCTION()
	void HandleDeath(UCombatTargetComponent* InTarget);

	/**
	 * CombatTargetComponent 의 피격 델리게이트에 연결된다 — 반격.
	 *
	 * 이 훅이 없으면 사거리 밖에서 쏘는 원거리 유닛에게 일방적으로 맞는다.
	 * 제자리 고수 중인 유닛은 아예 무저항이 된다.
	 */
	UFUNCTION()
	void HandleDamaged(UCombatTargetComponent* InTarget, float InDamage, AActor* InInstigator);

	bool IsTargetUsable(const AActor* InTarget) const;

	/**
	 * 이 유닛이 플레이어 명령을 받아들이는가.
	 *
	 * 컨트롤러가 드래그 선택에 상대 유닛까지 같이 담아 명령을 넘기더라도,
	 * 스스로 진격하는 유닛이면 명령을 무시한다.
	 *
	 * 반드시 IMovable 진입점(MoveToLocation_Implementation·StopMove_Implementation)
	 * 에서 검사해야 한다. 컨트롤러는 인터페이스로 직접 부르므로, Command 계열
	 * 함수에만 걸어두면 그대로 통과한다.
	 *
	 * 팀 번호를 직접 비교하지 않고 기존 AutoAdvanceTeams 목록 조회와
	 * AdvanceMode 주입 여부만 본다 — "적이면" 이 아니라 "스스로 진격하는
	 * 유닛이 아니면" 으로 판정하므로 진영 분기가 아니다.
	 */
	bool IsCommandableByPlayer() const;

	/**
	 * 교전이 성립하는 거리(대상 표면 기준).
	 *
	 * 건물은 자기 주변 NavMesh 를 넓게 파먹어서 유닛이 아무리 붙어도
	 * 원래 사거리 안으로 들어가지 못한다. 그래서 비율 대신 여유를 더한다.
	 * AttackRange 자체를 늘리면 유닛 간 교전 사거리까지 함께 늘어난다.
	 */
	float GetBaseRange(const AActor* InTarget) const;

	/** 공격 슬롯에서 대상 표면으로부터 떨어져 설 거리 */
	float GetApproachDistance(const AActor* InTarget) const;

	/**
	 * CurrentTarget 에 맞춰 공격 슬롯을 갱신한다.
	 * 대상이 바뀌었으면 이전 자리를 먼저 반납한다.
	 */
	void UpdateAttackSlot();

	/** 잡고 있는 슬롯을 반납한다. 사망·소멸 시 반드시 부른다 */
	void ReleaseAttackSlot();

	/**
	 * 근접 구간(사거리 + SlotActivationRange 안) 전용 이동.
	 *
	 * NavMesh/DetourCrowd 에 맡기지 않고 매 틱 직접 계산한다 — 슬롯(있으면)
	 * 또는 대상 접근 지점으로 당기는 힘에, 근처 아군에게서 밀려나는 힘을
	 * 더해 AddMovementInput 으로 넣는다. 정확히 슬롯에 닿을 필요는 없다.
	 * 사거리 판정은 대상 표면까지의 거리로 따로 하므로, 접근하다 사거리에
	 * 들어오는 순간 위쪽 교전 분기가 멈춰 세운다.
	 */
	void SteerTowardSlot(float DeltaSeconds);

	/**
	 * 조향 방향에서 지형에 막히는 성분을 걷어낸다.
	 *
	 * 근접 구간은 NavMesh 이동을 끄고 직접 조향하므로, 지형을 피하는 책임이
	 * 여기에 있다. WallProbeDistance 가 0 이면 아무것도 하지 않는다.
	 */
	void ApplyWallAvoidance(FVector& InOutDirection) const;

	/**
	 * 명령 목적지의 마지막 구간(CommandFinalApproachRange 안) 전용 이동.
	 *
	 * NavMesh 이동은 도달 반경에 캡슐 반지름을 더한 자리에서 멈춘다 —
	 * 기본값으로 클릭 지점 95cm 앞이다. 이 구간만 직접 조향해서 그 여유를
	 * 없앤다. 근접 전투(SteerTowardSlot)와 같은 방식이지만, 반드시 지정한
	 * 한 점에 닿아야 하므로 분리력은 넣지 않는다.
	 */
	void SteerTowardCommandPoint(float DeltaSeconds);

	/** bBlockOtherUnits 값을 캡슐 콜리전에 반영한다 */
	void ApplyUnitCollisionMode();

	/** 교전 대상이 없을 때의 행동 — 라인 전진 또는 적 핵심 방향으로 */
	void Advance(float DeltaSeconds);

	// ── 진격로 코리도 ───────────────────────────────────

	/**
	 * 코리도 제한이 지금 이 유닛에 걸리는가.
	 *
	 * BP 스위치(bRestrictAutoChaseToLane) + 콘솔(sf.LaneRestrict) + 레벨에
	 * 라인이 배치돼 있는지를 모두 본다. 셋 중 하나라도 아니면 제한이 없다.
	 */
	bool IsLaneRestrictionActive() const;

	/**
	 * 이 대상을 자동으로 쫓는 것이 코리도 제한에 걸리는가.
	 *
	 * 걸리는 조건은 "코리도 밖 + 사거리 밖 + 건물 아님" 셋이 모두 참일 때뿐이다.
	 * 사거리 안이면 이동 없이 때리기만 하면 되므로 막을 이유가 없다.
	 */
	bool IsChaseBlockedByLane(const AActor* InTarget) const;

	/**
	 * 이 유닛이 탈 라인. AssignedLane 이 비어 있으면 가장 가까운 것을 찾아 넣는다.
	 *
	 * 스포너가 SetAssignedLane 을 부르지 않아도 동작하게 하기 위한 경로다.
	 * 한 번 정해지면 계속 그 라인을 쓴다.
	 */
	ALanePath* ResolveLane();

	/**
	 * 코리도를 따라 한 걸음 전진하거나, 벗어나 있으면 코리도로 돌아간다.
	 *
	 * @param InGoalLocation 진격 목표(적 핵심 건물 등)의 위치. 라인을 어느 쪽으로
	 *                       탈지 이것으로 정한다.
	 * @return 이동 요청을 냈으면 true. false 면 목표가 이미 코앞이거나 라인을 쓸
	 *         수 없다는 뜻이고, 호출부(Advance)는 목표로 직접 향한다.
	 */
	bool AdvanceAlongLane(ALanePath* InLane, AUnitAIController* InAI, const FVector& InGoalLocation);

	void SetCombatState(EUnitCombatState InNewState);

	void FaceTarget(float DeltaSeconds);

	AUnitAIController* GetUnitAI() const;

	/**
	 * 인구수를 증감한다.
	 * 생산 시 +PopulationCost, 사망 시 -PopulationCost.
	 * ASFGameState 가 없으면 조용히 넘어간다(테스트 레벨 대응).
	 */
	void ApplyPopulationDelta(int32 InDelta);

	/**
	 * 재생 중인 공격 몽타주를 끊는다.
	 *
	 * 이동 명령이 들어왔는데 공격 모션이 남아 있으면
	 * 유닛이 칼을 휘두르며 이동하는 그림이 된다.
	 */
	void StopAttackMontage();

	/** 이펙트를 지정 위치에 스폰한다. 시스템이 비어 있으면 아무것도 안 한다 */
	void SpawnEffectAt(UNiagaraSystem* InSystem, const FVector& InLocation) const;

	/**
	 * 디버그 표시 여부.
	 * 유닛별 bDrawCombatDebug 또는 콘솔 sf.DrawCombatDebug 중 하나라도 켜지면 true.
	 */
	bool ShouldDrawDebug() const;

	/** 교전 반경·타겟 라인·상태 문자열을 그린다. Tick 어느 분기에서든 호출된다 */
	void DrawCombatDebug() const;

#pragma region Components

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHealthComponent> HealthComponent;

	/** 팀·분류·방어를 보유하며 전투 레지스트리에 자신을 등록한다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCombatTargetComponent> CombatTarget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> StatusWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Component")
	TObjectPtr<USFBuffComponent> BuffComponent;

#pragma endregion

private:
#pragma region Information

	/** 데이터테이블 RowName. 비우면 아래 기본값을 그대로 쓴다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Information", meta = (AllowPrivateAccess = true))
	FName UnitRowName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Information", meta = (AllowPrivateAccess = true))
	EUnitRole UnitRole = EUnitRole::Defense;

#pragma endregion

#pragma region Stat

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat", meta = (AllowPrivateAccess = true))
	float Attack = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat", meta = (AllowPrivateAccess = true))
	float AttackRange = 270.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat", meta = (AllowPrivateAccess = true, ClampMin = "0.05"))
	float AttackCooldown = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat", meta = (AllowPrivateAccess = true))
	float Penetration = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat", meta = (AllowPrivateAccess = true))
	float SplashRadius = 0.f;

	/** 이 유닛이 차지하는 인구수 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat", meta = (AllowPrivateAccess = true, ClampMin = "0"))
	int32 PopulationCost = 1;

	/**
	 * 인구수를 이미 반영했는가.
	 * 생산과 사망에서 각각 한 번씩만 계산되도록 막는다.
	 */
	uint8 bPopulationApplied : 1;

	/** 광역 피해가 주 대상 대비 몇 배인가 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat", meta = (AllowPrivateAccess = true, ClampMin = "0.0", ClampMax = "1.0"))
	float SplashDamageRatio = 1.f;

#pragma endregion

#pragma region Combat Tuning

	/** 교전 반경. 이 안의 대상만 타겟으로 잡는다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true))
	float AggroRadius = 1050.f;

	/**
	 * 교전을 시작하는 거리 비율. 실제 시작 거리 = AttackRange * 이 값.
	 *
	 * 1.0 이면 최대 사거리 경계에서 멈춰 서서, 벽 모서리에 붙은 채
	 * 코너 너머를 찌르는 그림이 나온다. 낮출수록 제대로 붙어 싸운다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "0.2", ClampMax = "1.0"))
	float AttackApproachRatio = 0.7f;

	/**
	 * 건물 대상일 때 사거리에 더하는 여유(cm).
	 *
	 * 건물은 자기 주변 NavMesh 를 넓게 파먹어서, 유닛이 아무리 붙어도
	 * 원래 사거리 안으로 들어가지 못하는 경우가 있다.
	 * AttackRange 를 늘리면 유닛 간 교전 사거리까지 함께 늘어나므로
	 * 건물에만 적용되는 값을 따로 둔다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "0.0"))
	float BuildingRangeBonus = 250.f;

	/** 교전 유지 여유. 사거리 경계에서 걷다 멈추다 반복하는 것을 막는다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "0.0"))
	float RangeHysteresis = 60.f;

	/**
	 * 이동 명령의 도달 반경(cm). 사거리와 무관한 값이다.
	 *
	 * 언리얼은 여기에 목표 반지름을 더해 도달을 판정하므로,
	 * 사거리처럼 큰 값을 넣으면 벽 반대편에서 "도착했다"고 멈춰버린다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "10.0"))
	float MoveAcceptanceRadius = 50.f;

	/**
	 * 플레이어 명령 목적지에 도착했다고 볼 거리(cm).
	 *
	 * 예전에는 이 값의 하한이 95(MoveAcceptanceRadius 50 + 캡슐 반지름 45)였다.
	 * NavMesh 이동이 딱 그 자리에서 멈추기 때문에, 더 작게 잡으면 도착 판정이
	 * 영영 나지 않고 명령 플래그가 안 풀려 그 유닛은 다시 싸우지 않았다.
	 * 그래서 클릭 지점에서 1m 가까이 떨어져 서는 것이 구조적으로 불가피했다.
	 *
	 * 이제 마지막 구간(CommandFinalApproachRange 안)은 NavMesh 가 아니라
	 * SteerTowardCommandPoint 가 직접 몰고 가므로 그 하한이 사라졌다.
	 *
	 * 캡슐 반지름(45)보다 작게 두면 유닛 몸이 클릭 지점을 덮는다 —
	 * 눈으로는 "찍은 자리에 정확히 섰다" 로 보인다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "10.0"))
	float CommandArrivalRadius = 40.f;

	/**
	 * 명령 목적지까지 남은 거리가 이 값 안이면 NavMesh 를 놓고 직접 조향한다.
	 *
	 * 근접 전투에서 SlotActivationRange 가 하는 일과 같다. 먼 구간은 경로
	 * 추종이 낫고, 마지막에 정확히 붙는 일만 직접 계산이 낫다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "0.0"))
	float CommandFinalApproachRange = 250.f;

	/**
	 * 마지막 구간에서 이만큼(초) 제자리면 그 자리를 도착으로 친다.
	 *
	 * 지형이나 다른 유닛에 막혀 끝내 클릭 지점에 못 닿는 경우의 안전장치다.
	 * 이게 없으면 명령 플래그가 영영 풀리지 않아 그 유닛은 다시 싸우지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "0.05"))
	float CommandStallDuration = 0.5f;

	/**
	 * 대기 지점으로 복귀할 때의 도착 반경(cm).
	 *
	 * 이쪽은 여전히 NavMesh 이동이라 위의 95 하한이 그대로 적용된다.
	 * CommandArrivalRadius 와 같은 값을 쓰면 복귀가 끝나지 않아 대기 지점에서
	 * 계속 들썩인다. 그래서 별도로 둔다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "100.0"))
	float HoldReturnAcceptRadius = 120.f;

	/** 타겟 재탐색 주기(초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "0.05"))
	float RetargetInterval = 0.3f;

	/** 추격 중 이만큼(초) 제자리면 경로를 다시 계산한다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "0.1"))
	float StuckThreshold = 0.6f;

	/**
	 * 유닛끼리 캡슐로 서로를 막을 것인가. 기본은 끔(= 서로 통과).
	 *
	 * 켜면 ACharacter 기본값 그대로 Pawn 채널을 Block 한다.
	 * 그러면 대상에 붙은 앞줄 3~4기가 물리적인 벽이 되어, 뒤에서 오는 유닛은
	 * 자리가 남아 있어도 그리로 갈 수가 없다. 병목의 실체가 이것이다.
	 *
	 * 끄면 유닛이 서로를 통과한다. 겹치지 않게 벌리는 일은
	 * DetourCrowd 분리력(AUnitAIController::CrowdSeparationWeight)과
	 * 공격 슬롯이 맡는다. 슬롯이 각자 다른 각도를 주므로 최종 위치가 갈린다.
	 *
	 * Overlap 이 아니라 Ignore 를 쓴다. 겹침 이벤트를 쓰지 않는데
	 * Overlap 으로 두면 유닛 수의 제곱만큼 이벤트가 발생한다.
	 *
	 * 주의: 적 유닛도 통과한다. 콜리전 채널은 팀을 구분하지 못한다.
	 * 다만 사거리에 들어오는 순간 멈추므로 상대를 뚫고 지나가지는 않는다.
	 * 건물과 지형은 WorldStatic 이라 영향이 없다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true))
	uint8 bBlockOtherUnits : 1;

	/** 공격에 시야가 필요한가. 끄면 벽 너머로도 때린다 (곡사 유닛) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true))
	uint8 bRequireLineOfSight : 1;

	/**
	 * 건물 대상에도 시야를 요구할 것인가. 기본은 끔.
	 *
	 * 건물은 크고 고정된 대상이고, 사거리 판정이 이미 바운딩 박스 표면 기준이다.
	 * 사거리 안에 들었다는 것은 사실상 건물에 붙었다는 뜻이라 시야를 따질 이유가 없다.
	 *
	 * 반대로 요구하면, 광물 위에 세운 채집 건물처럼 대상 바로 옆에 다른
	 * WorldStatic 액터가 붙어 있는 경우 구체 스윕이 그것에 걸려 시야가 영구히
	 * 막힌 것으로 나온다. 유닛은 붙어서 바라보기만 하고 피해는 안 들어간다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true))
	uint8 bRequireLineOfSightForBuildings : 1;

	/**
	 * 시야 검사에 쓰는 구체 반지름(cm).
	 *
	 * 0 에 가까우면 선 검사와 같아져 모서리를 스치는 순간 시야가 열린다.
	 * 캡슐 반지름에 가깝게 두면 몸이 실제로 코너를 빠져나와야 열린다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "0.0"))
	float LineOfSightRadius = 40.f;

	/**
	 * 피격 시 반격할 것인가.
	 *
	 * 끄면 예전처럼 교전 반경 탐색으로만 대상을 정한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true))
	uint8 bRetaliateWhenDamaged : 1;

	/**
	 * 이미 다른 대상과 싸우는 중일 때, 가해자로 갈아탈 거리 조건.
	 *
	 * 가해자가 "현재 대상까지의 거리 × 이 값" 보다 가까울 때만 바꾼다.
	 * 1.0 이면 조금이라도 가까우면 갈아탄다 — 광역 피해나 난전에서 매 타격마다
	 * 대상이 뒤바뀌어 아무것도 못 때리게 된다. 그래서 기본값을 낮게 둔다.
	 *
	 * 대상이 아예 없을 때는 이 값과 무관하게 무조건 받는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "0.1", ClampMax = "1.0"))
	float RetaliateSwitchRatio = 0.6f;

	/**
	 * 반격 대상을 유지하는 시간(초).
	 *
	 * 이 시간 동안은 주기 재탐색(RetargetInterval)이 반격 대상을 덮지 않는다.
	 * 없으면 0.3초 뒤 최근접 판정이 다시 돌아 원래 대상으로 돌아가 버린다.
	 *
	 * 대상이 죽거나 사라지면 유지와 무관하게 즉시 다시 찾는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "0.0"))
	float RetaliateStickyDuration = 2.f;

	/** 시야 트레이스를 쏘는 높이 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true))
	float EyeHeight = 60.f;

	/** 시야가 막힌 대상의 거리 점수 배율. 클수록 벽 뒤 적을 덜 쫓는다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "1.0"))
	float BlockedTargetPenalty = 2.5f;

	/**
	 * 현재 타겟에 주는 가산점 배율(작을수록 잘 안 바꾼다).
	 *
	 * 1.0 이면 매 탐색마다 최근접으로 갈아탄다. 난전에서는 최근접이
	 * 계속 바뀌므로 유닛이 앞으로 못 나가고 제자리에서 밀치기만 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "0.1", ClampMax = "1.0"))
	float TargetStickiness = 0.6f;

	/**
	 * 타겟 분류별 거리 가중치. 점수 = 거리 × 가중치.
	 *
	 * 전부 1.0 이면 "시야가 열린 가장 가까운 적" 을 고른다(기본).
	 * 특정 분류를 우선하려면 1 보다 작게, 후순위로 미루려면 크게 준다.
	 * 비어 있는 항목은 1.0 으로 간주한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true))
	TMap<ETargetType, float> TargetTypeWeights;

	/**
	 * 슬롯을 사거리 경계에서 얼마나 더 안쪽에 둘지(cm).
	 *
	 * 접근 거리 = 사거리 − 이 값. 작게 잡아야 한다.
	 *
	 * 슬롯은 "도착해야 하는 자리" 가 아니라 "그 방향으로 파고들라" 는 지시다.
	 * 유닛은 자리에 닿기 전에 사거리에 들어오고, 그 순간 교전 분기가 걸려 멈춘다.
	 * 그러니 자리를 사거리 경계 바로 안쪽에 두면 된다.
	 *
	 * 반대로 크게 잡으면 링이 대상 캡슐에 달라붙는다.
	 * 사거리 105 기준으로 이 값이 60 이면 링 반지름이 90 —
	 * 두 캡슐이 정확히 맞닿는 거리다. DetourCrowd 분리력이 밀어내는 자리와
	 * 겹쳐서, 유닛은 자리에 닿지 못하고 그 바깥에서 굳는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "0.0"))
	float SlotRangeMargin = 10.f;

	/**
	 * 슬롯으로 이동할 때 쓰는 도달 반경(cm). MoveAcceptanceRadius 와 별개다.
	 *
	 * 작게 잡는다. 슬롯은 도착이 목적이 아니라 방향 지시라서,
	 * 도달 판정이 크면 사거리에 들기도 전에 "도착했다" 며 멈춰버린다.
	 * 유닛을 실제로 멈추는 것은 교전 판정이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "5.0"))
	float SlotAcceptanceRadius = 15.f;

	/**
	 * 슬롯을 쓰고, NavMesh 대신 직접 조향(SteerTowardSlot)하기 시작하는 거리.
	 * 교전 사거리에 이만큼 더한 안쪽에서만 적용된다.
	 *
	 * 이 구간에서는 AIController 에게 MoveTo 를 넘기지 않는다. 여러 유닛이
	 * 한 대상에 몰릴 때 DetourCrowd 회피가 정체되는 문제가 반복돼서,
	 * 붙는 마지막 구간만 직접 계산으로 바꿨다.
	 *
	 * 멀리서는 그대로 액터 추적(RequestMoveToActor)에 맡긴다. 목표가 계속
	 * 움직이는 장거리 추격에서는 경로 추종이 알아서 따라가는 쪽이 낫고,
	 * 포위 모양은 어차피 붙어서 싸울 때 보이는 것이라 먼 거리는 상관없다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "0.0"))
	float SlotActivationRange = 250.f;

	/** 근접 조향에서 슬롯(또는 접근 지점)으로 당기는 힘의 가중치 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "0.0"))
	float SeekWeight = 1.f;

	/**
	 * 근접 조향 중 앞을 살피는 거리(cm). 0 이면 지형 회피를 끈다.
	 *
	 * SteerTowardSlot 은 NavMesh 를 쓰지 않고 AddMovementInput 만 한다.
	 * 평지에서는 문제가 없지만 벽·바위·건물이 파먹은 NavMesh 구멍 앞에서는
	 * 유닛이 지형을 밀며 갈린다. 진행 방향으로 한 번 스윕해서 막혀 있으면
	 * 벽면을 따라 미끄러지도록 방향을 튼다.
	 *
	 * 캡슐 반지름(45)보다 충분히 커야 부딪히기 전에 감지된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "0.0"))
	float WallProbeDistance = 120.f;

	/**
	 * 근접 조향에서 근처 아군에게 밀려나는 힘의 가중치.
	 *
	 * 추격력보다 세게 주면(1.0 초과) 자리를 못 잡아도 서로 밀어내며
	 * 자연스럽게 벌어진다. 너무 세면 대상에 붙지 못하고 겉돈다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "0.0"))
	float SeparationWeight = 1.5f;

	/**
	 * 분리력이 미치는 반경(cm). 캡슐 지름(45 × 2 = 90)과 같게 잡는 게 기본이다.
	 * 이보다 넓히면 아직 안 붙었는데도 서로 밀어내 대형이 헐거워진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "1.0"))
	float SeparationRadius = 90.f;

	/**
	 * 근접 조향 방향이 안정되는 속도.
	 *
	 * 추격력·분리력의 합이 매 틱 조금씩 흔들려도, 이 보간 때문에 실제
	 * 이동 방향은 부드럽게 따라간다. 낮추면 더 매끄럽지만 반응이 늦어진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "0.1"))
	float SteerInterpSpeed = 8.f;

	/**
	 * 근접 조향의 분리력에 주는 회전 각도(도).
	 *
	 * 분리력을 추격력과 정반대 방향으로만 밀면, 앞을 막은 아군과 정면으로
	 * 힘겨루기를 하다가 합력이 0에 가까워져 멈춰 선다 — 이 경우 값을 아무리
	 * 조절해도 "방향이 정반대"라는 사실 자체는 바뀌지 않으니 해결되지 않는다.
	 *
	 * 분리력을 이 각도만큼 항상 같은 방향으로 틀어서 적용하면, 정면으로
	 * 마주친 두 유닛이 서로 반대쪽으로 감아 돌며 지나가는 흐름이 생긴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "0.0", ClampMax = "89.0"))
	float SeparationDeflectionAngle = 35.f;

	/** 근접 조향 중 "정체" 로 판단하는 속도(cm/s) 상한 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "0.0"))
	float CloseQuartersStuckSpeed = 30.f;

	/** 위 속도 이하가 이만큼(초) 이어지면 옆으로 미끄러지는 탈출력을 더한다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "0.05"))
	float CloseQuartersStuckDuration = 0.6f;

	/**
	 * 정체 탈출력의 가중치. 추격력·분리력과 같은 저울에 더해진다.
	 *
	 * 회전 바이어스만으로는 못 뚫는 극단적인 밀집(사방이 아군으로 막힌 경우)을
	 * 위한 마지막 안전장치다 — 평소에는 0으로 유지되다가 정체가 감지될 때만
	 * 붙는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "0.0"))
	float StuckEscapeWeight = 1.5f;

	/**
	 * 슬롯이 가득 찬 대상의 거리 점수 배율.
	 *
	 * 이게 없으면 여덟 기가 한 마리에게 전부 몰려, 자리를 못 잡은 뒤쪽 유닛은
	 * 공격은 못 하고 걷기만 한다. 클수록 다른 적으로 흩어진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "1.0"))
	float OvercrowdedTargetPenalty = 2.f;

	/** 한 번의 탐색에서 시야 검사를 수행할 최대 후보 수 (성능 상한) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "1"))
	int32 MaxLineOfSightChecks = 6;

#pragma endregion

#pragma region Lane Corridor

	// 전투를 길(ALanePath 의 코리도) 위에 붙들어 두기 위한 값들이다.
	//
	// 최단경로만 보면 유닛은 길을 벗어나 잔디를 가로지른다. 지형이 그렇게
	// 생겼으니 당연한 결과이고, NavMesh 비용을 만져도 근본이 바뀌지 않는다 —
	// 길 밖에 적이 서 있으면 결국 그리로 간다.
	//
	// 그래서 힘이 아니라 목적지를 막는다. 코리도 밖 적을 자동 탐색 후보에서
	// 빼고, 진격은 스플라인을 따라가게 한다. 조향(SteerTowardSlot)에는 손대지
	// 않는다 — 거기에 "길 안으로 미는 힘" 을 더하면 경계에서 추격력과 정확히
	// 반대가 되어 합력이 0 이 된다. 분리력에서 이미 겪은 그 문제다.

	/**
	 * 코리도 밖 적을 자동으로 쫓지 않는다. 기본은 켬.
	 *
	 * "이동" 만 막고 "공격" 은 막지 않는다. 사거리 안에 들어온 적은 길 밖에
	 * 있어도 평소대로 때린다 — 길 가장자리 교전이 한쪽만 일방적으로 맞는
	 * 그림이 되지 않게 하기 위해서다. 제자리 고수(H)와 같은 규칙이다.
	 *
	 * 건물은 이 제한에서 빠진다. 적 핵심 건물이 길 밖에 있으면 진격 자체가
	 * 멈춰버리기 때문이다.
	 *
	 * 플레이어 명령(이동·공격 이동·대상 지정)은 전부 이 제한을 통과한다.
	 * 길 밖에서 싸우고 싶으면 직접 찍으면 된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lane", meta = (AllowPrivateAccess = true))
	uint8 bRestrictAutoChaseToLane : 1;

	/**
	 * 자동 진격 시 코리도를 따라갈 것인가. 기본은 켬.
	 *
	 * 끄면 예전처럼 적 핵심 건물을 향해 NavMesh 최단경로로 간다.
	 * 그 경로가 잔디를 가로지르는 것이 원래 문제였다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lane", meta = (AllowPrivateAccess = true))
	uint8 bFollowLaneWhileAdvancing : 1;

	/**
	 * 코리도를 벗어났다고 볼 여유 거리(cm).
	 *
	 * 이탈 판정과 복귀 완료 판정을 다른 값으로 둔다 — 벗어남은 반폭 + 이 값을
	 * 넘을 때, 복귀 완료는 반폭 안으로 들어왔을 때다.
	 *
	 * 같은 값 하나로 판정하면 경계에서 "이탈 → 복귀 → 진격 → 이탈" 을 반복하며
	 * 그 자리에서 덜컥거린다. 교전 사거리에 RangeHysteresis 를 둔 것과 같은 이유다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lane", meta = (AllowPrivateAccess = true, ClampMin = "0.0"))
	float LaneExitTolerance = 120.f;

	/**
	 * 코리도를 따라갈 때 몇 cm 앞을 보고 갈 것인가.
	 *
	 * 스플라인 위 "내 위치의 조금 앞" 을 매번 다시 계산해 목표로 삼는다.
	 * 웨이포인트를 하나씩 소비하는 방식과 달리 진행도가 어긋날 일이 없고,
	 * 한 지점에 못 닿아 굳는 상태도 생기지 않는다.
	 *
	 * 짧게 잡으면 길을 정확히 따라가지만 코너에서 자주 방향을 바꿔 어색하고,
	 * 길게 잡으면 부드러운 대신 코너를 가로질러 잔디를 밟는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lane", meta = (AllowPrivateAccess = true, ClampMin = "100.0"))
	float LaneLookAhead = 600.f;

#pragma endregion

#pragma region State

	/**
	 * 공격이 성립한 순간 재생할 이펙트.
	 * 타겟 위치에 스폰된다. 비워두면 아무것도 나오지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect", meta = (AllowPrivateAccess = true))
	TObjectPtr<UNiagaraSystem> AttackEffect;

	/**
	 * 공격 시 재생할 몽타주.
	 *
	 * AnimBP 의 AnimGraph 에 Slot 노드(기본 'DefaultSlot')가 있어야
	 * 화면에 나온다. 비워두면 아무것도 재생하지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = true))
	TObjectPtr<UAnimMontage> AttackMontage;

	/**
	 * 공격 몽타주를 끊을 때의 블렌드 아웃 시간(초).
	 *
	 * 0 이면 즉시 끊겨 동작이 툭 끊긴 느낌이 난다.
	 * 0.15 정도면 자연스럽게 이동 자세로 넘어간다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = true, ClampMin = "0.0"))
	float MontageBlendOutTime = 0.15f;

	/** 사망 시 재생할 몽타주. 없으면 AnimBP 의 사망 상태에 맡긴다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = true))
	TObjectPtr<UAnimMontage> DeathMontage;

	/** 사망 순간 재생할 이펙트. 자기 위치에 스폰된다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect", meta = (AllowPrivateAccess = true))
	TObjectPtr<UNiagaraSystem> DeathEffect;

	/** 이펙트 스폰 높이 보정. 발밑이 아니라 몸통 높이에서 나오게 한다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect", meta = (AllowPrivateAccess = true))
	float EffectHeightOffset = 60.f;

	/** 사망 후 액터가 남아 있는 시간(초). 0 이면 즉시 소멸 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = true, ClampMin = "0.0"))
	float DeathLifeSpan = 2.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = true))
	TObjectPtr<ALanePath> AssignedLane;

	/** 라인 진행도 0=아군 진영, 1=적군 진영 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = true))
	float LaneAlpha = 0.f;

	/** 켜면 타겟 라인과 교전 반경, 상태를 그린다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (AllowPrivateAccess = true))
	uint8 bDrawCombatDebug : 1;

#pragma endregion

	/** InitStat 이 이미 호출됐는가. BeginPlay 의 중복 주입을 막는다 */
	uint8 bStatInitialized : 1;

	/** "적이 없어 정지" 로그를 한 번만 찍기 위한 플래그 */
	uint8 bNoTargetReported : 1;

	/** 진격 목표 로그를 한 번만 찍기 위한 플래그 */
	uint8 bAdvanceTargetReported : 1;

	/**
	 * 명령이 없을 때의 행동. 런타임 값이다.
	 *
	 * 디테일 패널에서 직접 고치지 말 것 — BeginPlay 의
	 * ResolveAdvanceModeFromTeam 이 팀 기준으로 덮어쓴다.
	 * 바꾸려면 아래 AutoAdvanceTeams 를 고치거나 SetAdvanceMode 를 부른다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true))
	EUnitAdvanceMode AdvanceMode = EUnitAdvanceMode::HoldPosition;

	/**
	 * 스스로 진격하는 팀 번호 목록. 기본값은 { 1 }.
	 *
	 * 코드에 "아군이면" 같은 분기를 두지 않기 위한 장치다.
	 * 유닛은 자기 팀이 이 목록에 있는지만 본다. 아군·적군이 같은 클래스와
	 * 같은 BP 를 공유해도, 각자 자기 TeamIndex 로 갈린다.
	 *
	 *   0번 팀(플레이어) → 목록에 없음 → 제자리 유지, 직접 조작
	 *   1번 팀(AI)      → 목록에 있음 → 핵심 건물로 자동 진격
	 *
	 * 3인 이상 대전이나 진영 교체가 생기면 이 목록만 고치면 된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true))
	TSet<int32> AutoAdvanceTeams;

	/** SetAdvanceMode 로 직접 주입받았는가. 켜져 있으면 팀 기준 판정을 하지 않는다 */
	uint8 bAdvanceModeInjected : 1;

	/**
	 * 전투가 끝나면 원래 자리로 돌아갈 것인가. 기본은 끔.
	 *
	 * 끄면(기본) 유닛은 싸움이 끝난 그 자리에 그대로 선다.
	 * 이동 명령을 내린 지점은 "그때 가라고 한 곳" 일 뿐이고,
	 * 전투로 자리를 옮긴 뒤에는 그곳이 새 대기 지점이 된다.
	 *
	 * 켜면 아래 LeashRadius 를 벗어났을 때 명령 지점으로 복귀한다.
	 * 수비 위치를 엄격히 지켜야 하는 유닛에 쓴다.
	 *
	 * 절충안은 없다. 복귀를 켜지 않으면 추격 거리를 제한할 방법도 없다 —
	 * 목줄만 걸고 복귀를 안 시키면, 유닛이 경계에서 대상을 놓았다 물었다
	 * 반복하며 그 자리에서 덜컥거린다. 그래서 둘을 한 스위치로 묶었다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true))
	uint8 bReturnToHoldLocation : 1;

	/**
	 * 대기 지점에서 벗어날 수 있는 최대 거리(cm).
	 * bReturnToHoldLocation 이 켜져 있을 때만 의미가 있다.
	 *
	 * 이 거리를 넘으면 쫓던 대상을 놓고 대기 지점으로 돌아간다.
	 * 없으면 적을 물고 늘어지다 다음 적을 또 물면서 결국 적진까지 끌려간다.
	 *
	 * AggroRadius(1050) 보다 작게 두면 반경 끝의 적은 아예 쫓지 않는 셈이 된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "0.0"))
	float LeashRadius = 800.f;

	/** 제자리 유지 모드에서 돌아갈 자리 */
	FVector HoldLocation = FVector::ZeroVector;

	/** 위 자리가 정해졌는가 */
	uint8 bHoldLocationSet : 1;

	/**
	 * 목줄을 벗어나 복귀하는 중인가.
	 *
	 * 복귀 중에는 새 적을 물지 않는다. 경계선에서 놓았다 물었다를 반복하면
	 * 유닛이 그 자리에서 왔다 갔다 한다.
	 */
	uint8 bReturningToHold : 1;

	/** "제자리 유지" 로그를 한 번만 찍기 위한 플래그 */
	uint8 bHoldReported : 1;

	/** 플레이어가 지정한 목적지 */
	FVector CommandedDestination = FVector::ZeroVector;

	/** 위 목적지가 유효한가 */
	uint8 bHasCommandedDestination : 1;

	/**
	 * 위 이동 명령이 공격 이동인가.
	 *
	 * 꺼져 있으면 강제 이동 — Tick 이 교전 판정 자체를 건너뛴다.
	 * 켜져 있으면 평소대로 적을 찾고, 적이 없을 때만 목적지로 향한다.
	 */
	uint8 bCommandIsAttackMove : 1;

	/**
	 * 제자리 고수 중인가.
	 *
	 * 켜져 있으면 어떤 이유로도 스스로 움직이지 않는다.
	 * 진격 모드와 추격 모두 건너뛴다. 사거리 안의 적만 친다.
	 */
	uint8 bHoldGround : 1;

	/**
	 * 플레이어가 지정한 강제 공격 대상.
	 *
	 * 유효한 동안 타겟 재탐색을 건너뛴다. 타겟 고정력(TargetStickiness)이
	 * 점수 배율인 것과 달리 이쪽은 완전 고정이다.
	 */
	UPROPERTY(Transient)
	TObjectPtr<AActor> ForcedTarget;

	/** "사거리에 도달 불가" 로그를 한 번만 찍기 위한 플래그 */
	uint8 bRangeUnreachableReported : 1;

	/** "명령을 받지 않는 유닛" 로그를 한 번만 찍기 위한 플래그 */
	uint8 bCommandRejectedReported : 1;

	/** "사거리 안인데 시야가 막힘" 로그를 한 번만 찍기 위한 플래그 */
	uint8 bSightBlockedReported : 1;

	/**
	 * 대상 주위에서 배정받은 슬롯 각도 인덱스. INDEX_NONE 이면 미배정.
	 *
	 * 미배정 상태에서는 예전처럼 대상에게 직접 접근한다.
	 */
	int32 AttackSlotIndex = INDEX_NONE;

	/**
	 * 자리를 받을 때의 링 세대.
	 *
	 * 대상의 팀이 바뀌면 링이 통째로 버려지는데, 그때 이 유닛은 여전히 유효한
	 * 인덱스를 쥐고 있다. 세대를 함께 보지 않으면 UpdateAttackSlot 의
	 * "이미 자리가 있다" 조기 탈출에 영원히 걸려 다시 잡지 못한다.
	 */
	uint32 SlotGeneration = 0;

	/** 슬롯을 잡아둔 대상. CurrentTarget 과 어긋나면 반납하고 다시 잡는다 */
	UPROPERTY(Transient)
	TObjectPtr<AActor> SlotTarget;

	float AttackCooldownRemaining = 0.f;

	/** 반격 대상을 유지할 남은 시간(초). 이 동안 주기 재탐색을 억제한다 */
	float RetaliateHoldRemaining = 0.f;

	/**
	 * 공격 모션을 끊지 않고 보장할 남은 시간(초).
	 *
	 * 대상이 쓰러지는 순간 모션을 바로 끊으면, 정작 그 대상을 죽인 스윙이
	 * 한 프레임 만에 잘려 때리지도 않고 죽은 것처럼 보인다.
	 * 반대로 그냥 두면 쿨다운보다 긴 몽타주가 허공에서 계속 재생된다.
	 *
	 * 그래서 min(몽타주 길이, AttackCooldown) 만큼만 보장하고,
	 * 만료된 뒤에는 교전 상태가 아닌 한 잘라낸다.
	 */
	float AttackAnimHoldRemaining = 0.f;

	float RetargetTimer = 0.f;
	float StuckTimer = 0.f;

	/**
	 * 근접 조향(SteerTowardSlot) 전용 정체 타이머.
	 * 실제 속도가 CloseQuartersStuckSpeed 이하로 이어진 시간을 잰다.
	 * 대상이 바뀌거나 교전(Attacking) 상태에 들어가면 0으로 되돌린다.
	 */
	float CloseQuartersStuckTimer = 0.f;

	/** 명령 목적지 마지막 구간에서 제자리인 시간(초). 도착 안전장치용 */
	float CommandStallTimer = 0.f;

	/** 근접 조향의 현재 방향. 매 틱 목표 방향으로 보간해서 흔들림을 줄인다 */
	FVector SmoothedSteerDirection = FVector::ZeroVector;

	/**
	 * 근접 조향의 이웃 목록 임시 버퍼.
	 *
	 * SteerTowardSlot 이 유닛당 매 틱 불리는데, 지역 배열을 쓰면 그때마다
	 * 힙 할당이 발생한다. 멤버로 올려 재사용한다.
	 *
	 * 채운 직후 같은 함수 안에서만 읽고 버리므로 리플렉션에 올리지 않는다.
	 * 프레임을 넘겨 들고 있지 않으니 GC 앵커도 필요 없다.
	 */
	TArray<AActor*> SteerNeighborScratch;

	/**
	 * 코리도를 벗어난 상태인가.
	 *
	 * LaneExitTolerance 를 넘어야 켜지고, 코리도 안(반폭 이내)으로 들어와야 꺼진다.
	 * 경계에서의 덜컥거림을 막는 히스테리시스다.
	 */
	uint8 bOffLane : 1;

	/**
	 * 코리도를 따라가는 방향. +1 이면 알파 증가(스플라인 끝 쪽), -1 이면 감소.
	 *
	 * 진격 목표(적 핵심 건물)가 스플라인의 어느 끝에 가까운지로 정한다.
	 * 팀 번호를 보지 않으므로 진영 분기가 아니다 — 같은 라인을 양쪽 팀이
	 * 서로 반대 방향으로 타고 올라온다.
	 */
	int32 LaneAdvanceDirection = 1;

	/** "코리도로 복귀한다" 로그를 한 번만 찍기 위한 플래그 */
	uint8 bOffLaneReported : 1;
};
