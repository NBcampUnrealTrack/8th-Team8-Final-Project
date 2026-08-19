#include "SFMinimapPanelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Widget.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "InGame/Controller/SFRTSCameraPawn.h"
#include "InputCoreTypes.h"
#include "Logging/SFLog.h"

bool USFMinimapPanelWidget::HasValidMapBounds() const
{
    return
        MapWorldMax.X > MapWorldMin.X &&
        MapWorldMax.Y > MapWorldMin.Y;
}

FVector2D USFMinimapPanelWidget::WorldToMinimapPosition(
    const FVector& WorldLocation
) const
{
    if (
        !HasValidMapBounds() ||
        !IsValid(MinimapMarkerLayer)
        )
    {
        return FVector2D::ZeroVector;
    }

    const FVector2D MinimapSize =
        MinimapMarkerLayer
        ->GetCachedGeometry()
        .GetLocalSize();

    if (
        MinimapSize.X <= 0.0f ||
        MinimapSize.Y <= 0.0f
        )
    {
        return FVector2D::ZeroVector;
    }

    const float NormalizedHorizontal =
        FMath::Clamp(
            (
                WorldLocation.Y -
                MapWorldMin.Y
                ) /
            (
                MapWorldMax.Y -
                MapWorldMin.Y
                ),
            0.0f,
            1.0f
        );

    const float NormalizedVertical =
        FMath::Clamp(
            (
                WorldLocation.X -
                MapWorldMin.X
                ) /
            (
                MapWorldMax.X -
                MapWorldMin.X
                ),
            0.0f,
            1.0f
        );

    const float MinimapX =
        NormalizedHorizontal *
        MinimapSize.X;

    // UMG의 Y축은 아래쪽으로 증가하므로 반전한다.
    const float MinimapY =
        (
            1.0f -
            NormalizedVertical
            ) *
        MinimapSize.Y;

    return FVector2D(
        MinimapX,
        MinimapY
    );
}

void USFMinimapPanelWidget::NativeConstruct()
{
    Super::NativeConstruct();

    CreateMinimapMarkers();
}

void USFMinimapPanelWidget::NativeDestruct()
{
    ClearMinimapMarkers();

    Super::NativeDestruct();
}

void USFMinimapPanelWidget::NativeTick(
    const FGeometry& MyGeometry,
    float InDeltaTime
)
{
    Super::NativeTick(
        MyGeometry,
        InDeltaTime
    );

    UpdatePlayerViewIndicator();
    UpdateMinimapMarkers();
}

void USFMinimapPanelWidget::CreateMinimapMarkers()
{
    ClearMinimapMarkers();

    if (
        !IsValid(MinimapMarkerLayer) ||
        !IsValid(WidgetTree)
        )
    {
        return;
    }

    CreateMarkersForTag(
        AllyMarkerActorTag,
        ESFMinimapMarkerType::Ally
    );

    CreateMarkersForTag(
        EnemyMarkerActorTag,
        ESFMinimapMarkerType::Enemy
    );

    CreateMarkersForTag(
        BuildingMarkerActorTag,
        ESFMinimapMarkerType::Building
    );
}

void USFMinimapPanelWidget::CreateMarkersForTag(
    FName ActorTag,
    ESFMinimapMarkerType MarkerType
)
{
    if (ActorTag.IsNone())
    {
        return;
    }

    TArray<AActor*> FoundActors;

    UGameplayStatics::GetAllActorsWithTag(
        this,
        ActorTag,
        FoundActors
    );

    for (AActor* FoundActor : FoundActors)
    {
        if (!IsValid(FoundActor))
        {
            continue;
        }

        const bool bAlreadyRegistered =
            MinimapMarkerEntries.ContainsByPredicate(
                [FoundActor](
                    const FSFMinimapMarkerEntry& MarkerEntry
                    )
                {
                    return
                        MarkerEntry.TargetActor.Get() ==
                        FoundActor;
                }
            );

        if (bAlreadyRegistered)
        {
            continue;
        }

        CreateMarkerForActor(
            FoundActor,
            MarkerType
        );
    }
}

