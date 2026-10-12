#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Datas/Race/SFRaceType.h"
#include "Styling/SlateTypes.h"
#include "SFRaceSelectionWidget.generated.h"

/** Modal faction choice before travelling to the final battle. */
UCLASS()
class STRIKEFORGE_API USFRaceSelectionWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    USFRaceSelectionWidget(const FObjectInitializer& ObjectInitializer);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
    FReply Choose(ESFRace Race);
    FReply Cancel();

    UPROPERTY()
    TObjectPtr<class UTexture2D> EtherianPortrait;
    UPROPERTY()
    TObjectPtr<UTexture2D> BeastPortrait;
    UPROPERTY()
    TObjectPtr<class UFont> TitleFont;

    FSlateBrush EtherianBrush;
    FSlateBrush BeastBrush;
    FButtonStyle EtherianButtonStyle;
    FButtonStyle BeastButtonStyle;
    FButtonStyle BackButtonStyle;
    FText TravelError;
    bool bTravelling = false;
};
