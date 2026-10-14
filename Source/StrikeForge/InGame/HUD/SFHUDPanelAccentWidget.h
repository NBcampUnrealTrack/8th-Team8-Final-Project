#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "SFHUDPanelAccentWidget.generated.h"

class UUserWidget;
class SSFHUDPanelAccent;

/** Lightweight, input-transparent ornamentation fitted to the authored frame geometry. */
UCLASS()
class STRIKEFORGE_API USFHUDPanelAccentWidget : public UWidget
{
    GENERATED_BODY()
public:
    enum class EPanel : uint8 { Selection, Command, Minimap };
    static void Install(UUserWidget* Root, TArray<TWeakObjectPtr<USFHUDPanelAccentWidget>>& Accents);
    void Pulse();
    EPanel Panel = EPanel::Command;
    bool bForeground = false;
    TWeakObjectPtr<UUserWidget> PanelOwner;
    double PulseStart = -100.;
    UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> FrameMaterial;
    UPROPERTY(Transient) TObjectPtr<class ASFHUDMotesActor> MotesActor;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
    TSharedPtr<SSFHUDPanelAccent> AccentSlate;
};