void USFMinimapPanelWidget::CreateMarkerForActor(
    AActor* TargetActor,
    ESFMinimapMarkerType MarkerType
)
{
    if (
        !IsValid(TargetActor) ||
        MarkerType == ESFMinimapMarkerType::None ||
        !IsValid(MinimapMarkerLayer) ||
        !IsValid(WidgetTree)
        )
    {
        return;
    }

    UBorder* NewMarker =
        WidgetTree->ConstructWidget<UBorder>(
            UBorder::StaticClass()
        );

    if (!IsValid(NewMarker))
    {
        return;
    }

    NewMarker->SetBrushColor(
        GetMarkerColor(
            MarkerType
        )
    );

    NewMarker->SetVisibility(
        ESlateVisibility::HitTestInvisible
    );

    UCanvasPanelSlot* MarkerCanvasSlot =
        MinimapMarkerLayer->AddChildToCanvas(
            NewMarker
        );

    if (!IsValid(MarkerCanvasSlot))
    {
        NewMarker->RemoveFromParent();
        return;
    }

    MarkerCanvasSlot->SetAnchors(
        FAnchors(
            0.0f,
            0.0f
        )
    );

    MarkerCanvasSlot->SetAlignment(
        FVector2D(
            0.5f,
            0.5f
        )
    );

    MarkerCanvasSlot->SetAutoSize(
        false
    );

    MarkerCanvasSlot->SetSize(
        GetMarkerSize(
            MarkerType
        )
    );

    MarkerCanvasSlot->SetPosition(
        FVector2D::ZeroVector
    );

    MarkerCanvasSlot->SetZOrder(
        MarkerType ==
        ESFMinimapMarkerType::Building
        ? 1
        : 2
    );

    FSFMinimapMarkerEntry MarkerEntry;

    MarkerEntry.TargetActor =
        TargetActor;

    MarkerEntry.MarkerWidget =
        NewMarker;

    MarkerEntry.MarkerType =
        MarkerType;

    MinimapMarkerEntries.Add(
        MarkerEntry
    );
}

void USFMinimapPanelWidget::ClearMinimapMarkers()
{
    for (
        FSFMinimapMarkerEntry& MarkerEntry :
        MinimapMarkerEntries
        )
    {
        UBorder* MarkerWidget =
            MarkerEntry.MarkerWidget.Get();

        if (IsValid(MarkerWidget))
        {
            MarkerWidget->RemoveFromParent();
        }
    }

    MinimapMarkerEntries.Reset();
}

void USFMinimapPanelWidget::UpdatePlayerViewIndicator()
{
    if (
        !IsValid(MinimapMarkerLayer) ||
        !IsValid(PlayerViewIndicator)
        )
    {
        return;
    }

    APlayerController* PlayerController =
        GetOwningPlayer();

    if (!IsValid(PlayerController))
    {
        return;
    }

    FVector CameraLocation;
    FRotator CameraRotation;

    PlayerController->GetPlayerViewPoint(
        CameraLocation,
        CameraRotation
    );

    const FVector ViewDirection =
        CameraRotation.Vector();

    if (FMath::IsNearlyZero(ViewDirection.Z))
    {
        return;
    }

    constexpr float GroundZ = 0.0f;

    const float DistanceToGround =
        (
            GroundZ -
            CameraLocation.Z
            ) /
        ViewDirection.Z;

    if (DistanceToGround < 0.0f)
    {
        return;
    }

    const FVector CameraFocusLocation =
        CameraLocation +
        ViewDirection *
        DistanceToGround;

    const FVector2D MinimapPosition =
        WorldToMinimapPosition(
            CameraFocusLocation
        );

    SetMarkerLayerPosition(
        PlayerViewIndicator,
        MinimapPosition
    );
}

