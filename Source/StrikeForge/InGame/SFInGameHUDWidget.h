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

    // 함수 영역
protected:
    virtual void NativeConstruct() override;

    virtual void NativeDestruct() override;

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

    void UnbindSelectedActorDestroyed();

    void ClearSelection();

    // 변수 영역
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
        meta = (
            BindWidgetOptional
            )
    )
    TObjectPtr<USFSelectionPanelWidget> SelectionPanel = nullptr;
};