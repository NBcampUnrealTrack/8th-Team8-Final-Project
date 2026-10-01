#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFMineableResourceWidget.generated.h"

class UBuildingPlacement;
class UTextBlock;

/** Designer-authored WBP; observes the owning controller's live mine preview. */
UCLASS(Abstract, Blueprintable)
class STRIKEFORGE_API USFMineableResourceWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
    TObjectPtr<UTextBlock> AmountText;
private:
    UFUNCTION()
    void HandleMineableResourceAmountChanged(int32 Amount);
    UPROPERTY(Transient)
    TWeakObjectPtr<UBuildingPlacement> BoundPlacement;
};