void USFMinimapPanelWidget::UpdateMinimapMarkers()
{
    for (
        int32 Index =
        MinimapMarkerEntries.Num() - 1;

        Index >= 0;

        --Index
        )
    {
        FSFMinimapMarkerEntry& MarkerEntry =
            MinimapMarkerEntries[Index];

        AActor* TargetActor =
            MarkerEntry.TargetActor.Get();

        UBorder* MarkerWidget =
            MarkerEntry.MarkerWidget.Get();

        if (
            !IsValid(TargetActor) ||
            !IsValid(MarkerWidget)
            )
        {
            if (IsValid(MarkerWidget))
            {
                MarkerWidget->RemoveFromParent();
            }

            MinimapMarkerEntries.RemoveAt(
                Index
            );

            continue;
        }

        const FVector2D MinimapPosition =
            WorldToMinimapPosition(
                TargetActor->GetActorLocation()
            );

        SetMarkerLayerPosition(
            MarkerWidget,
            MinimapPosition
        );
    }
}

void USFMinimapPanelWidget::SetMarkerLayerPosition(
    UWidget* MarkerWidget,
    const FVector2D& DesiredPosition
) const
{
    if (
        !IsValid(MarkerWidget) ||
        !IsValid(MinimapMarkerLayer)
        )
    {
        return;
    }

    UCanvasPanelSlot* MarkerCanvasSlot =
        Cast<UCanvasPanelSlot>(
            MarkerWidget->Slot
        );

    if (!IsValid(MarkerCanvasSlot))
    {
        return;
    }

    const FVector2D MinimapSize =
        MinimapMarkerLayer
        ->GetCachedGeometry()
        .GetLocalSize();

    const FVector2D MarkerSize =
        MarkerCanvasSlot->GetSize();

    const FVector2D MarkerAlignment =
        MarkerCanvasSlot->GetAlignment();

    const FVector2D MinimumPosition(
        MarkerSize.X *
        MarkerAlignment.X,

        MarkerSize.Y *
        MarkerAlignment.Y
    );

    const FVector2D MaximumPosition(
        FMath::Max(
            MinimumPosition.X,

            MinimapSize.X -
            MarkerSize.X *
            (
                1.0f -
                MarkerAlignment.X
                )
        ),

        FMath::Max(
            MinimumPosition.Y,

            MinimapSize.Y -
            MarkerSize.Y *
            (
                1.0f -
                MarkerAlignment.Y
                )
        )
    );

    const FVector2D ClampedPosition(
        FMath::Clamp(
            DesiredPosition.X,
            MinimumPosition.X,
            MaximumPosition.X
        ),

        FMath::Clamp(
            DesiredPosition.Y,
            MinimumPosition.Y,
            MaximumPosition.Y
        )
    );

    MarkerCanvasSlot->SetPosition(
        ClampedPosition
    );
}

FVector2D USFMinimapPanelWidget::GetMarkerSize(
    ESFMinimapMarkerType MarkerType
) const
{
    switch (MarkerType)
    {
    case ESFMinimapMarkerType::Ally:
        return AllyMarkerSize;

    case ESFMinimapMarkerType::Enemy:
        return EnemyMarkerSize;

    case ESFMinimapMarkerType::Building:
        return BuildingMarkerSize;

    case ESFMinimapMarkerType::None:
    default:
        return FVector2D::ZeroVector;
    }
}

FLinearColor USFMinimapPanelWidget::GetMarkerColor(
    ESFMinimapMarkerType MarkerType
) const
{
    switch (MarkerType)
    {
    case ESFMinimapMarkerType::Ally:
        return AllyMarkerColor;

    case ESFMinimapMarkerType::Enemy:
        return EnemyMarkerColor;

    case ESFMinimapMarkerType::Building:
        return BuildingMarkerColor;

    case ESFMinimapMarkerType::None:
    default:
        return FLinearColor::Transparent;
    }
}

