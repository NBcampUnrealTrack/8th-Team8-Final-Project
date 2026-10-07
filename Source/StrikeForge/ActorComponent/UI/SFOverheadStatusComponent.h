#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "Datas/Building/OverHeadUI/SFOverheadUISettings.h"
#include "SFOverheadStatusComponent.generated.h"

class UHealthComponent;
class UMeshComponent;

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

/** 머리 위 상태 아이콘의 종류. 무엇으로 그릴지는 아이콘 위젯이 정한다. */
UENUM(BlueprintType)
enum class ESFOverheadStatusIcon : uint8
{
	None,

	/** 완성된 생산 건물의 대기열이 비었다. */
	ProductionIdle,

	/** 미네랄 광산이 캘 수 있는 자원을 모두 소진했다. */
	MineralDepleted,

	/** 에테르 광산이 캘 수 있는 자원을 모두 소진했다. */
	EtherDepleted,
};

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

	/** 현재 건물에 실제 적용 중인 생산 시너지 증가율입니다. */
	UPROPERTY(BlueprintReadOnly, Category = "Overhead")
	float SynergyBonusPercent = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Overhead")
	ESFOverheadStatusIcon StatusIcon = ESFOverheadStatusIcon::None;
};

/**
 * 판을 화면 어디에 얼마만 한 크기로 놓을지.
 *
 * 메시와 배치 설정에서 계산한다. 매니저는 이 값을 슬롯 크기·정렬과 렌더 이동에 그대로 쓴다.
 */
struct FSFOverheadLayout
{
	/**
	 * 액터 루트 기준 앵커 위치. 높이는 메시 중심, 앞뒤(액터 +Y)는 ForwardRatio만큼 옮긴 자리다.
	 * 건물 기준 좌표라 월드로 바꿀 때 액터 회전이 적용된다.
	 */
	FVector AnchorLocation = FVector::ZeroVector;

	/** 판 크기. 픽셀. */
	FVector2D DrawSize = FVector2D(100.f, 44.f);

	/** 줌 배율을 곱하기 전의 화면 세로 오프셋. 픽셀. */
	float ScreenOffsetY = 0.f;

	FVector2D Pivot = FVector2D(0.5f, 0.5f);

	float ElementSpacing = 4.f;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FSFOnOverheadStatusChanged, const FSFOverheadStatus&);

/**
 * 머리 위 UI의 액터 쪽 유일한 창구.
 *
 * 무엇을 보여줄지(값·켜진 요소·억제)와 어디에 보여줄지(앵커·크기)를 모은다.
 * 위젯은 USFOverheadUISubsystem이 띄운다. 소유자는 이 컴포넌트만 호출한다.
 *
 * BeginPlay에서 매니저에 등록하고 EndPlay에서 해제한다. 액터가 사라지면 판도 함께 거둬진다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STRIKEFORGE_API USFOverheadStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USFOverheadStatusComponent();

	FORCEINLINE const FSFOverheadStatus& GetStatus() const { return Status; }
	FORCEINLINE const FSFOverheadLayout& GetLayout() const { return Layout; }

	/** 값이 실제로 바뀔 때만 호출된다. 매니저는 판이 떠 있는 동안만 구독한다. */
	FSFOnOverheadStatusChanged OnStatusChanged;

	// ── 값 ──

	/** 체력 출처를 연결한다. 소유자가 BeginPlay에서 한 번 호출한다. */
	void BindHealth(UHealthComponent* InHealthComponent);
	void UnbindHealth(UHealthComponent* InHealthComponent);

	void SetHealthColor(const FLinearColor& InColor);
	void SetProgressColor(const FLinearColor& InColor);
	void SetSynergyBonusPercent(float InBonusPercent);

	/** 상태 아이콘을 바꾼다. 아이콘 요소도 함께 켜고 끈다. None이면 끈다. */
	void SetStatusIcon(ESFOverheadStatusIcon InIcon);

	/** UProductComponent::OnProductRatioChanged 와 시그니처가 같다. */
	UFUNCTION()
	void HandleProgressRatio(float InRatio);

	/** UMiningComponent::OnExtractProgressRatioChanged 와 시그니처가 같다. */
	UFUNCTION()
	void HandleExtractProgressRatio(EResourceType InResourceType, float InRatio);

	// ── 표시 ──

	/** 판 크기·위치 계산에 쓸 메시. 소유자가 BeginPlay에서 한 번 넘긴다. */
	void SetAnchorMesh(UMeshComponent* InMeshComponent);

	void ApplySettings(const FSFOverheadUISettings& InSettings);

	/** 메시가 바뀌었을 때 호출한다. 판 크기와 앵커를 다시 계산한다. */
	void RefreshLayout();

	/**
	 * 요소 하나를 켜고 끈다. 조건 해석은 소유자가 한다.
	 * 하나라도 켜져 있으면 판이 뜨고, 전부 꺼지면 매니저가 위젯을 거둔다.
	 */
	void SetElementVisible(ESFOverheadElement InElement, bool bInVisible);

	/** 프리뷰·파괴처럼 조건과 무관하게 막아야 하는 상태. */
	void SetSuppressed(bool bInSuppressed);

	/**
	 * 자원 획득 배지를 띄운다. 애니메이션이 끝나면 매니저가 요소를 끈다.
	 * 켜기와 재생의 순서가 정해져 있어 매니저가 한 번에 처리한다.
	 */
	void PlayResourceGain(EResourceType InResourceType, int32 InAmount);

	/** 판을 띄워야 하는가. 요소가 하나라도 켜져 있고 억제되지 않았을 때다. */
	FORCEINLINE bool ShouldShowPanel() const { return !bSuppressed && ElementMask != 0; }
	FORCEINLINE uint8 GetElementMask() const { return ElementMask; }

	/** 판이 붙을 월드 위치. 매니저가 투영할 때 읽는다. */
	FVector GetAnchorWorldLocation() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleHealthChanged(float InBeforeHP, float InCurrentHP, float InMaxHP);

	void SetProgressRatio(float InRatio);
	void NotifyChanged();

	/** 켜진 요소·억제·배치가 바뀌었음을 매니저에 알린다. */
	void NotifyPresentationChanged();

	FSFOverheadStatus Status;

	/**
	 * 기본 배치값. 소유자가 ApplySettings()를 호출하면 덮어쓴다.
	 * 건물은 Definition이 덮어쓰므로 여기 값은 DA가 없는 액터가 쓴다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Overhead")
	FSFOverheadUISettings Settings;

	UPROPERTY(Transient)
	TObjectPtr<UMeshComponent> AnchorMesh;

	FSFOverheadLayout Layout;

	/** 켜져 있는 요소 비트. 0이면 판을 띄우지 않는다. */
	uint8 ElementMask = 0;

	bool bSuppressed = false;
};
