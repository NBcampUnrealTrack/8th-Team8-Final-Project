#pragma once
#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "SFCommandButtonArt.generated.h"

class UButton;
class SSFCommandButtonArt;

/** Decorative content only: the original UButton retains input, focus and delegates. */
UCLASS()
class STRIKEFORGE_API USFCommandButtonArt : public UWidget
{
    GENERATED_BODY()
public:
    static void Install(UButton* Button, int32 Symbol);
    TWeakObjectPtr<UButton> OwnerButton;
    int32 Symbol = 0;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
    TSharedPtr<SSFCommandButtonArt> Art;
};
