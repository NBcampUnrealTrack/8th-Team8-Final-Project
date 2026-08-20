#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"
#include "SFMinimapMarkerInfo.h"
#include "SFMinimapPanelWidget.generated.h"

class AActor;
class UBorder;
class UCanvasPanel;
class UWidget;

UCLASS(Blueprintable)
class STRIKEFORGE_API USFMinimapPanelWidget
    : public UUserWidget
{
    GENERATED_BODY()

    // 함수 영역
public:
    UFUNCTION(
        BlueprintPure,
        Category = "SF|Minimap|World Bounds"
    )
    bool HasValidMapBounds() const;

    UFUNCTION(
        BlueprintPure,
        Category = "SF|Minimap|Coordinate"
    )
    FVector2D WorldToMinimapPosition(
        const FVector& WorldLocation
    ) const;

    UFUNCTION(BlueprintPure, Category = "SF|Minimap|Coordinate")
    FVector MinimapToWorldLocation(const FVector2D& MinimapPosition) const;

protected:
    virtual void NativeConstruct() override;

    virtual void NativeDestruct() override;

    virtual void NativeTick(const FGeometry& MyGeometry,float InDeltaTime) override;

    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
   
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
private:
    void CreateMinimapMarkers();

    void CreateMarkersForTag(
        FName ActorTag,
        ESFMinimapMarkerType MarkerType
    );

    void CreateMarkerForActor(
        AActor* TargetActor,
        ESFMinimapMarkerType MarkerType
    );

    void ClearMinimapMarkers();

    void UpdatePlayerViewIndicator();

    void UpdateMinimapMarkers();

    void MoveCameraToPointerPosition(const FPointerEvent& InMouseEvent);

    bool bIsMinimapCameraDragging = false;

    void SetMarkerLayerPosition(
        UWidget* MarkerWidget,
        const FVector2D& DesiredPosition
    ) const;

    FVector2D GetMarkerSize(
        ESFMinimapMarkerType MarkerType
    ) const;

    FLinearColor GetMarkerColor(
        ESFMinimapMarkerType MarkerType
    ) const;

    // 변수 영역
protected:
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Minimap|World Bounds"
    )
    FVector2D MapWorldMin =
        FVector2D(
            -5000.0f,
            -5000.0f
        );

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Minimap|World Bounds"
    )
    FVector2D MapWorldMax =
        FVector2D(
            5000.0f,
            5000.0f
        );

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Minimap|Marker|Tag"
    )
    FName AllyMarkerActorTag =
        TEXT("MinimapAlly");

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Minimap|Marker|Tag"
    )
    FName EnemyMarkerActorTag =
        TEXT("MinimapEnemy");

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Minimap|Marker|Tag"
    )
    FName BuildingMarkerActorTag =
        TEXT("MinimapBuilding");

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Minimap|Marker|Size"
    )
    FVector2D AllyMarkerSize =
        FVector2D(
            8.0f,
            8.0f
        );

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Minimap|Marker|Size"
    )
    FVector2D EnemyMarkerSize =
        FVector2D(
            8.0f,
            8.0f
        );

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Minimap|Marker|Size"
    )
    FVector2D BuildingMarkerSize =
        FVector2D(
            12.0f,
            12.0f
        );

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Minimap|Marker|Color"
    )
    FLinearColor AllyMarkerColor =
        FLinearColor(
            0.1f,
            0.65f,
            1.0f,
            1.0f
        );

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Minimap|Marker|Color"
    )
    FLinearColor EnemyMarkerColor =
        FLinearColor(
            1.0f,
            0.2f,
            0.2f,
            1.0f
        );

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Minimap|Marker|Color"
    )
    FLinearColor BuildingMarkerColor =
        FLinearColor(
            1.0f,
            0.8f,
            0.2f,
            1.0f
        );

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Minimap",
        meta = (
            BindWidget
            )
    )
    TObjectPtr<UCanvasPanel> MinimapMarkerLayer = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Minimap",
        meta = (
            BindWidget
            )
    )
    TObjectPtr<UBorder> PlayerViewIndicator = nullptr;

private:
    UPROPERTY(Transient)
    TArray<FSFMinimapMarkerEntry> MinimapMarkerEntries;
};