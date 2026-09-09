#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFSelectableInfo.h"
#include "SFMultiSelectionPanelWidget.generated.h"

class AActor;
class UUniformGridPanel;
class USFMultiSelectionEntryWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FSFOnMultiSelectionPrimaryRequested,
    AActor*,
    SelectedActor
);

UCLASS(Abstract, Blueprintable)
class STRIKEFORGE_API USFMultiSelectionPanelWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void ShowSelections(
        const TArray<TObjectPtr<AActor>>& Actors,
        const TArray<FSFSelectableInfo>& SelectableInfos
    );

    void ClearSelections();

    UPROPERTY(BlueprintAssignable, Category = "SF|Selection")
    FSFOnMultiSelectionPrimaryRequested OnPrimarySelectionRequested;

protected:
    virtual void NativeDestruct() override;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Selection", meta = (BindWidget))
    TObjectPtr<UUniformGridPanel> SelectionGrid = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Selection")
    TSubclassOf<USFMultiSelectionEntryWidget> EntryWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Selection", meta = (ClampMin = "1"))
    int32 ColumnCount = 8;

private:
    UFUNCTION()
    void HandleEntryClicked(AActor* Actor);

    UPROPERTY(Transient)
    TArray<TObjectPtr<USFMultiSelectionEntryWidget>> EntryWidgets;
};