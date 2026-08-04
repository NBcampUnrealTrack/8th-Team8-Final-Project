#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFSelectableInfo.h"
#include "SFSelectionPanelWidget.generated.h"

UCLASS(Abstract, Blueprintable)
class STRIKEFORGE_API USFSelectionPanelWidget
    : public UUserWidget
{
    GENERATED_BODY()

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
    void HideSelection();

    UFUNCTION(
        BlueprintPure,
        Category = "SF|Selection"
    )
    bool HasSelection() const;

protected:
    virtual void NativeConstruct() override;

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
            DisplayName = "On Selection Hidden"
            )
    )
    void BP_OnSelectionHidden();
};