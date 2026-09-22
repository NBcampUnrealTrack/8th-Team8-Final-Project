#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFControlNumWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTexture2D;
class UWidgetAnimation;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FSFOnControlNumClicked,
    int32,
    GroupIndex
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FSFOnControlNumResetRequested,
    int32,
    GroupIndex
);

UCLASS(Blueprintable)
class STRIKEFORGE_API USFControlNumWidget
    : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "SF|ControlGroup")
    void InitializeControlNum(int32 InGroupIndex);

    UFUNCTION(BlueprintCallable, Category = "SF|ControlGroup")
    void SetControlGroupData(
        UTexture2D* InRepresentativeImage,
        int32 InActorCount
    );

    UFUNCTION(BlueprintCallable, Category = "SF|ControlGroup")
    void ClearControlGroupData();

    UFUNCTION(BlueprintPure, Category = "SF|ControlGroup")
    bool HasControlGroupData() const
    {
        return bHasControlGroupData;
    }

    UFUNCTION(BlueprintPure, Category = "SF|ControlGroup")
    int32 GetGroupIndex() const
    {
        return GroupIndex;
    }

    UPROPERTY(BlueprintAssignable, Category = "SF|ControlGroup")
    FSFOnControlNumClicked OnControlNumClicked;

    UPROPERTY(BlueprintAssignable, Category = "SF|ControlGroup")
    FSFOnControlNumResetRequested OnControlNumResetRequested;

    UFUNCTION(BlueprintCallable, Category = "SF|ControlGroup")
    void PlaySlideOutAndHide();

    void SetActorCount(int32 NewCount);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    virtual FReply NativeOnMouseButtonDown(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent
    ) override;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|ControlGroup",
        meta = (BindWidgetOptional)
    )
    TObjectPtr<UTextBlock> ControlNumberText = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|ControlGroup",
        meta = (BindWidgetOptional)
    )
    TObjectPtr<UImage> RepresentativeImage = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|ControlGroup",
        meta = (BindWidgetOptional)
    )
    TObjectPtr<UTextBlock> UnitCountText = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|ControlGroup",
        meta = (BindWidgetOptional)
    )
    TObjectPtr<UButton> SlotButton = nullptr;

    UPROPERTY(
        Transient,
        BlueprintReadOnly,
        Category = "SF|ControlGroup",
        meta = (BindWidgetAnim)
    )
    TObjectPtr<UWidgetAnimation> ControlNum_SlideIn = nullptr;

    UPROPERTY(
        Transient,
        BlueprintReadOnly,
        Category = "SF|ControlGroup"
    )
    int32 GroupIndex = INDEX_NONE;

    UPROPERTY(
        Transient,
        BlueprintReadOnly,
        Category = "SF|ControlGroup"
    )
    bool bHasControlGroupData = false;

private:
    UFUNCTION()
    void HandleSlotButtonClicked();

    UFUNCTION()
    void HandleControlNumSlideAnimationFinished();

    bool bIsPlayingSlideOut = false;
};