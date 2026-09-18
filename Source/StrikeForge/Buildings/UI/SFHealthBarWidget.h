#pragma once

#include "CoreMinimal.h"
#include "Buildings/UI/SFOverheadBarWidget.h"
#include "SFHealthBarWidget.generated.h"

class UProgressBar;

UCLASS()
class STRIKEFORGE_API USFHealthBarWidget : public USFOverheadBarWidget
{
	GENERATED_BODY()

public:
	virtual void ApplyStatus(const FSFOverheadStatus& InStatus) override;

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;

	float LastRatio = -1.f;
	FLinearColor LastColor = FLinearColor::Transparent;
};
