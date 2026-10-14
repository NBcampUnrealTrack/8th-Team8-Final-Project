#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "SFUnitHealthBarComponent.generated.h"

class UHealthComponent;
class UProgressBar;

UCLASS()
class STRIKEFORGE_API USFUnitHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void UpdateHealth(float Ratio, const FLinearColor& Color);
protected:
	virtual void NativeOnInitialized() override;
private:
	UPROPERTY(Transient) TObjectPtr<UProgressBar> Bar;
};

/** Shared by combat units, neutral monsters, and commanders. */
UCLASS(ClassGroup=(UI), meta=(BlueprintSpawnableComponent))
class STRIKEFORGE_API USFUnitHealthBarComponent : public UWidgetComponent
{
	GENERATED_BODY()
public:
	USFUnitHealthBarComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	UFUNCTION() void HealthChanged(float Before, float Current, float Maximum);
	void RefreshLayout();
	UPROPERTY(Transient) TObjectPtr<UHealthComponent> Health;
};
