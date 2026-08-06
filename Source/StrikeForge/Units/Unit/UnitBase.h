#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interfaces/Units/Damagable.h"
#include "Interfaces/Units/Selectable.h"
#include "InGame/SFSelectINfo/SFSelectable.h"
#include "Interfaces/Units/Targetable.h"
#include "Datas/Unit/UnitStatData.h"
#include "UnitBase.generated.h"

class UHealthComponent;
class UCombatTargetComponent;
class UWidgetComponent;
class AUnitAIController;
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
 * 자동 전투 유닛.
 *
 * 아군·적군이 이 클래스 하나를 공유한다.
 * 구분은 TeamIndex 값뿐이며 로직에는 어떤 진영 분기도 두지 않는다.
 * 체력은 UHealthComponent 가 전담한다.
 */
UCLASS()
class STRIKEFORGE_API AUnitBase : public ACharacter, public ISelectable, public ISFSelectable
{
	GENERATED_BODY()

public:
	AUnitBase();

	// ── ISelectable ─────────────────────────────────────
	virtual void OnSelected_Implementation() override {}
	virtual void OnDeSelected_Implementation() override {}

	// ── ISFSelectable ───────────────────────────────────
	/**
	 * HUD 선택 패널에 넘길 정보를 만든다.
	 *
	 * 런타임 값(체력·공격력·이동속도)은 컴포넌트와 현재 스탯에서,
	 * 표시용 값(초상화·설명)은 UI 테이블에서 가져온다.
	 * 두 테이블 모두 UnitRowName 을 키로 쓴다.
	 */
	virtual FSFSelectableInfo GetSelectableInfo_Implementation() override;

	/** 체력 변화를 구독해 선택 패널을 갱신하려면 이걸 쓴다 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Unit")
	UHealthComponent* GetHealthComponent() const { return HealthComponent; }

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

	/** 역할별 공격 우선순위에 따라 대상 탐색 */
	UFUNCTION(BlueprintCallable, Category = "SF|Combat")
	AActor* FindBestTarget();

	UFUNCTION(BlueprintCallable, Category = "SF|Combat")
	void PerformAttack(AActor* InTarget);

	/** 타겟까지 시야가 열려 있는가. 벽 너머 공격을 막는다 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Combat")
	bool HasLineOfSight(const AActor* InTarget) const;

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

	bool IsTargetUsable(const AActor* InTarget) const;

	/** 교전 대상이 없을 때의 행동 — 라인 전진 또는 적 핵심 방향으로 */
	void Advance();

	void SetCombatState(EUnitCombatState InNewState);

	void FaceTarget(float DeltaSeconds);

	AUnitAIController* GetUnitAI() const;

#pragma region Components

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHealthComponent> HealthComponent;

	/** 팀·분류·방어를 보유하며 전투 레지스트리에 자신을 등록한다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCombatTargetComponent> CombatTarget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> StatusWidget;

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

	/** 타겟 재탐색 주기(초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "0.05"))
	float RetargetInterval = 0.3f;

	/** 추격 중 이만큼(초) 제자리면 경로를 다시 계산한다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "0.1"))
	float StuckThreshold = 0.6f;

	/** 공격에 시야가 필요한가. 끄면 벽 너머로도 때린다 (곡사 유닛) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true))
	uint8 bRequireLineOfSight : 1;

	/**
	 * 시야 검사에 쓰는 구체 반지름(cm).
	 *
	 * 0 에 가까우면 선 검사와 같아져 모서리를 스치는 순간 시야가 열린다.
	 * 캡슐 반지름에 가깝게 두면 몸이 실제로 코너를 빠져나와야 열린다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "0.0"))
	float LineOfSightRadius = 40.f;

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

	/** 한 번의 탐색에서 시야 검사를 수행할 최대 후보 수 (성능 상한) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = true, ClampMin = "1"))
	int32 MaxLineOfSightChecks = 6;

#pragma endregion

#pragma region State

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

	float AttackCooldownRemaining = 0.f;
	float RetargetTimer = 0.f;
	float StuckTimer = 0.f;
};
