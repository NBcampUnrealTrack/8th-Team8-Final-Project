#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "SFSettingsWidget.generated.h"

class UButton;

UCLASS(Abstract, Blueprintable)
class STRIKEFORGE_API USFSettingsWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    USFSettingsWidget(const FObjectInitializer& ObjectInitializer);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeOnActivated() override;
    virtual UWidget* NativeGetDesiredFocusTarget() const override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Settings", meta = (BindWidget))
    TObjectPtr<UButton> ContinueButton = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Settings", meta = (BindWidget))
    TObjectPtr<UButton> ExitButton = nullptr;

private:
    void BuildMenuPresentation();

    UPROPERTY(Transient) TObjectPtr<class UCanvasPanel> MenuRoot;
    UPROPERTY(Transient) TObjectPtr<class USFPauseMenuPresentation> Atmosphere;
    UPROPERTY(Transient) TObjectPtr<class USFPauseMenuPresentation> PanelArt;
    UPROPERTY() TObjectPtr<class UFont> MenuFont;
    UPROPERTY() TObjectPtr<class UFontFace> TitleFace;
    UPROPERTY(Transient) TObjectPtr<class UFont> TitleFont;
    UPROPERTY(Transient) TObjectPtr<UButton> OptionsButton;
    UPROPERTY(Transient) TObjectPtr<class USettingWidget> OptionsContent;

    UFUNCTION()
    void HandleOptionsButtonClicked();

    UFUNCTION()
    void HandleContinueButtonClicked();

    UFUNCTION()
    void HandleExitButtonClicked();
};
