#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFResourcePanelWidget.generated.h"

class UTextBlock;

UCLASS()
class STRIKEFORGE_API USFResourcePanelWidget : public UUserWidget
{
    GENERATED_BODY()

    // 함수 영역
public:
    UFUNCTION(BlueprintCallable, Category = "SF|HUD|Resource")
    void UpdateMineral(int32 NewMineralValue, float NewMineralRate);

    UFUNCTION(BlueprintCallable, Category = "SF|HUD|Resource")
    void UpdateEther(int32 NewEtherValue, float NewEtherRate);

    UFUNCTION(BlueprintCallable, Category = "SF|HUD|Resource")
    void UpdatePopulation(int32 CurrentPopulation, int32 MaximumPopulation);

private:
    FText MakeResourceRateText(float ResourceRate) const;

    // 변수 영역
protected:
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "SF|HUD|Resource")
    TObjectPtr<UTextBlock> MineralValueText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "SF|HUD|Resource")
    TObjectPtr<UTextBlock> MineralRateText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "SF|HUD|Resource")
    TObjectPtr<UTextBlock> EtherValueText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "SF|HUD|Resource")
    TObjectPtr<UTextBlock> EtherRateText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "SF|HUD|Resource")
    TObjectPtr<UTextBlock> PopulationValueText;
};