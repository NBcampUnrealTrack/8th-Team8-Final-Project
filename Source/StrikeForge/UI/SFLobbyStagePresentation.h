#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "SFLobbyStagePresentation.generated.h"

class USFMenuAtmosphereWidget;
class UTexture2D;
class UMaterialInterface;
class UMaterialInstanceDynamic;

USTRUCT()
struct FSFLobbyHeaderControl
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<UWidget> Widget;
    UPROPERTY() FButtonStyle OriginalStyle;
    UPROPERTY() FSlateBrush OriginalImageBrush;
    UPROPERTY() FAnchorData OriginalLayout;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Material;
    FWidgetTransform Transform;
    FVector2D Pivot = FVector2D(.5,.5);
    float Opacity = 1.f;
    float Hover = 0.f;
    float Press = 0.f;
};

USTRUCT()
struct FSFLobbyStageControl
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<UButton> Button;
    UPROPERTY() FOnButtonClickedEvent OriginalClick;
    UPROPERTY() FButtonStyle OriginalStyle;
    UPROPERTY() FAnchorData OriginalLayout;
    bool bEnabled = false;
    bool bCleared = false;
    float ContentOpacity = 1.f;
    float Hover = 0.f;
};

/** Presentation over the existing named buttons; their original click delegates are retained. */
UCLASS()
class STRIKEFORGE_API USFLobbyStagePresentation : public UUserWidget
{
    GENERATED_BODY()
public:
    USFLobbyStagePresentation(const FObjectInitializer& ObjectInitializer);
    void InitializeControls(UUserWidget* Lobby, USFMenuAtmosphereWidget* Backdrop, const TArray<FName>& Rows);
    void RestoreControls();
#if WITH_DEV_AUTOMATION_TESTS
    int32 PreviewHover = INDEX_NONE;
    int32 PreviewHeaderHover = INDEX_NONE;
    void PreviewLaunch(int32 Index) { LaunchIndex = Index; LaunchElapsed = 0.f; bPreviewLaunch = true; }
#endif
protected:
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullRect,
        FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override;
private:
    void RequestLaunch(int32 Index);
    UFUNCTION() void Click1() { RequestLaunch(0); }
    UFUNCTION() void Click2() { RequestLaunch(1); }
    UFUNCTION() void Click3() { RequestLaunch(2); }
    UFUNCTION() void Click4() { RequestLaunch(3); }
    UFUNCTION() void Click5() { RequestLaunch(4); }
    UFUNCTION() void Click6() { RequestLaunch(5); }
    UFUNCTION() void Click7() { RequestLaunch(6); }
    UPROPERTY() TObjectPtr<UTexture2D> IconAtlas;
    UPROPERTY() TObjectPtr<UTexture2D> ClearedIcon;
    UPROPERTY() TObjectPtr<UMaterialInterface> HeaderMaterial;
    UPROPERTY(Transient) TObjectPtr<USFMenuAtmosphereWidget> Atmosphere;
    UPROPERTY(Transient) TArray<FSFLobbyStageControl> Controls;
    UPROPERTY(Transient) TArray<FSFLobbyHeaderControl> HeaderControls;
    UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> Roads;
    TArray<ESlateVisibility> RoadVisibility;
    float Time = 0.f;
    float LaunchElapsed = 0.f;
    int32 LaunchIndex = INDEX_NONE;
    bool bPreviewLaunch = false;
};
