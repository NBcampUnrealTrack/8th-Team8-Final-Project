#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFSelectableInfo.h"
#include "SFMultiSelectionEntryWidget.generated.h"

class AActor;
class UButton;
class UImage;
class UProgressBar;
class UHealthComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FSFOnMultiSelectionEntryClicked,
    AActor*,
    SelectedActor
);

UCLASS(Abstract, Blueprintable)
class STRIKEFORGE_API USFMultiSelectionEntryWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "SF|Selection")
    void InitializeEntry(
        AActor* Actor,
        const FSFSelectableInfo& SelectableInfo,
        int32 Index
    );

    UFUNCTION(BlueprintPure, Category = "SF|Selection")
    AActor* GetRepresentedActor() const;

    UFUNCTION(BlueprintPure, Category = "SF|Selection")
    int32 GetSelectionIndex() const;

    UPROPERTY(BlueprintAssignable, Category = "SF|Selection")
    FSFOnMultiSelectionEntryClicked OnEntryClicked;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Selection", meta = (BindWidget))
    TObjectPtr<UButton> EntryButton = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Selection", meta = (BindWidget))
    TObjectPtr<UImage> SelectionIconImage = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Selection", meta = (BindWidgetOptional))
    TObjectPtr<UProgressBar> HealthBar = nullptr;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "SF|Selection")
    TObjectPtr<AActor> RepresentedActor = nullptr;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "SF|Selection")
    int32 EntryIndex = INDEX_NONE;

private:
    UFUNCTION()
    void HandleEntryClicked();
    UFUNCTION()
    void HandleHealthChanged(float BeforeHP, float CurrentHP, float MaxHP);

    void BindHealthDelegate(AActor* Actor);
    void UnbindHealthDelegate();
    void RefreshHealthBar(float CurrentHP, float MaxHP);

    UPROPERTY(Transient)
    TWeakObjectPtr<UHealthComponent> BoundHealthComponent;

    bool bShouldShowHealth = false;

};