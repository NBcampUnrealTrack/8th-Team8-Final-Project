#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/Building/Productable.h"
#include "SFProductionBuildingPanelWidget.generated.h"

class AActor;
class UVerticalBox;
class USFProductionUnitEntryWidget;
class UImage;
class UProductComponent;
class UProgressBar;

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

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Production",
        meta = (BindWidget)
    )
    TObjectPtr<UImage> QueueIcon01 = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Production",
        meta = (BindWidget)
    )
    TObjectPtr<UImage> QueueIcon02 = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Production",
        meta = (BindWidget)
    )
    TObjectPtr<UImage> QueueIcon03 = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Production",
        meta = (BindWidget)
    )
    TObjectPtr<UImage> QueueIcon04 = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Production",
        meta = (BindWidget)
    )
    TObjectPtr<UImage> QueueIcon05 = nullptr;

    UFUNCTION()
    void HandleProductRatioChanged(
        float ProductRatio
    );

    void RefreshProductionProgress(
        float ProductRatio
    );

protected:
    virtual void NativeConstruct() override;

private:
    void RefreshProductionUnitEntries(
        const FProductableData& ProductableData
    );

    UFUNCTION()
    void HandleProductionUnitClicked(
        FName UnitRowName
    );

    UFUNCTION()
    void HandleProductLineChanged(
        TArray<FProductInfo> ProductLine
    );

    void BindProductComponent(
        UProductComponent* InProductComponent
    );

    void UnbindProductComponent();

    void RefreshProductionQueue(
        const TArray<FProductInfo>& ProductLine
    );

protected:
    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Production",
        meta = (BindWidget)
    )
    TObjectPtr<UVerticalBox> UnitEntryBox = nullptr;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Production"
    )
    TSubclassOf<USFProductionUnitEntryWidget>
        ProductionUnitEntryWidgetClass;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Production",
        meta = (BindWidget)
    )
    TObjectPtr<UProgressBar> ProductionProgressBar = nullptr;

private:
    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> CurrentProductionBuilding;

    UPROPERTY(Transient)
    TObjectPtr<UProductComponent> CurrentProductComponent = nullptr;

    TArray<TObjectPtr<UImage>> QueueSlots;
};