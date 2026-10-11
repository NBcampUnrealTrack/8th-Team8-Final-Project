#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFRaceSelectionWidget.generated.h"

/** Modal faction choice before travelling to the final battle. */
UCLASS()
class STRIKEFORGE_API USFRaceSelectionWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
    FReply Choose(bool bBeast);
    FReply Cancel();
};
