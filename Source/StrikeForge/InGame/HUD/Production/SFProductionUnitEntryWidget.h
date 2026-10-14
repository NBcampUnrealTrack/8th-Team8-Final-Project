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

    void ShowRequestFeedback(bool bAccepted);
    FName GetProductionRowName() const { return UnitRowName; }
    UWidget* GetTutorialProductionButton() const;
#if WITH_DEV_AUTOMATION_TESTS
    bool bPreviewHover = false;
#endif
protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
        const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer,
        const FWidgetStyle& Style, bool bParentEnabled) const override;
private:
    void ArrangeButton();
    float HoverAmount = 0.f;
    float PressAmount = 0.f;
    float FeedbackRemaining = 0.f;
    float ShineTime = 0.f;
    bool bLastAccepted = false;

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

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Production",
        meta = (BindWidget)
    )
    TObjectPtr<UTextBlock> UnitNeedGold = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Production",
        meta = (BindWidget)
    )
    TObjectPtr<UTextBlock> UnitNeedEther = nullptr;
public:
    UPROPERTY(
        BlueprintAssignable,
        Category = "SF|Production"
    )
    FSFOnProductionUnitClicked OnProductionUnitClicked;
};
