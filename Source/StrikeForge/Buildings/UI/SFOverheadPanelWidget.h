#pragma once

#include "CoreMinimal.h"
#include "Buildings/UI/SFOverheadBarWidget.h"
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
 *
 * 요소끼리의 세로 배치는 UMG 레이아웃(VerticalBox 등)이 정한다.
 * 코드가 요소별 화면 오프셋을 계산하지 않는다. 같은 판 위에 있으므로 정렬은 그냥 따라온다.
 */
UCLASS()
class STRIKEFORGE_API USFOverheadPanelWidget : public USFOverheadBarWidget
{
	GENERATED_BODY()

public:
	/** 자식 전부에게 상태를 넘긴다. 어느 필드를 쓸지는 자식이 정한다. */
	virtual void ApplyStatus(const FSFOverheadStatus& InStatus) override;

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
};
