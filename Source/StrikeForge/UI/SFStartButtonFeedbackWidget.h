#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFStartButtonFeedbackWidget.generated.h"

class UTextBlock;

// Button content owns both the label and the effect's paint space. Never mix
// a parent paint geometry with a child's cached desktop/window geometry.
UCLASS()
class STRIKEFORGE_API USFStartButtonFeedbackWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UTextBlock* CreateLabel();
    void SetFeedback(float InHover, float InHoverTime, float InClickTime, bool bInDrawFeedback);
protected:
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
        const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer,
        const FWidgetStyle& Style, bool bParentEnabled) const override;
private:
    float Hover = 0.f;
    float HoverTime = 0.f;
    float ClickTime = 1.f;
    bool bDrawFeedback = true;
};
