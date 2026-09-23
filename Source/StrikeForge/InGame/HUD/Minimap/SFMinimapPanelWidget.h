#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"
#include "SFMinimapMarkerInfo.h"
#include "TimerManager.h"
#include "SFMinimapPanelWidget.generated.h"

class AActor;
class ASceneCapture2D;
class UBorder;
class UImage;
class USFMinimapViewIndicatorWidget;
class UCanvasPanel;
class UCombatTargetComponent;
class UTextureRenderTarget2D;
class UWidget;
struct FSFMinimapSettings;

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

    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
    friend class FSFMinimapRegistryTest;
    void BindMarkerRegistry();
    void UnbindMarkerRegistry();
    void QueueMarkerActor(AActor* Actor);
    void HandleSourceRegistered(UCombatTargetComponent* Source);
    void HandleSourceUnregistered(UCombatTargetComponent* Source);
    void ProcessPendingMarkers();
    void RemoveMarkerAt(int32 Index);
    ESFMinimapMarkerType ResolveMarkerType(AActor* Actor) const;
    TMap<TWeakObjectPtr<AActor>, int32> ActorToMarker;
    TSet<TWeakObjectPtr<AActor>> PendingMarkerActors;
    FDelegateHandle ActorSpawnedHandle;
    void InitializeMinimapCapture();
    void UpdateFogOverlay();

    bool LoadCurrentMinimapSettings(
        FSFMinimapSettings& OutSettings
    ) const;

    bool TryResolveMapBoundsFromNavMesh(
        float BoundsPadding
    );

    void ApplySquareMapBounds(
        const FVector2D& InWorldMin,
        const FVector2D& InWorldMax
    );

    void CreateRuntimeMinimapCapture(
        float CaptureHeight
    );

    void CaptureMinimapScene();

    void DestroyRuntimeMinimapCapture();

    void CreateMinimapBoundsMasks();

    void ClearMinimapBoundsMasks();

    void UpdateMinimapBoundsMasks();

    bool GetPlayableMinimapBounds(
        FVector2D& OutMinimapMin,
        FVector2D& OutMinimapMax
    ) const;

    FVector2D ClampToPlayableMinimapBounds(
        const FVector2D& MinimapPosition
    ) const;

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

    void UpdateMinimapMarkers(float InDeltaTime);

    void UpdateCombatDangerOverlay(float InDeltaTime);

    UFUNCTION()
    void HandleCombatTargetDamaged(
        UCombatTargetComponent* InTarget,
        float InDamage,
        AActor* InInstigator
    );

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

    void UpdateBuildingEventOverlay(
        float InDeltaTime
    );

    void StartBuildingEventFlash();

    bool bInitialMarkerScanCompleted = false;

    bool IsActorOnMainScreen(AActor* Actor) const;

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
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "SF|Minimap|World Bounds"
    )
    FVector2D PlayableWorldMin =
        FVector2D(
            -5000.0f,
            -5000.0f
        );

    UPROPERTY(
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "SF|Minimap|World Bounds"
    )
    FVector2D PlayableWorldMax =
        FVector2D(
            5000.0f,
            5000.0f
        );

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Minimap|Capture"
    )
    TObjectPtr<UTextureRenderTarget2D>
        MinimapRenderTarget = nullptr;

    /**
     * 레벨 시작 후 실제 미니맵 장면을 캡처하기까지 기다리는 시간.
     * 스트리밍된 지형과 머티리얼이 준비될 시간을 준다.
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Minimap|Capture",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float MinimapCaptureDelay = 1.5f;

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
        Category = "SF|Minimap|Marker|Tag"
	)
    FName CommanderMarkerActorTag =
        TEXT("MinimapCommander");

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Minimap|Marker|Size"
    )
    FVector2D AllyMarkerSize =
        FVector2D(
            4.0f,
            4.0f
        );

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Minimap|Marker|Size"
    )
    FVector2D EnemyMarkerSize = FVector2D(4.0f, 4.0f);

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Minimap|Marker|Size"
    )
    FVector2D BuildingMarkerSize = FVector2D(7.0f, 7.0f);

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Minimap|Marker|Size"
    )
    FVector2D CommanderMarkerSize = FVector2D(7.0f, 7.0f);

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Minimap|Marker|Position"
    )
    FVector2D CommanderMarkerPositionOffset = FVector2D(0.0f,6.0f);

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Minimap|Marker|Color"
    )
    FLinearColor AllyMarkerColor =
        FLinearColor(
            0.0f,
            0.8f,
            0.95f,
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
            0.05f,
            0.05f,
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
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Minimap|Marker|Color"
    )
    FLinearColor CommanderMarkerColor =
        FLinearColor(
            1.0f, 
            0.75f, 
            0.05f, 
            1.0f
        );

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Minimap|Marker|Color"
    )
    FLinearColor DangerMarkerColor =
        FLinearColor(
            1.0f,
            0.0f,
            0.0f,
            1.0f
        );

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Minimap|Marker|Effect",
        meta = (ClampMin = "0.0", Units = "s")
    )
    float DangerMarkerDuration = 1.0f;

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
        meta = (BindWidget)
    )
TObjectPtr<USFMinimapViewIndicatorWidget>
    PlayerViewIndicator = nullptr;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Minimap|View Indicator",
        meta = (ClampMin = "0.1",ClampMax = "1.0")
    )
    float PlayerViewIndicatorScale = 0.9f;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Minimap",
        meta = (BindWidget)
    )
    TObjectPtr<UBorder> CombatDangerOverlay = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Minimap",
        meta = (BindWidget)
    )
    TObjectPtr<UBorder> BuildingEventOverlay = nullptr;

private:
    UPROPERTY(Transient)
    TArray<FSFMinimapMarkerEntry> MinimapMarkerEntries;

    UPROPERTY(Transient)
    TObjectPtr<UImage> FogOverlay = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<ASceneCapture2D>
        RuntimeMinimapCaptureActor = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UBorder> LeftBoundsMask = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UBorder> RightBoundsMask = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UBorder> TopBoundsMask = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UBorder> BottomBoundsMask = nullptr;

    FVector2D CachedMinimapMaskSize =
        FVector2D::ZeroVector;

    FTimerHandle MinimapCaptureTimerHandle;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "SF|Minimap|Combat Danger",
        meta = (ClampMin = "0.01", Units = "s")
    )
    float CombatDangerFlashHalfPeriod = 0.15f;

    float CombatDangerFlashElapsed = 0.0f;

    int32 CombatDangerFlashToggleCount = 0;

    bool bCombatDangerFlashActive = false;

    bool bCombatDangerOverlayVisible = false;

    UPROPERTY(
        EditDefaultsOnly,
        Category = "SF|Minimap|Building Event",
        meta = (ClampMin = "0.01", Units = "s")
    )
    float BuildingEventFlashHalfPeriod = 0.15f;

    float BuildingEventFlashElapsed = 0.0f;

    int32 BuildingEventFlashToggleCount = 0;

    bool bBuildingEventFlashActive = false;

    bool bBuildingEventOverlayVisible = false;

    void StartBuildingEventFlash(AActor* InBuilding);

    TWeakObjectPtr<AActor> BuildingEventTarget;
};
