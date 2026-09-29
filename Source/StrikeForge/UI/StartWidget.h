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
	FWidgetTransform Transform;
	float Hover = 0.f;
	float Press = 0.f;
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

	UFUNCTION()
	void OnStartButtonClicked();
	UFUNCTION()
	void OnSettingButtonClicked();
	UFUNCTION()
	void OnExitButtonClicked();

protected:
	UPROPERTY() TObjectPtr<UMaterialInterface> MenuArtMaterial;
	UPROPERTY() TObjectPtr<UTexture2D> MenuArtAtlas;
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