FVector USFMinimapPanelWidget::MinimapToWorldLocation(const FVector2D& MinimapPosition) const
{
    if (!HasValidMapBounds() || !IsValid(MinimapMarkerLayer))
    {
        return FVector::ZeroVector;
    }

    const FVector2D MinimapSize = MinimapMarkerLayer->GetCachedGeometry().GetLocalSize();

    if (MinimapSize.X <= 0.0f || MinimapSize.Y <= 0.0f)
    {
        return FVector::ZeroVector;
    }

    const float NormalizedHorizontal = FMath::Clamp(MinimapPosition.X / MinimapSize.X, 0.0f, 1.0f);
    const float NormalizedVertical = FMath::Clamp(MinimapPosition.Y / MinimapSize.Y, 0.0f, 1.0f);

    return FVector(
        FMath::Lerp(MapWorldMin.X, MapWorldMax.X, 1.0f - NormalizedVertical),
        FMath::Lerp(MapWorldMin.Y, MapWorldMax.Y, NormalizedHorizontal),
        0.0f
    );
}

FReply USFMinimapPanelWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (!IsValid(MinimapMarkerLayer))
    {
        return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
    }

    const FGeometry MinimapGeometry = MinimapMarkerLayer->GetCachedGeometry();
    const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();

    if (!MinimapGeometry.IsUnderLocation(ScreenPosition))
    {
        return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
    }

    const FVector2D LocalPosition = MinimapGeometry.AbsoluteToLocal(ScreenPosition);
    const FVector WorldLocation = MinimapToWorldLocation(LocalPosition);

    ASFInGamePlayerController* PlayerController = Cast<ASFInGamePlayerController>(GetOwningPlayer());

    if (!IsValid(PlayerController))
    {
        return FReply::Handled();
    }

    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bIsMinimapCameraDragging = true;
        MoveCameraToPointerPosition(InMouseEvent);

        SF_LOG(Log, "[SFMinimapPanelWidget] Camera drag started.");

        return FReply::Handled().CaptureMouse(TakeWidget());
    }

    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        PlayerController->IssueMoveCommand(WorldLocation);
        SF_LOG(Log, "[SFMinimapPanelWidget] Move requested | Location: %s", *WorldLocation.ToString());
        return FReply::Handled();
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void USFMinimapPanelWidget::MoveCameraToPointerPosition(const FPointerEvent& InMouseEvent)
{
    if (!IsValid(MinimapMarkerLayer))
    {
        return;
    }

    const FGeometry MinimapGeometry = MinimapMarkerLayer->GetCachedGeometry();
    const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();

    if (!MinimapGeometry.IsUnderLocation(ScreenPosition))
    {
        return;
    }

    ASFInGamePlayerController* PlayerController = Cast<ASFInGamePlayerController>(GetOwningPlayer());

    if (!IsValid(PlayerController))
    {
        return;
    }

    ASFRTSCameraPawn* CameraPawn = Cast<ASFRTSCameraPawn>(PlayerController->GetPawn());

    if (!IsValid(CameraPawn))
    {
        return;
    }

    const FVector2D LocalPosition = MinimapGeometry.AbsoluteToLocal(ScreenPosition);
    const FVector WorldLocation = MinimapToWorldLocation(LocalPosition);

    CameraPawn->FocusOnWorldLocation(WorldLocation);
}

FReply USFMinimapPanelWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (!bIsMinimapCameraDragging)
    {
        return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
    }

    if (!InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
    {
        bIsMinimapCameraDragging = false;
        return FReply::Handled().ReleaseMouseCapture();
    }

    MoveCameraToPointerPosition(InMouseEvent);
    return FReply::Handled();
}

FReply USFMinimapPanelWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bIsMinimapCameraDragging)
    {
        bIsMinimapCameraDragging = false;

        SF_LOG(Log, "[SFMinimapPanelWidget] Camera drag ended.");

        return FReply::Handled().ReleaseMouseCapture();
    }

    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}