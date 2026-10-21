#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/Save/SFPVEAchievement.h"
#include "SFAchievementToastWidget.generated.h"

class UFont;
class UFontFace;

/** Non-interactive, real-time celebration that continues while the result screen pauses play. */
UCLASS()
class STRIKEFORGE_API USFAchievementToastWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    USFAchievementToastWidget(const FObjectInitializer& ObjectInitializer);
    void InitializeAchievement(const FSFPVEAchievement& Entry);
    static constexpr float Delay = 1.7f;
    static constexpr float Duration = 4.8f;
#if WITH_DEV_AUTOMATION_TESTS
    void PreviewAt(float Seconds) { PreviewTime = Seconds; }
#endif
protected:
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullRect,
        FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override;
private:
    float GetAge() const;
    UPROPERTY() TObjectPtr<UFont> TitleFont;
    UPROPERTY() TObjectPtr<UFont> BodyFont;
    UPROPERTY() TObjectPtr<UFontFace> BodyFace;
    UPROPERTY(Transient) FSFPVEAchievement Achievement;
    double StartTime = 0;
    float PreviewTime = -1.f;
};
