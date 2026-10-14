#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFBuffTooltipWidget.generated.h"

class UTextBlock;

/** Compact counterpart of the commander skill tooltip, using the authored buff text. */
UCLASS()
class STRIKEFORGE_API USFBuffTooltipWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetDetails(const FText& Name, const FText& Description);

protected:
    virtual void NativeOnInitialized() override;

private:
    UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> EffectText;
};
