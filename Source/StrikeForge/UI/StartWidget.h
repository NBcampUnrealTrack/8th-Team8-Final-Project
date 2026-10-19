#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SettingWidget.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "StartWidget.generated.h"

class UButton;
class UWidget;
class USFMenuAtmosphereWidget;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;
class UImage;
class UTextBlock;
class USFStartButtonFeedbackWidget;
class UFont;
class ASFStartSceneAmbience;

USTRUCT()
struct FSFStartArtControl
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UWidget> Widget;
	UPROPERTY() FButtonStyle OriginalStyle;
	UPROPERTY() FSlateBrush OriginalImage;
	UPROPERTY() FAnchorData OriginalLayout;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Material;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Label;
    UPROPERTY(Transient) TObjectPtr<USFStartButtonFeedbackWidget> Feedback;
	UPROPERTY(Transient) TObjectPtr<UWidget> OriginalContent;
    ESlateVisibility OriginalVisibility = ESlateVisibility::Visible;
	FWidgetTransform Transform;
	float Hover = 0.f;
	float Press = 0.f;
    float HoverTime = 0.f;
    float ClickTime = 1.f;
    bool bWasHovered = false;
};
UCLASS()
class STRIKEFORGE_API UStartWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UStartWidget(const FObjectInitializer& ObjectInitializer);
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	void ApplyStartArt();
	void FinishLobbyTransition();
    void PlayButtonFeedback(int32 Index);
    UFUNCTION() void OnStartButtonPressed();
    UFUNCTION() void OnSettingButtonPressed();
    UFUNCTION() void OnExitButtonPressed();
    UFUNCTION() void OnPVPButtonPressed();
    UFUNCTION() void OnPVPButtonClicked();
    UPROPERTY(Transient) TObjectPtr<UButton> PVPButton;

	UFUNCTION()
	void OnStartButtonClicked();
	UFUNCTION()
	void OnSettingButtonClicked();
	UFUNCTION()
	void OnExitButtonClicked();

protected:
	UPROPERTY() TObjectPtr<UMaterialInterface> LogoMaterial;
	UPROPERTY() TObjectPtr<UTexture2D> MenuArtAtlas;
	UPROPERTY() TObjectPtr<UTexture2D> Wordmark;
	UPROPERTY() TObjectPtr<UFont> MenuFont;
	ESlateVisibility OriginalBackdropVisibility = ESlateVisibility::Visible;
	UPROPERTY() TObjectPtr<UMaterialInterface> MenuShadeMaterial;
	UPROPERTY(Transient) TObjectPtr<UImage> MenuShade;
	UPROPERTY(Transient) TObjectPtr<ASFStartSceneAmbience> SceneAmbience;
	UPROPERTY(Transient) TArray<FSFStartArtControl> ArtControls;
	float ArtElapsed = 0.f;
	UPROPERTY(Transient)
	TObjectPtr<USFMenuAtmosphereWidget> DepartureMist;
	FTimerHandle LobbyTransitionTimer;
	bool bEnteringLobby = false;

	UPROPERTY(meta = (BindWidget))
	UWidget* Title;
	UPROPERTY(meta = (BindWidget))
	UButton* Start_Btn;
	UPROPERTY(meta = (BindWidget))
	UButton* Setting_Btn;
	UPROPERTY(meta = (BindWidget))
	UButton* Exit_Btn;

	UPROPERTY(meta = (BindWidget))
	USettingWidget* SettingWidget;
};
