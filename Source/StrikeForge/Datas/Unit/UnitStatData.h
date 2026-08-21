#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "UnitStatData.generated.h"

/** 유닛 역할. 공격 우선순위 분기에 쓰인다 */
UENUM(BlueprintType)
enum class EUnitRole : uint8
{
	Defense		UMETA(DisplayName = "방어"),
	Charge		UMETA(DisplayName = "돌격"),
	Ranged		UMETA(DisplayName = "원거리"),
	Assassin	UMETA(DisplayName = "암살"),
	Splash		UMETA(DisplayName = "광역"),
	Support		UMETA(DisplayName = "지원")
};

/**
 * 유닛 밸런스 수치 한 행.
 *
 * EDataTableType::Unit 테이블의 행 구조체로 사용한다.
 * 밸런스 수치는 전부 여기 있고 C++ 에 하드코딩하지 않는다.
 * 스폰 시 AUnitBase::InitStat 이 유닛에 주입한다.
 */
USTRUCT(BlueprintType)
struct FUnitStatData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
	EUnitRole Role = EUnitRole::Defense;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat|Survival")
	float MaxHealth = 100.f;

	/** 받는 피해 감소 0~0.9 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat|Survival", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float Armor = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat|Attack")
	float Attack = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat|Attack")
	float AttackRange = 270.f;

	/** 공격 간격(초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat|Attack", meta = (ClampMin = "0.05"))
	float AttackCooldown = 1.f;

	/** 대상의 피해 감소를 상쇄하는 값 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat|Attack", meta = (ClampMin = "0.0"))
	float Penetration = 0.f;

	/** 0 이면 단일 대상 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat|Attack", meta = (ClampMin = "0.0"))
	float SplashRadius = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat|Move")
	float MoveSpeed = 400.f;

	/**
	 * 이 유닛이 차지하는 인구수.
	 *
	 * 생산 시 증가하고 사망 시 감소한다.
	 * 0 이면 인구를 차지하지 않는다(지휘관 등).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat|Population", meta = (ClampMin = "0"))
	int32 PopulationCost = 1;

	/**
	 * 이 행으로 생성할 유닛 블루프린트.
	 *
	 * 생산 시간·비용은 FUnitProductData(EDataTableType::Product)가 담당하므로
	 * 여기에는 두지 않는다. 이 테이블은 "무엇을 어떤 능력치로 만들 것인가"만 다룬다.
	 *
	 * 비워두면 호출부가 넘긴 기본 클래스를 사용한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	TSoftClassPtr<class AUnitBase> UnitClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Production")
	float UnitProductionDuration = 0.f;

	/** 생산 비용. 생산 큐를 거치지 않는 유닛은 기본값(0)으로 둔다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Production")
	FResources UnitProductionCost;
};
