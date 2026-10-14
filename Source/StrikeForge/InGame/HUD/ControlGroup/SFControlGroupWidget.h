#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFControlGroupWidget.generated.h"

class USFControlNumWidget;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FSFOnControlGroupClicked,
    int32,
    GroupIndex
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FSFOnControlGroupResetRequested,
    int32,
    GroupIndex
);

UCLASS(Blueprintable)
class STRIKEFORGE_API USFControlGroupWidget
    : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "SF|ControlGroup")
    void UpdateControlGroupSlot(
        int32 GroupIndex,
        UTexture2D* RepresentativeImage,
        int32 ActorCount
    );

    UFUNCTION(BlueprintCallable, Category = "SF|ControlGroup")
    void RemoveControlGroupSlot(int32 GroupIndex);

    UFUNCTION(BlueprintCallable, Category = "SF|ControlGroup")
    void UpdateControlGroupSlotCount(
        int32 GroupIndex,
        int32 ActorCount
    );

    UFUNCTION(BlueprintCallable, Category = "SF|ControlGroup")
    void ClearControlGroupSlot(int32 GroupIndex);

    UFUNCTION(BlueprintCallable, Category = "SF|ControlGroup")
    void ClearAllControlGroupSlots();

    UFUNCTION(BlueprintPure, Category = "SF|ControlGroup")
    bool IsControlGroupAssigned(int32 GroupIndex) const;

    UPROPERTY(BlueprintAssignable, Category = "SF|ControlGroup")
    FSFOnControlGroupClicked OnControlGroupClicked;

    UPROPERTY(BlueprintAssignable, Category = "SF|ControlGroup")
    FSFOnControlGroupResetRequested OnControlGroupResetRequested;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // WBP_ControlGroup의 위젯 이름과 정확히 일치해야 함
    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|ControlGroup",
        meta = (BindWidget)
    )
    TObjectPtr<USFControlNumWidget> WBP_ControlNum01 = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|ControlGroup",
        meta = (BindWidget)
    )
    TObjectPtr<USFControlNumWidget> WBP_ControlNum02 = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|ControlGroup",
        meta = (BindWidget)
    )
    TObjectPtr<USFControlNumWidget> WBP_ControlNum03 = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|ControlGroup",
        meta = (BindWidget)
    )
    TObjectPtr<USFControlNumWidget> WBP_ControlNum04 = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|ControlGroup",
        meta = (BindWidget)
    )
    TObjectPtr<USFControlNumWidget> WBP_ControlNum05 = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|ControlGroup",
        meta = (BindWidget)
    )
    TObjectPtr<USFControlNumWidget> WBP_ControlNum06 = nullptr;

private:
    UFUNCTION()
    void HandleControlNumClicked(int32 GroupIndex);

    UFUNCTION()
    void HandleControlNumResetRequested(int32 GroupIndex);

    UPROPERTY(Transient)
    TArray<TObjectPtr<USFControlNumWidget>> ControlNumWidgets;
};