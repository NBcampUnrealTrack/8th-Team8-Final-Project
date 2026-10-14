#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "SFResultPresentation.generated.h"

/** Real-time Slate effects keep playing after the battlefield has been paused. */
UCLASS()
class STRIKEFORGE_API USFResultPresentation : public UWidget
{
    GENERATED_BODY()
public:
    bool bVictory = true;
    bool bForeground = false;
    bool bEtherBurstPreview = false;
    UPROPERTY(Transient) TObjectPtr<class UFont> PreviewFont;
    double StartTime = 0.;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
};
