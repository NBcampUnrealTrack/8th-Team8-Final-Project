#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFDefenceTimerWidget.generated.h"
class ASFDefenceWaveController;
class UTextBlock;
class UBorder;

UCLASS()
class STRIKEFORGE_API USFDefenceTimerWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(Transient) TObjectPtr<ASFDefenceWaveController> Controller;
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
private:
    UPROPERTY(Transient) TObjectPtr<UTextBlock> PhaseText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> TimeText;
    UPROPERTY(Transient) TObjectPtr<UBorder> TimerBorder;
};
