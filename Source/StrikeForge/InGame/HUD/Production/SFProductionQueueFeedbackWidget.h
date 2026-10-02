#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFProductionQueueFeedbackWidget.generated.h"

UCLASS()
class STRIKEFORGE_API USFProductionQueueFeedbackWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void InitializeContent(UWidget* Content);
    UWidget* ReleaseContent();
    void SetPresentation(float InHover,float InFlash,bool InOccupied,bool InActive,float InProgress);
protected:
    virtual int32 NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Cull,
        FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const override;
private:
    UPROPERTY(Transient) TObjectPtr<UWidget> OriginalContent;
    float Hover=0.f, Flash=0.f, Progress=0.f;
    bool bOccupied=false, bActive=false;
};
