#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFBuildingStatWidget.generated.h"

class UImage;
class UOverlay;
class USFHealthBarWidget;
class UProgressBar;

UCLASS()
class STRIKEFORGE_API USFBuildingStatWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	// 건물은 그대로 이 함수에 바인딩하고, 내부에서 자식 위젯으로 넘긴다.
	UFUNCTION()
	void SetHealthBar(float InBeforeHP, float InCurrentHP, float InMaxHP);
	void SetHealthBarColor(const FLinearColor& InColor);
	
	void SetHeathBarVisible(bool InVisible);

	/** 진행도는 소비자가 생산 건물뿐이라 분리하지 않는다. */
	void SetProgressRatio(float InRatio);
	void SetProgressVisible(bool bInVisible);
	void SetProgressBarColor(const FLinearColor& InColor);
private:
	/** WBP 안에 USFHealthBarWidget 파생 BP를 같은 이름으로 배치한다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USFHealthBarWidget> HealthBarWidget;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ProgressBarBackground;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOverlay> ProgressBarOverlay;
	
	// 진행도 스로틀
	float LastProgressRatio = -1.f;
};
