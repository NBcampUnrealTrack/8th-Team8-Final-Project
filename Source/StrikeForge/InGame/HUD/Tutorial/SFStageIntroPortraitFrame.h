#pragma once

#include "CoreMinimal.h"
#include "Components/ContentWidget.h"
#include "SFStageIntroPortraitFrame.generated.h"

class SBox;
class UTexture2D;

/** 기존 초상화 Image를 유지하면서 공통 크기의 휘장을 씌우는 런타임 컨테이너. */
UCLASS()
class STRIKEFORGE_API USFStageIntroPortraitFrame : public UContentWidget
{
    GENERATED_BODY()

public:
    void SetFrame(UTexture2D* Texture, const FVector2D& Size, const FMargin& Inset,
        float Zoom, const FVector2D& Offset, const FLinearColor& BackgroundColor);
    virtual void SynchronizeProperties() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void OnSlotAdded(UPanelSlot* InSlot) override;
    virtual void OnSlotRemoved(UPanelSlot* InSlot) override;

private:
    void RefreshPresentation();

    UPROPERTY(Transient)
    FSlateBrush FrameBrush;

    FVector2D FrameSize = FVector2D(320.f, 480.f);
    FMargin FrameInset = FMargin(0.17f, 0.13f, 0.17f, 0.23f);
    float PortraitZoom = 1.5f;
    FVector2D PortraitOffset = FVector2D::ZeroVector;
    FLinearColor BackdropColor = FLinearColor(0.025f, 0.04f, 0.065f, 1.f);
    TSharedPtr<SBox> Container;
};
