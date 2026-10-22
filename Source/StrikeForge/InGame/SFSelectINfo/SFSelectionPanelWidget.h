#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFSelectableInfo.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "SFSelectionPanelWidget.generated.h"

class UWidget;
class UButton;
class UTextBlock;

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
    friend struct FSFPVPTeamAudit;
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

    UFUNCTION(BlueprintCallable, Category = "SF|Selection|Resource")
    void SetMiningResourceAmount(int32 RemainingResourceAmount);

    UFUNCTION(BlueprintCallable, Category = "SF|Selection|Resource")
    void ClearMiningResourceAmount();

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

    UFUNCTION(BlueprintCallable, Category = "SF|Selection|Command")
    void SetCommandButtonsVisible(bool bVisible);

    void SetAttackCommandButtonVisible(bool bVisible);

    void SetTierUpStageLocked(bool bLocked);

    UFUNCTION(BlueprintCallable, Category = "SF|Selection|Building")
    void SetUpgradeCost(const FResources& InUpgradeCost);

    UFUNCTION(BlueprintCallable, Category = "SF|Selection|Building")
    void SetUpgradeAffordability(bool bInCanAfford);

    UFUNCTION(BlueprintImplementableEvent, Category = "SF|Selection|Building")
    void BP_OnUpgradeAffordabilityChanged(bool bCanAfford);

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
        Category = "SF|Selection|Building"
    )
    void BP_OnTierUpStageLockChanged(
        bool bLocked
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

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Command",
        meta = (BindWidget)
    )
    TObjectPtr<UButton> AttackCommandButton = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Command",
        meta = (BindWidget)
    )
    TObjectPtr<UButton> StopCommandButton = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Selection|Building",
        meta = (BindWidget)
    )
    TObjectPtr<UTextBlock> UpgradeMineralCostText = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Selection|Building",
        meta = (BindWidget)
    )
    TObjectPtr<UTextBlock> UpgradeEtherCostText = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Selection|Resource",
        meta = (BindWidgetOptional)
    )
    TObjectPtr<UTextBlock> MiningResourceAmountText = nullptr;

private:
    void HandleBuffEntryGenerated(UUserWidget& EntryWidget);

    UFUNCTION()
    void HandleRallyPointButtonClicked();

    UFUNCTION()
    void HandleAttackCommandButtonClicked();

    UFUNCTION()
    void HandleStopCommandButtonClicked();

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
