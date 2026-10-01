#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFProgressBarWidget.generated.h"

class UImage;
class UTextBlock;
class UProgressBar;

/**
 * 생산·채굴 진행도 바 한 줄.
 *
 * 자기가 그릴 값만 받는다. 변화 판단은 판(USFOverheadPanelWidget)이 하므로
 * 여기에는 이전 값 캐시가 없다.
 */
UCLASS()
class STRIKEFORGE_API USFProgressBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetRatio(float InRatio);

	/** 채워진 부분과 배경을 같이 맞춘다. 색이 실제로 바뀔 때만 불린다. */
	void SetColor(const FLinearColor& InColor);

	/** 적용 중인 생산 시너지를 +50% 형식으로 표시하며, 0이면 숨긴다. */
	void SetSynergyBonusPercent(float InBonusPercent);

private:
	/** Widget Blueprint에서 같은 이름의 TextBlock과 연결된다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SynergyAmountText;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> ProgressBar;

	/** 채워지지 않은 구간의 배경. 없어도 동작한다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ProgressBarBackground;
};
