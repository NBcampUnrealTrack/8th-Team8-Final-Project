#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFSelectInfo/SFSelectableInfo.h"
#include "SFInGameHUDWidget.generated.h"

class AActor;
class UButton;
class UCommonActivatableWidget;
class UCommonActivatableWidgetStack;
class UResultWidget;
class USFProductionBuildingPanelWidget;
class USFSelectionPanelWidget;
class UHealthComponent;

UCLASS(Blueprintable)
class STRIKEFORGE_API USFInGameHUDWidget
    : public UUserWidget
{
    GENERATED_BODY()

    // ==================== Functions ====================

public:
    UFUNCTION(
        BlueprintCallable,
        Category = "SF|HUD|Building"
    )
    void OpenBuildingMode();

    UFUNCTION(
        BlueprintCallable,
        Category = "SF|HUD|Building"
    )
    void CloseBuildingMode();
    
    UFUNCTION(
        BlueprintCallable,
        Category = "SF|HUD|Settings"
    )
    void ToggleSettings();

    void ShowResultWidget(bool bIsWin);

    void SelectActor(AActor* Actor);

    bool IsPointerOverSelectionAction() const;

protected:
    virtual void NativeConstruct() override;

    virtual void NativeDestruct() override;

    UFUNCTION()
    void HandleSelectedHealthChanged(float BeforeHP, float CurrentHP, float MaxHP);

    void BindSelectedHealthDelegate(AActor* Actor);

    void UnbindSelectedHealthDelegate();

private:
    UFUNCTION()
    void HandleSelectInput();

    UFUNCTION()
    void HandleSelectedActorDestroyed(
        AActor* DestroyedActor
    );

    void PerformSelectionTrace();

    void ApplySelection(
        AActor* NewSelectedActor,
        const FSFSelectableInfo& NewSelectedInfo
    );

    void ApplySelectionUIData(
        FSFSelectableInfo& InOutSelectableInfo
    );

    void RefreshSelectionPanels();

    void ClearSelection();

    void UnbindSelectedActorDestroyed();

    void HandleBuildingModeDeactivated();

    void RefreshBuildModeOpenButtonVisibility();

    UFUNCTION()
    void HandleTierUpgradeRequested();

    void HandleSettingsDeactivated();

    UPROPERTY(Transient)
    TObjectPtr<UCommonActivatableWidget> ActiveSettingsWidget = nullptr;

    // ==================== Variables ====================

protected:
    UPROPERTY(
        Transient,
        BlueprintReadOnly,
        Category = "SF|Selection"
    )
    TObjectPtr<AActor> SelectedActor = nullptr;

    UPROPERTY(
        Transient,
        BlueprintReadOnly,
        Category = "SF|Selection"
    )
    FSFSelectableInfo SelectedInfo;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|HUD",
        meta = (BindWidgetOptional)
    )
    TObjectPtr<USFSelectionPanelWidget> SelectionPanel = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|HUD|Production",
        meta = (BindWidgetOptional)
    )
    TObjectPtr<USFProductionBuildingPanelWidget>
        ProductionBuildingPanel = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|HUD|Building",
        meta = (BindWidget)
    )
    TObjectPtr<UCommonActivatableWidgetStack> BuildingUIStack = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|HUD|Building",
        meta = (BindWidget)
    )
    TObjectPtr<UButton> BuildModeOpenButton = nullptr;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|HUD|Building"
    )
    TSubclassOf<UCommonActivatableWidget> BuildingModeWidgetClass;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|HUD|Result"
    )
    TSubclassOf<UResultWidget> ResultWidgetClass;

    UPROPERTY(BlueprintReadOnly, Category = "SF|HUD|Settings", meta = (BindWidget))
    TObjectPtr<UCommonActivatableWidgetStack> SettingsUIStack = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|HUD|Settings")
    TSubclassOf<UCommonActivatableWidget> SettingsWidgetClass;

private:
    UPROPERTY(Transient)
    TObjectPtr<UCommonActivatableWidget> ActiveBuildingModeWidget = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UResultWidget> ResultWidget = nullptr;

    UPROPERTY(Transient)
    TWeakObjectPtr<UHealthComponent> SelectedHealthComponent;
};