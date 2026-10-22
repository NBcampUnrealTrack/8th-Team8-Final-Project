#pragma once
#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "SFOptionsPresentation.generated.h"

namespace SFOptionsColors
{
    inline FLinearColor Hex(const TCHAR* Value) { return FLinearColor(FColor::FromHex(Value)); }
}

/** Metal, cut crystal, and live control feedback for the 1240 x 900 options layout. */
UCLASS()
class STRIKEFORGE_API USFOptionsPresentation : public UWidget
{
    GENERATED_BODY()
public:
    double StartTime = 0.;
    TArray<TWeakObjectPtr<UWidget>> Controls;
    TArray<TWeakObjectPtr<class USlider>> Sliders;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
};
