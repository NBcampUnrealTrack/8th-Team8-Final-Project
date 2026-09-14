#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "UnitStatData.generated.h"

/**
 * 유닛에 무언가를 붙일 때 쓰는 소켓 + 상대 트랜스폼.
 *
 * 무기 메시든 이펙트든 붙이는 쪽은 이 네 값만 있으면 된다.
 * 유닛마다 골격과 덩치가 달라서 같은 소켓에 붙여도 위치가 어긋나므로,
 * 소켓 이름만으로는 부족하고 상대 보정값이 함께 있어야 한다.
 *
 * ── 값을 어떻게 채우는가 ────────────────────────────────
 *
 * SocketName 만 먼저 넣고 나머지는 기본값(0, 0, 1)으로 두고 붙여본다.
 * 어긋난 만큼만 Location / Rotation 으로 밀고, 크기가 안 맞으면 Scale 을
 * 건드린다. 처음부터 세 값을 다 채우려 들면 무엇 때문에 틀어졌는지
 * 알 수가 없다.
 */
USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFUnitAttachTransform
{
	GENERATED_BODY()

	/**
	 * 붙일 소켓 이름. 비워두면 메시 루트에 붙는다.
	 *
	 * 스켈레톤에 없는 이름을 넣으면 언리얼이 조용히 루트에 붙인다 —
	 * "왜 손이 아니라 발밑에 있지" 의 원인이 대개 여기 오타다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attach")
	FName SocketName = NAME_None;

	/** 소켓 기준 위치 보정(cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attach")
	FVector RelativeLocation = FVector::ZeroVector;

	/** 소켓 기준 회전 보정 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attach")
	FRotator RelativeRotation = FRotator::ZeroRotator;

	/** 소켓 기준 크기 배율. 1 이 원본 크기다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attach")
	FVector RelativeScale = FVector::OneVector;

	/**
	 * 세 값을 하나로 묶어 돌려준다.
	 *
	 * AttachToComponent 뒤에 SetRelativeTransform 으로 한 번에 넣으면
	 * 위치 / 회전 / 크기를 따로 넣다가 하나를 빠뜨리는 일이 없다.
	 */
	FTransform GetRelativeTransform() const
	{
		return FTransform(RelativeRotation, RelativeLocation, RelativeScale);
	}
};

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
	 * 무기·장비를 붙일 자리.
	 *
	 * 지금은 한 벌만 둔다. 한 유닛에 여러 개(오른손 무기 + 왼손 방패 등)를
	 * 붙여야 하면 배열로 바꾸면 되고, 그때 어느 자리인지 구분할 이름 칸만
	 * 하나 더 붙이면 된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attach")
	FSFUnitAttachTransform WeaponAttach;

	/**
	 * 이 행으로 생성할 유닛 블루프린트.
	 * 비워두면 호출부가 넘긴 기본 클래스를 사용한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	TSoftClassPtr<class AUnitBase> UnitClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Production")
	float UnitProductionDuration = 0.f;

	/** 생산 비용. 생산 큐를 거치지 않는 유닛은 기본값(0)으로 둔다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Production")
	FResources UnitProductionCost;

	/**
	 * 이 유닛을 선택했을 때 재생할 소리.
	 *
	 * 비워두면 소리가 나지 않는다 — 소리를 줄 유닛만 채우면 된다.
	 * 실제 로딩은 AUnitBase::InitStat 이 스폰 때 한 번만 한다.
	 * 선택할 때마다 로드하면 그 유닛을 처음 고르는 순간 히치가 생긴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Selection")
	TSoftObjectPtr<class USoundBase> SelectionSound;

	/**
	 * 이 유닛에게 이동 명령을 내렸을 때 재생할 소리.
	 *
	 * 비워두면 소리가 나지 않는다 — 소리를 줄 유닛만 채우면 된다.
	 * SelectionSound 와 마찬가지로 AUnitBase::InitStat 이 스폰 때 한 번만
	 * 로드한다. 명령할 때마다 로드하면 그 유닛에게 처음 명령하는 순간
	 * 히치가 생긴다.
	 *
	 * 여러 기를 한꺼번에 움직여도 한 기만 대답한다 — 간격은
	 * sf.CommandVoiceInterval 로 조절한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Command")
	TSoftObjectPtr<class USoundBase> MoveCommandSound;
};
