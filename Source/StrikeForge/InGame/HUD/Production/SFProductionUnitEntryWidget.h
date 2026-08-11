#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFProductionUnitEntryWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FSFOnProductionUnitClicked,
    FName,
    UnitRowName
);

UCLASS(Abstract, Blueprintable)
class STRIKEFORGE_API USFProductionUnitEntryWidget
    : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(
        BlueprintCallable,
        Category = "SF|Production"
    )
    void InitializeProductionUnit(
        FName InUnitRowName
    );

protected:
    virtual void NativeConstruct() override;

private:
    UFUNCTION()
    void HandleProductionButtonClicked();

protected:
    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Production",
        meta = (BindWidget)
    )
    TObjectPtr<UButton> ProductionButton = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Production",
        meta = (BindWidget)
    )
    TObjectPtr<UImage> UnitIconImage = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Production",
        meta = (BindWidget)
    )
    TObjectPtr<UTextBlock> UnitNameText = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Production"
    )
    FName UnitRowName = NAME_None;

public:
    UPROPERTY(
        BlueprintAssignable,
        Category = "SF|Production"
    )
    FSFOnProductionUnitClicked OnProductionUnitClicked;
};