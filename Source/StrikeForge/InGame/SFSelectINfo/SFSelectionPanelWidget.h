#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFSelectableInfo.h"
#include "SFSelectionPanelWidget.generated.h"

class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(
    FSFOnTierUpgradeRequested
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(
    FSFOnRallyPointRequested
);

UCLASS(Abstract, Blueprintable)
class STRIKEFORGE_API USFSelectionPanelWidget
    : public UUserWidget
{
    GENERATED_BODY()

    // 함수 영역
public:
    UFUNCTION(
        BlueprintCallable,
        Category = "SF|Selection"
    )
    void ShowSelection(
        const FSFSelectableInfo& SelectableInfo
    );

    UFUNCTION(
        BlueprintCallable,
        Category = "SF|Selection"
    )
    void ClearSelection();

    UFUNCTION(
        BlueprintPure,
        Category = "SF|Selection"
    )
    bool HasSelection() const;

    UPROPERTY(
        BlueprintAssignable,
        Category = "SF|Selection|Building"
    )
    FSFOnTierUpgradeRequested OnTierUpgradeRequested;

    UPROPERTY(
        BlueprintAssignable,
        Category = "SF|Selection|Building"
    )
    FSFOnRallyPointRequested OnRallyPointRequested;

    UFUNCTION(
        BlueprintPure,
        Category = "SF|Selection|Building"
    )
    bool IsTierUpButtonHovered() const;

    void SetRallyPointButtonVisible(bool bVisible);

    void SetTierUpButtonVisible(bool bVisible);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UFUNCTION(
        BlueprintImplementableEvent,
        Category = "SF|Selection",
        meta = (
            DisplayName = "On Selection Info Updated"
            )
    )
    void BP_OnSelectionInfoUpdated(
        const FSFSelectableInfo& SelectableInfo
    );

    UFUNCTION(
        BlueprintImplementableEvent,
        Category = "SF|Selection",
        meta = (
            DisplayName = "On Selection Cleared"
            )
    )
    void BP_OnSelectionCleared();

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Selection|Building",
        meta = (BindWidget)
    )
    TObjectPtr<UButton> TierUpButton = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Selection|Building",
        meta = (BindWidget)
    )
    TObjectPtr<UButton> RallyPointButton = nullptr;

private:
    UFUNCTION()
    void HandleRallyPointButtonClicked();

    // 변수 영역
protected:
    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Selection"
    )
    FSFSelectableInfo CurrentSelectableInfo;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Selection"
    )
    bool bHasSelection = false;

private:
    UFUNCTION()
    void HandleTierUpgradeButtonClicked();

};