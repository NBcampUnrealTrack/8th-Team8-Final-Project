#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "ActorComponent/UI/SFOverheadStatusComponent.h"
#include "Datas/Building/OverHeadUI/SFOverheadUISettings.h"
#include "SFOverheadUIComponent.generated.h"

class UMeshComponent;
class USFOverheadPanelWidget;

/**
 * 액터 머리 위(또는 발밑)에 판 하나를 띄운다. 액터당 하나면 된다.
 *
 * 판에 무엇이 들어 있는지는 모른다. 배치·가시성·줌 반영만 담당한다.
 * 요소를 늘릴 때는 이 컴포넌트를 더 다는 게 아니라 ESFOverheadElement와 판 위젯에 추가한다.
 *
 * 띄울 위젯은 부모의 WidgetClass(디테일 패널 User Interface 항목)로 지정한다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STRIKEFORGE_API USFOverheadUIComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	USFOverheadUIComponent();

	/** 소유자가 BeginPlay에서 한 번 호출한다. 크기 계산에 쓸 메시를 넘긴다. */
	void Initialize(UMeshComponent* InMeshComponent);

	void ApplySettings(const FSFOverheadUISettings& InSettings);

	/** 메시가 바뀌었을 때 호출한다. */
	void RefreshTransform();

	/**
	 * 요소 하나를 켜고 끈다. 조건 해석은 소유자가 한다.
	 *
	 * 하나라도 켜져 있으면 판이 보이고, 전부 꺼지면 판이 접힌다.
	 * 접히면 부모가 스스로 틱을 멈춘다.
	 */
	void SetElementVisible(ESFOverheadElement InElement, bool bInVisible);

	/** 프리뷰·사망처럼 조건과 무관하게 막아야 하는 상태. */
	void SetSuppressed(bool bInSuppressed);

	/**
	 * 자원 획득 배지를 띄운다. 애니메이션이 끝나면 스스로 꺼진다.
	 *
	 * 켜기와 재생을 따로 부르면 순서를 틀릴 수 있어 여기서 한 번에 처리한다.
	 */
	void PlayResourceGain(EResourceType InResourceType, int32 InAmount);

	template<typename T>
	T* GetWidget() const
	{
		return Cast<T>(GetUserWidgetObject());
	}

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void RefreshVisibility();
	void ApplyUIScale();

	/** 배율이 바뀌었다는 신호만 받는다. 값은 ApplyUIScale()이 직접 읽는다. */
	void HandleUIScaleChanged();

	/** 상태 컴포넌트의 신호를 받아 판에 넘긴다. 판의 내용은 알 필요가 없다. */
	void HandleStatusChanged(const FSFOverheadStatus& InStatus);

	/** 일회성 요소가 끝났다는 판의 신호. 비트를 내린다. */
	void HandleElementFinished(ESFOverheadElement InElement);

	/** 위젯이 만들어지거나 설정이 바뀐 뒤, 판 내부 상태(요소 표시·간격)를 한 번에 맞춘다. */
	void PushPanelState();

	/**
	 * 기본 배치값. 소유자가 ApplySettings()를 호출하면 덮어쓴다.
	 * 건물은 Definition이 덮어쓰므로 여기 값은 쓰이지 않는다.
	 * DA가 없는 유닛 등이 이 값을 쓴다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Overhead")
	FSFOverheadUISettings Settings;

	UPROPERTY(Transient)
	TObjectPtr<UMeshComponent> MeshComponent;

	/** BeginPlay에서 한 번 찾아 둔다. EndPlay에서 다시 찾지 않기 위해. */
	UPROPERTY(Transient)
	TObjectPtr<USFOverheadStatusComponent> StatusComponent;

	/** 켜져 있는 요소 비트. 0이면 판을 접는다. */
	uint8 ElementMask = 0;

	bool bSuppressed = false;

	/** 메시 윗면에서 판을 띄우는 화면 오프셋. RefreshTransform()이 갱신한다. */
	float ComputedScreenOffsetY = 0.f;
	FDelegateHandle UIScaleHandle;
	FDelegateHandle StatusHandle;
	FDelegateHandle ElementFinishedHandle;
};
