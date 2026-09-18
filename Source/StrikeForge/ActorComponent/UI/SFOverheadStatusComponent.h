#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "SFOverheadStatusComponent.generated.h"

class UHealthComponent;

/**
 * 머리 위 UI가 쓰는 공통 색.
 *
 * 건물마다 달라질 값이 아니라 게임 전체 규칙이라 한 곳에 모은다.
 * 색을 바꾸려면 이 파일만 열면 된다.
 */
namespace SFOverheadUIColors
{
	inline constexpr FLinearColor AllyHealth  = FLinearColor(0.03f, 0.93f, 0.95f, 1.f);
	inline constexpr FLinearColor EnemyHealth = FLinearColor(0.95f, 0.15f, 0.15f, 1.f);

	inline constexpr FLinearColor Production  = FLinearColor(0.20f, 0.85f, 0.25f, 1.f);
	inline constexpr FLinearColor Mineral     = FLinearColor(0.9f, 0.9f, 0.1f, 1.f);
	inline constexpr FLinearColor Ether       = FLinearColor(0.1f, 0.1f, 0.95f, 1.f);

	/** 자원 종류에 대응하는 색. 진행도 바와 획득 배지가 같은 색을 쓴다. */
	inline FLinearColor FromResourceType(EResourceType InResourceType)
	{
		return InResourceType == EResourceType::Ether ? Ether : Mineral;
	}
}

/**
 * 액터 머리 위에 표시할 상태.
 *
 * 게임플레이 사실이 아니라 표시용으로 가공한 값이다.
 * 위젯은 이 구조체만 알면 되고, 값이 어느 컴포넌트에서 왔는지는 몰라도 된다.
 */
USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFOverheadStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Overhead")
	float HealthRatio = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Overhead")
	FLinearColor HealthColor = SFOverheadUIColors::AllyHealth;

	UPROPERTY(BlueprintReadOnly, Category = "Overhead")
	float ProgressRatio = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Overhead")
	FLinearColor ProgressColor = SFOverheadUIColors::Production;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FSFOnOverheadStatusChanged, const FSFOverheadStatus&);

/**
 * 머리 위 UI가 볼 상태를 한 곳에 모은다.
 *
 * 체력·진행도·색을 각각의 출처에서 받아 하나의 상태로 정규화하고, 변경 신호를 하나만 쏜다.
 * 소유자는 출처를 연결해 주기만 하고 값을 중계하지 않는다.
 *
 * 표시 여부는 여기서 정하지 않는다. 그건 소유자의 정책이고
 * USFOverheadUIComponent::SetVisible()이 담당한다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STRIKEFORGE_API USFOverheadStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USFOverheadStatusComponent();

	FORCEINLINE const FSFOverheadStatus& GetStatus() const { return Status; }

	/** 값이 실제로 바뀔 때만 호출된다. */
	FSFOnOverheadStatusChanged OnStatusChanged;

	/** 체력 출처를 연결한다. 소유자가 BeginPlay에서 한 번 호출한다. */
	void BindHealth(UHealthComponent* InHealthComponent);
	void UnbindHealth(UHealthComponent* InHealthComponent);

	void SetHealthColor(const FLinearColor& InColor);
	void SetProgressColor(const FLinearColor& InColor);

	// ── 진행도 출처에 직접 바인딩할 수 있는 형태로 열어둔다 ──

	/** UProductComponent::OnProductRatioChanged 와 시그니처가 같다. */
	UFUNCTION()
	void HandleProgressRatio(float InRatio);

	/** UMiningComponent::OnExtractProgressRatioChanged 와 시그니처가 같다. */
	UFUNCTION()
	void HandleExtractProgressRatio(EResourceType InResourceType, float InRatio);

private:
	UFUNCTION()
	void HandleHealthChanged(float InBeforeHP, float InCurrentHP, float InMaxHP);

	void SetProgressRatio(float InRatio);
	void NotifyChanged();

	FSFOverheadStatus Status;
};
