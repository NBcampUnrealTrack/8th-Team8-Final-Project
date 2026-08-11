#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFProductionBuildingPanelWidget.generated.h"

class AActor;

UCLASS(Abstract, Blueprintable)
class STRIKEFORGE_API USFProductionBuildingPanelWidget
    : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(
        BlueprintCallable,
        Category = "SF|Production"
    )
    void ShowProductionBuilding(
        AActor* InProductionBuilding
    );

    UFUNCTION(
        BlueprintCallable,
        Category = "SF|Production"
    )
    void HideProductionBuilding();

    UFUNCTION(
        BlueprintPure,
        Category = "SF|Production"
    )
    bool HasProductionBuilding() const;

    UFUNCTION(
        BlueprintPure,
        Category = "SF|Production"
    )
    AActor* GetProductionBuilding() const;

protected:
    virtual void NativeConstruct() override;

    UFUNCTION(
        BlueprintImplementableEvent,
        Category = "SF|Production",
        meta = (
            DisplayName = "On Production Building Updated"
            )
    )
    void BP_OnProductionBuildingUpdated(
        AActor* InProductionBuilding
    );

    UFUNCTION(
        BlueprintImplementableEvent,
        Category = "SF|Production",
        meta = (
            DisplayName = "On Production Building Hidden"
            )
    )
    void BP_OnProductionBuildingHidden();

private:
    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> CurrentProductionBuilding;
};