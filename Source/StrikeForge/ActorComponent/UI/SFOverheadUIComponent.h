#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SFOverheadUIComponent.generated.h"

class UMeshComponent;
class USFBuildingStatWidget;
class UWidgetComponent;

/** 소유자가 넘겨주는 표시 설정. Definition이든 BP 기본값이든 출처는 상관없다. */
USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFOverheadUISettings
{
	GENERATED_BODY()

	/** 앵커 높이. 액터 로컬 Z. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overhead")
	float AnchorZ = 200.f;

	/** 바 가로 길이 = 메시 폭 × 이 값. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overhead", meta = (ClampMin = "0.01"))
	float WidthRatio = 0.7f;

	/** 가로 길이의 하한(X)·상한(Y). 픽셀. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Overhead")
	FVector2D WidthRange = FVector2D(50.f, 200.f);
};

/**
 * 액터 머리 위 체력·진행도 표시를 담당한다.
 *
 * 소유자는 "언제 보여줄지"만 알려준다.
 * 위치·크기 계산, 표시 조건 합성, 카메라 줌 반영은 전부 이 컴포넌트가 한다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STRIKEFORGE_API USFOverheadUIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USFOverheadUIComponent();

	/** 소유자가 BeginPlay에서 한 번 호출한다. */
	void Initialize(UWidgetComponent* InWidgetComponent, UMeshComponent* InMeshComponent);

	/** 표시 설정을 적용하고 위치·크기를 다시 계산한다. */
	void ApplySettings(const FSFOverheadUISettings& InSettings);

	/** 메시가 바뀌었을 때 호출한다. */
	void RefreshTransform();

	// ── 표시 조건 ──
	void SetHovered(bool bInHovered);
	void SetProgressActive(bool bInActive);
	/** 프리뷰 고스트처럼 아예 표시하면 안 되는 상태. */
	void SetSuppressed(bool bInSuppressed);
	/** 켜면 오버나 작업 여부와 무관하게 항상 보인다. 유닛처럼 상시 표시가 필요한 대상용. */
	void SetAlwaysVisible(bool bInAlways);
	
	// ── 값 전달 ──
	void SetHealthBar(float InBeforeHP, float InCurrentHP, float InMaxHP);
	void SetHealthBarColor(const FLinearColor& InColor);
	void SetProgressRatio(float InRatio);
	void SetProgressBarColor(const FLinearColor& InColor);

	USFBuildingStatWidget* GetWidget() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void RefreshVisibility();
	void ApplyUIScale();
	void HandleUIScaleChanged(float InScale);

	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> WidgetComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMeshComponent> MeshComponent;

	/** 매번 Cast하지 않도록 캐시한다. */
	mutable TWeakObjectPtr<USFBuildingStatWidget> CachedWidget;
	
	/** 소유자가 ApplySettings()로 덮어쓰지 않으면 이 값을 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category = "Overhead")
	FSFOverheadUISettings Settings;

	/** 체크하면 마우스 오버 없이도 항상 표시한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Overhead")
	bool bAlwaysVisible = false;

	bool bHovered = false;
	bool bProgressActive = false;
	bool bSuppressed = false;

	FDelegateHandle UIScaleHandle;
};

inline void USFOverheadUIComponent::SetAlwaysVisible(bool bInAlways)
{
	if (bAlwaysVisible == bInAlways)	return;

	bAlwaysVisible = bInAlways;
	RefreshVisibility();
}
