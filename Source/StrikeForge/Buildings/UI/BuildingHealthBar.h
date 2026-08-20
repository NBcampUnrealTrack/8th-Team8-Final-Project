#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BuildingHealthBar.generated.h"

class UProgressBar;

UCLASS()
class STRIKEFORGE_API UBuildingHealthBar : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION()
	void SetHealthBar(float InBeforeHP, float InCurrentHP, float InMaxHP);
	
private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;
};
