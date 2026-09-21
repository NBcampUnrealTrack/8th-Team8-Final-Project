#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ActorComponent/UI/SFOverheadStatusComponent.h"
#include "Datas/Building/OverHeadUI/SFOverheadUISettings.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "SFOverheadPanelWidget.generated.h"

class USFHealthBarWidget;
class USFProgressBarWidget;
class USFResourceGainWidget;
class USpacer;

/** 잠깐 떴다 사라지는 요소가 끝났을 때. 컴포넌트가 받아 비트를 내린다. */
DECLARE_MULTICAST_DELEGATE_OneParam(FSFOnOverheadElementFinished, ESFOverheadElement);

/**
 * 액터 머리 위에 뜨는 요소들을 담는 판.
 *
 * 컴포넌트는 이 판 하나만 띄운다. 요소가 늘어도 컴포넌트는 늘지 않는다.
 * 요소끼리의 세로 배치는 UMG 레이아웃이 정한다. 코드가 화면 오프셋을 계산하지 않는다.
 *
 * 상태 구조체가 도달하는 마지막 지점이기도 하다. 자식에게는 각자 쓰는 값만,
 * 그것도 실제로 바뀌었을 때만 넘긴다. 자식이 남의 필드를 보거나 남의 변화에 깨어나지 않게 한다.
 */
UCLASS()
class STRIKEFORGE_API USFOverheadPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 바뀐 필드를 골라 해당 자식에게만 넘긴다. */
	void ApplyStatus(const FSFOverheadStatus& InStatus);

	/** 요소 하나만 켜고 끈다. 판 전체의 표시 여부는 컴포넌트가 정한다. */
	void SetElementVisible(ESFOverheadElement InElement, bool bInVisible);

	/** 요소 사이를 얼마나 벌릴지. DA 값이 여기로 들어온다. */
	void SetElementSpacing(float InSpacing);

	/**
	 * 자원 획득 배지를 재생한다.
	 * 켜는 건 소유자가 하고, 끄는 건 애니메이션이 끝날 때 OnElementFinished로 알린다.
	 */
	void PlayResourceGain(EResourceType InResourceType, int32 InAmount);

	FSFOnOverheadElementFinished OnElementFinished;

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USFHealthBarWidget> HealthBarWidget;

	/** 진행도가 없는 건물(집·넥서스)의 판에는 없어도 된다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USFProgressBarWidget> ProgressBarWidget;

	/** 자원을 캐지 않는 건물의 판에는 없어도 된다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USFResourceGainWidget> ResourceGainWidget;

	/**
	 * 요소 사이에 두는 빈 칸. 없으면 간격 조절이 무시된다.
	 * 어디에 둘지는 UMG가 정하고, 크기는 DA가 정한다.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USpacer> ElementSpacer;

private:
	UUserWidget* FindElement(ESFOverheadElement InElement) const;

	void HandleResourceGainFinished();

	/** 직전에 받은 상태. 어느 필드가 바뀌었는지 가려내는 데만 쓴다. */
	FSFOverheadStatus LastStatus;

	/** 첫 호출은 비교 없이 전부 내려보낸다. 기본값과 우연히 같아도 그려져야 한다. */
	bool bAppliedOnce = false;
};
