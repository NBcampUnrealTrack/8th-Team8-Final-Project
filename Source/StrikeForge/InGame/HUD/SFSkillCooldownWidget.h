#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "SFSkillCooldownWidget.generated.h"

class SSFSkillCooldown;

/** A square radial mask that reveals the icon clockwise from twelve o'clock. */
UCLASS()
class STRIKEFORGE_API USFSkillCooldownWidget : public UWidget
{
    GENERATED_BODY()
public:
    void SetCooldown(float Remaining, float Duration);
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    float RemainingSeconds = 0.f;
    float DurationSeconds = 30.f;
    TSharedPtr<SSFSkillCooldown> CooldownSlate;
};
