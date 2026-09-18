#pragma once

#include "CoreMinimal.h"
#include "Buildings/UI/SFOverheadBarWidget.h"
#include "SFProgressBarWidget.generated.h"

class UImage;
class UProgressBar;

UCLASS()
class STRIKEFORGE_API USFProgressBarWidget : public USFOverheadBarWidget
{
	GENERATED_BODY()

public:
	virtual void ApplyStatus(const FSFOverheadStatus& InStatus) override;

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> ProgressBar;

	/** 채워지지 않은 구간의 배경. 없어도 동작한다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ProgressBarBackground;

	float LastRatio = -1.f;
	FLinearColor LastColor = FLinearColor::Transparent;
};
