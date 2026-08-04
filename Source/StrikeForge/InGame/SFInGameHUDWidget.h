#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFSelectInfo/SFSelectableInfo.h"
#include "SFInGameHUDWidget.generated.h"

class AActor;
class USFSelectionPanelWidget;

UCLASS(Blueprintable)
class STRIKEFORGE_API USFInGameHUDWidget
    : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Selection"
    )
    TObjectPtr<AActor> SelectedActor;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Selection"
    )
    FSFSelectableInfo SelectedInfo;

    UPROPERTY(
        BlueprintReadOnly,
        meta = (BindWidgetOptional),
        Category = "SF|HUD"
    )
    TObjectPtr<USFSelectionPanelWidget> SelectionPanel;

private:
    UFUNCTION()
    void HandleSelectInput();

    void PerformSelectionTrace();

    void ClearSelection();
};