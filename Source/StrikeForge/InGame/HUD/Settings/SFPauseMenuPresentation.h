#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "SFPauseMenuPresentation.generated.h"

/** Resolution-independent metalwork, with Slate time so light continues while paused. */
UCLASS()
class STRIKEFORGE_API USFPauseMenuPresentation : public UWidget
{
    GENERATED_BODY()
public:
    bool bBackdrop = false;
    bool bSettingsPanel = false;
    double StartTime = 0.;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
};
