#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFHealthBarWidget.generated.h"

class UProgressBar;

UCLASS()
class STRIKEFORGE_API USFHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UFUNCTION()
	void SetHealthBar(float InBeforeHP, float InCurrentHP, float InMaxHP);
	void SetHealthBarColor(const FLinearColor& InColor);

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;
	
};
