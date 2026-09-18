#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActorComponent/UI/SFOverheadStatusComponent.h"
#include "Datas/Building/OverHeadUI/SFOverheadUISettings.h"
#include "SFOverheadUIComponent.generated.h"

class UMeshComponent;
class USFOverheadBarWidget;
class UUserWidget;
class UWidgetComponent;


/**
 * 액터 머리 위(또는 발밑)에 위젯 하나를 띄운다.
 *
 * 위젯의 내용은 모른다. 배치·가시성·줌 반영만 담당한다.
 * 체력바와 진행도 바처럼 위치가 달라야 하면 컴포넌트를 두 개 단다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STRIKEFORGE_API USFOverheadUIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USFOverheadUIComponent();

	/** 소유자가 BeginPlay에서 한 번 호출한다. 크기 계산에 쓸 메시를 넘긴다. */
	void Initialize(UMeshComponent* InMeshComponent);
	
	void ApplySettings(const FSFOverheadUISettings& InSettings);

	/** 메시가 바뀌었을 때 호출한다. */
	void RefreshTransform();

	/** 이 바를 보여줄지. 조건 해석은 소유자가 한다. */
	void SetVisible(bool bInVisible);

	/** 프리뷰·사망처럼 조건과 무관하게 막아야 하는 상태. */
	void SetSuppressed(bool bInSuppressed);

	UUserWidget* GetUserWidget() const;

	template<typename T>
	T* GetWidget() const
	{
		return Cast<T>(GetUserWidget());
	}

	/** 이 바가 띄울 위젯. 비어 있으면 아무것도 만들지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Overhead")
	TSubclassOf<USFOverheadBarWidget> WidgetClass;
	
protected:
	virtual void BeginPlay() override;
	void CreateWidgetComponent();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void RefreshVisibility();
	void ApplyUIScale();
	void HandleUIScaleChanged(float InScale);

	/** 상태 컴포넌트의 신호를 받아 위젯에 넘긴다. 위젯 타입은 알 필요가 없다. */
	void HandleStatusChanged(const FSFOverheadStatus& InStatus);

	/** 체크하면 소유자가 켜지 않아도 항상 보인다. 유닛 체력바처럼 상시 표시용. */
	UPROPERTY(EditDefaultsOnly, Category = "Overhead")
	bool bAlwaysVisible = false;

	/**
	 * 기본 배치값. 소유자가 ApplySettings()를 호출하면 덮어쓴다.
	 * 건물은 Definition이 덮어쓰므로 여기 값은 쓰이지 않는다.
	 * DA가 없는 유닛 등이 이 값을 쓴다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Overhead")
	FSFOverheadUISettings Settings;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> WidgetComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMeshComponent> MeshComponent;

	mutable TWeakObjectPtr<UUserWidget> CachedWidget;

	bool bVisible = false;
	bool bSuppressed = false;

	/** 메시 높이에서 계산한 화면 세로 오프셋. RefreshTransform()이 갱신한다. */
	float ComputedScreenOffsetY = 0.f;

	FDelegateHandle UIScaleHandle;
	FDelegateHandle StatusHandle;
};
