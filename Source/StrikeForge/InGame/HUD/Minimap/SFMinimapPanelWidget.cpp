#include "SFMinimapPanelWidget.h"

#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "Datas/Stage/SFStageData.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "Game/GameState/SFGameState.h"
#include "Game/Subsystem/SFDataTableManager.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "InGame/Controller/SFRTSCameraPawn.h"

#include "Blueprint/WidgetTree.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/Widget.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Interfaces/Units/Targetable.h"
#include "Kismet/GameplayStatics.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "TimerManager.h"

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

    if (IsValid(CombatDangerOverlay))
    {
        CombatDangerOverlay->SetVisibility(
            ESlateVisibility::Hidden
        );
    }

    if (IsValid(BuildingEventOverlay))
    {
        BuildingEventOverlay->SetVisibility(
            ESlateVisibility::Hidden
        );
    }
    else
    {
        SF_LOG(
            Warning,
            "[SFMinimapPanelWidget] "
            "BuildingEventOverlay is invalid."
        );
    }

    InitializeMinimapCapture();
    CreateMinimapMarkers();
    bInitialMarkerScanCompleted = true;
    CreateMinimapBoundsMasks();
}

void USFMinimapPanelWidget::NativeDestruct()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(
            MinimapCaptureTimerHandle
        );
    }

    ClearMinimapBoundsMasks();
    DestroyRuntimeMinimapCapture();
    ClearMinimapMarkers();

    Super::NativeDestruct();
}

void USFMinimapPanelWidget::InitializeMinimapCapture()
{
    FSFMinimapSettings Settings;

    LoadCurrentMinimapSettings(Settings);

    bool bBoundsResolved = false;

    if (Settings.bAutoDetectBounds)
    {
        bBoundsResolved =
            TryResolveMapBoundsFromNavMesh(
                Settings.BoundsPadding
            );
    }

    if (!bBoundsResolved)
    {
        PlayableWorldMin = FVector2D(
            FMath::Min(
                Settings.WorldMin.X,
                Settings.WorldMax.X
            ),
            FMath::Min(
                Settings.WorldMin.Y,
                Settings.WorldMax.Y
            )
        );

        PlayableWorldMax = FVector2D(
            FMath::Max(
                Settings.WorldMin.X,
                Settings.WorldMax.X
            ),
            FMath::Max(
                Settings.WorldMin.Y,
                Settings.WorldMax.Y
            )
        );

        ApplySquareMapBounds(
            PlayableWorldMin,
            PlayableWorldMax
        );
    }

    if (!HasValidMapBounds())
    {
        SF_LOG(
            Error,
            "[SFMinimapPanelWidget] "
            "Minimap bounds are invalid."
        );

        return;
    }

    CreateRuntimeMinimapCapture(
        Settings.CaptureHeight
    );
}

bool USFMinimapPanelWidget::LoadCurrentMinimapSettings(
    FSFMinimapSettings& OutSettings
) const
{
    USFGameInstance* GameInstance =
        Cast<USFGameInstance>(
            GetGameInstance()
        );

    if (!IsValid(GameInstance))
    {
        return false;
    }

    FName StageRowName = NAME_None;

    if (UWorld* World = GetWorld())
    {
        if (ASFGameState* GameState =
            World->GetGameState<ASFGameState>())
        {
            StageRowName =
                GameState->GetCurrentStageRowName();
        }
    }

    if (StageRowName.IsNone())
    {
        StageRowName =
            GameInstance->GetSelectedStageRowName();
    }

    // 에디터에서 레벨을 직접 실행하면 선택된 스테이지 행이
    // 없을 수 있다. 이 경우 기본 설정으로 NavMesh를 탐색한다.
    if (StageRowName.IsNone())
    {
        SF_LOG(
            Warning,
            "[SFMinimapPanelWidget] "
            "Stage RowName is None. "
            "Using default minimap settings."
        );

        return false;
    }

    USFDataTableManager* DataTableManager =
        GameInstance->GetSubsystem<
        USFDataTableManager
        >();

    if (!IsValid(DataTableManager))
    {
        return false;
    }

    const FSFStageData* StageData =
        DataTableManager->GetDataRow<
        FSFStageData
        >(
            EDataTableType::Stage,
            StageRowName,
            TEXT("LoadCurrentMinimapSettings")
        );

    if (!StageData)
    {
        return false;
    }

    OutSettings = StageData->Minimap;

    return true;
}

bool USFMinimapPanelWidget::TryResolveMapBoundsFromNavMesh(
    float BoundsPadding
)
{
    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return false;
    }

    FBox CombinedBounds(ForceInit);
    bool bFoundNavMeshBounds = false;

    for (
        TActorIterator<ANavMeshBoundsVolume> It(World);
        It;
        ++It
        )
    {
        ANavMeshBoundsVolume* BoundsVolume = *It;

        if (!IsValid(BoundsVolume))
        {
            continue;
        }

        const FBox ActorBounds =
            BoundsVolume->GetComponentsBoundingBox(
                true
            );

        if (!ActorBounds.IsValid)
        {
            continue;
        }

        CombinedBounds += ActorBounds;
        bFoundNavMeshBounds = true;
    }

    if (!bFoundNavMeshBounds)
    {
        SF_LOG(
            Warning,
            "[SFMinimapPanelWidget] "
            "NavMeshBoundsVolume was not found."
        );

        return false;
    }

    PlayableWorldMin = FVector2D(
        CombinedBounds.Min.X,
        CombinedBounds.Min.Y
    );

    PlayableWorldMax = FVector2D(
        CombinedBounds.Max.X,
        CombinedBounds.Max.Y
    );

    const float SafePadding = FMath::Max(
        0.0f,
        BoundsPadding
    );

    const FVector2D BoundsMin(
        PlayableWorldMin.X - SafePadding,
        PlayableWorldMin.Y - SafePadding
    );

    const FVector2D BoundsMax(
        PlayableWorldMax.X + SafePadding,
        PlayableWorldMax.Y + SafePadding
    );

    ApplySquareMapBounds(
        BoundsMin,
        BoundsMax
    );

    SF_LOG(
        Log,
        "[SFMinimapPanelWidget] "
        "Map bounds detected from NavMesh | "
        "Min: %s | Max: %s",
        *MapWorldMin.ToString(),
        *MapWorldMax.ToString()
    );

    return HasValidMapBounds();
}

void USFMinimapPanelWidget::ApplySquareMapBounds(
    const FVector2D& InWorldMin,
    const FVector2D& InWorldMax
)
{
    const FVector2D SafeMin(
        FMath::Min(InWorldMin.X, InWorldMax.X),
        FMath::Min(InWorldMin.Y, InWorldMax.Y)
    );

    const FVector2D SafeMax(
        FMath::Max(InWorldMin.X, InWorldMax.X),
        FMath::Max(InWorldMin.Y, InWorldMax.Y)
    );

    const FVector2D Center =
        (SafeMin + SafeMax) * 0.5f;

    const float HalfExtent =
        FMath::Max(
            SafeMax.X - SafeMin.X,
            SafeMax.Y - SafeMin.Y
        ) * 0.5f;

    if (HalfExtent <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    const FVector2D SquareExtent(
        HalfExtent,
        HalfExtent
    );

    MapWorldMin = Center - SquareExtent;
    MapWorldMax = Center + SquareExtent;
}

void USFMinimapPanelWidget::CreateRuntimeMinimapCapture(
    float CaptureHeight
)
{
    DestroyRuntimeMinimapCapture();

    if (!IsValid(MinimapRenderTarget))
    {
        SF_LOG(
            Error,
            "[SFMinimapPanelWidget] "
            "MinimapRenderTarget is not assigned."
        );

        return;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    const FVector2D MapCenter =
        (MapWorldMin + MapWorldMax) * 0.5f;

    const float OrthoWidth =
        MapWorldMax.Y - MapWorldMin.Y;

    FActorSpawnParameters SpawnParameters;

    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    SpawnParameters.ObjectFlags |= RF_Transient;

    RuntimeMinimapCaptureActor =
        World->SpawnActor<ASceneCapture2D>(
            ASceneCapture2D::StaticClass(),
            FVector(
                MapCenter.X,
                MapCenter.Y,
                FMath::Max(1000.0f, CaptureHeight)
            ),
            FRotator(
                -87.0f,
                0.0f,
                0.0f
            ),
            SpawnParameters
        );

    if (!IsValid(RuntimeMinimapCaptureActor))
    {
        SF_LOG(
            Error,
            "[SFMinimapPanelWidget] "
            "Failed to spawn SceneCapture2D."
        );

        return;
    }

    USceneCaptureComponent2D* CaptureComponent =
        RuntimeMinimapCaptureActor
        ->GetCaptureComponent2D();

    if (!IsValid(CaptureComponent))
    {
        DestroyRuntimeMinimapCapture();
        return;
    }

    CaptureComponent->ProjectionType =
        ECameraProjectionMode::Orthographic;

    CaptureComponent->OrthoWidth =
        OrthoWidth;

    // 지형은 한 번만 찍고, 움직이는 유닛과 건물은
    // 기존 미니맵 마커로 표시한다.
    CaptureComponent->bCaptureEveryFrame = false;
    CaptureComponent->bCaptureOnMovement = false;

    CaptureComponent->TextureTarget =
        MinimapRenderTarget;

    CaptureComponent->CaptureSource =
        ESceneCaptureSource::SCS_FinalColorLDR;

    World->GetTimerManager().ClearTimer(
        MinimapCaptureTimerHandle
    );

    if (MinimapCaptureDelay <= KINDA_SMALL_NUMBER)
    {
        CaptureMinimapScene();
    }
    else
    {
        World->GetTimerManager().SetTimer(
            MinimapCaptureTimerHandle,
            this,
            &USFMinimapPanelWidget::CaptureMinimapScene,
            MinimapCaptureDelay,
            false
        );
    }

    SF_LOG(
        Log,
        "[SFMinimapPanelWidget] "
        "Runtime minimap capture prepared | "
        "Center: %s | OrthoWidth: %.1f | Delay: %.2f",
        *MapCenter.ToString(),
        OrthoWidth,
        MinimapCaptureDelay
    );
}

void USFMinimapPanelWidget::CaptureMinimapScene()
{
    if (!IsValid(RuntimeMinimapCaptureActor))
    {
        return;
    }

    USceneCaptureComponent2D* CaptureComponent =
        RuntimeMinimapCaptureActor->GetCaptureComponent2D();

    if (!IsValid(CaptureComponent) ||
        !IsValid(CaptureComponent->TextureTarget))
    {
        return;
    }

    CaptureComponent->CaptureScene();

    SF_LOG(
        Log,
        "[SFMinimapPanelWidget] Delayed minimap capture completed."
    );
}

void USFMinimapPanelWidget::DestroyRuntimeMinimapCapture()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(
            MinimapCaptureTimerHandle
        );
    }

    if (IsValid(RuntimeMinimapCaptureActor))
    {
        RuntimeMinimapCaptureActor->Destroy();
    }

    RuntimeMinimapCaptureActor = nullptr;
}

void USFMinimapPanelWidget::CreateMinimapBoundsMasks()
{
    ClearMinimapBoundsMasks();

    if (
        !IsValid(MinimapMarkerLayer) ||
        !IsValid(WidgetTree)
        )
    {
        return;
    }

    auto CreateMask = [this]() -> UBorder*
        {
            UBorder* NewMask =
                WidgetTree->ConstructWidget<UBorder>(
                    UBorder::StaticClass()
                );

            if (!IsValid(NewMask))
            {
                return nullptr;
            }

            NewMask->SetBrushColor(
                FLinearColor::Black
            );

            NewMask->SetVisibility(
                ESlateVisibility::HitTestInvisible
            );

            UCanvasPanelSlot* MaskSlot =
                MinimapMarkerLayer->AddChildToCanvas(
                    NewMask
                );

            if (!IsValid(MaskSlot))
            {
                NewMask->RemoveFromParent();
                return nullptr;
            }

            MaskSlot->SetAnchors(
                FAnchors(0.0f, 0.0f)
            );

            MaskSlot->SetAlignment(
                FVector2D::ZeroVector
            );

            MaskSlot->SetAutoSize(false);
            MaskSlot->SetZOrder(100);

            return NewMask;
        };

    LeftBoundsMask = CreateMask();
    RightBoundsMask = CreateMask();
    TopBoundsMask = CreateMask();
    BottomBoundsMask = CreateMask();

    UpdateMinimapBoundsMasks();
}

void USFMinimapPanelWidget::ClearMinimapBoundsMasks()
{
    const TArray<UBorder*> BoundsMasks =
    {
        LeftBoundsMask.Get(),
        RightBoundsMask.Get(),
        TopBoundsMask.Get(),
        BottomBoundsMask.Get()
    };

    for (UBorder* BoundsMask : BoundsMasks)
    {
        if (IsValid(BoundsMask))
        {
            BoundsMask->RemoveFromParent();
        }
    }

    LeftBoundsMask = nullptr;
    RightBoundsMask = nullptr;
    TopBoundsMask = nullptr;
    BottomBoundsMask = nullptr;
    CachedMinimapMaskSize = FVector2D::ZeroVector;
}

void USFMinimapPanelWidget::UpdateMinimapBoundsMasks()
{
    if (
        !IsValid(LeftBoundsMask.Get()) ||
        !IsValid(RightBoundsMask.Get()) ||
        !IsValid(TopBoundsMask.Get()) ||
        !IsValid(BottomBoundsMask.Get()) ||
        !IsValid(MinimapMarkerLayer)
        )
    {
        return;
    }

    const FVector2D MinimapSize =
        MinimapMarkerLayer
        ->GetCachedGeometry()
        .GetLocalSize();

    FVector2D PlayableMinimapMin;
    FVector2D PlayableMinimapMax;

    if (
        MinimapSize.X <= 0.0f ||
        MinimapSize.Y <= 0.0f ||
        !GetPlayableMinimapBounds(
            PlayableMinimapMin,
            PlayableMinimapMax
        )
        )
    {
        return;
    }

    if (MinimapSize.Equals(CachedMinimapMaskSize, 0.1f))
    {
        return;
    }

    CachedMinimapMaskSize = MinimapSize;

    auto SetMaskRect = [](
        UBorder* BoundsMask,
        const FVector2D& Position,
        const FVector2D& Size
        )
        {
            if (!IsValid(BoundsMask))
            {
                return;
            }

            UCanvasPanelSlot* MaskSlot =
                Cast<UCanvasPanelSlot>(
                    BoundsMask->Slot
                );

            if (!IsValid(MaskSlot))
            {
                return;
            }

            const FVector2D SafeSize(
                FMath::Max(0.0f, Size.X),
                FMath::Max(0.0f, Size.Y)
            );

            MaskSlot->SetPosition(Position);
            MaskSlot->SetSize(SafeSize);

            BoundsMask->SetVisibility(
                SafeSize.X > KINDA_SMALL_NUMBER &&
                SafeSize.Y > KINDA_SMALL_NUMBER
                ? ESlateVisibility::HitTestInvisible
                : ESlateVisibility::Collapsed
            );
        };

    SetMaskRect(
        LeftBoundsMask.Get(),
        FVector2D::ZeroVector,
        FVector2D(
            PlayableMinimapMin.X,
            MinimapSize.Y
        )
    );

    SetMaskRect(
        RightBoundsMask.Get(),
        FVector2D(
            PlayableMinimapMax.X,
            0.0f
        ),
        FVector2D(
            MinimapSize.X - PlayableMinimapMax.X,
            MinimapSize.Y
        )
    );

    SetMaskRect(
        TopBoundsMask.Get(),
        FVector2D(
            PlayableMinimapMin.X,
            0.0f
        ),
        FVector2D(
            PlayableMinimapMax.X -
            PlayableMinimapMin.X,
            PlayableMinimapMin.Y
        )
    );

    SetMaskRect(
        BottomBoundsMask.Get(),
        FVector2D(
            PlayableMinimapMin.X,
            PlayableMinimapMax.Y
        ),
        FVector2D(
            PlayableMinimapMax.X -
            PlayableMinimapMin.X,
            MinimapSize.Y - PlayableMinimapMax.Y
        )
    );
}

bool USFMinimapPanelWidget::GetPlayableMinimapBounds(
    FVector2D& OutMinimapMin,
    FVector2D& OutMinimapMax
) const
{
    if (
        !HasValidMapBounds() ||
        PlayableWorldMax.X <= PlayableWorldMin.X ||
        PlayableWorldMax.Y <= PlayableWorldMin.Y ||
        !IsValid(MinimapMarkerLayer)
        )
    {
        return false;
    }

    const FVector2D TopLeft =
        WorldToMinimapPosition(
            FVector(
                PlayableWorldMax.X,
                PlayableWorldMin.Y,
                0.0f
            )
        );

    const FVector2D BottomRight =
        WorldToMinimapPosition(
            FVector(
                PlayableWorldMin.X,
                PlayableWorldMax.Y,
                0.0f
            )
        );

    OutMinimapMin = FVector2D(
        FMath::Min(TopLeft.X, BottomRight.X),
        FMath::Min(TopLeft.Y, BottomRight.Y)
    );

    OutMinimapMax = FVector2D(
        FMath::Max(TopLeft.X, BottomRight.X),
        FMath::Max(TopLeft.Y, BottomRight.Y)
    );

    return
        OutMinimapMax.X > OutMinimapMin.X &&
        OutMinimapMax.Y > OutMinimapMin.Y;
}

FVector2D USFMinimapPanelWidget::ClampToPlayableMinimapBounds(
    const FVector2D& MinimapPosition
) const
{
    FVector2D PlayableMinimapMin;
    FVector2D PlayableMinimapMax;

    if (
        !GetPlayableMinimapBounds(
            PlayableMinimapMin,
            PlayableMinimapMax
        )
        )
    {
        return MinimapPosition;
    }

    return FVector2D(
        FMath::Clamp(
            MinimapPosition.X,
            PlayableMinimapMin.X,
            PlayableMinimapMax.X
        ),
        FMath::Clamp(
            MinimapPosition.Y,
            PlayableMinimapMin.Y,
            PlayableMinimapMax.Y
        )
    );
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

    UpdateMinimapBoundsMasks();
    UpdatePlayerViewIndicator();
    UpdateMinimapMarkers(InDeltaTime);
    UpdateCombatDangerOverlay(InDeltaTime);
    UpdateBuildingEventOverlay(InDeltaTime);
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
        CommanderMarkerActorTag,
        ESFMinimapMarkerType::Commander
    );

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

        ESFMinimapMarkerType ResolvedMarkerType = MarkerType;

        if (MarkerType == ESFMinimapMarkerType::Building)
        {
            const ASFBaseBuilding* Building =
                Cast<ASFBaseBuilding>(FoundActor);

            ASFInGamePlayerController* PlayerController =
                Cast<ASFInGamePlayerController>(GetOwningPlayer());

            if (
                IsValid(Building) &&
                IsValid(PlayerController)
                )
            {
                const int32 BuildingTeam =
                    Building->GetTeamIndex();

                const int32 LocalTeam =
                    PlayerController->GetMyTeamIndex();

                if (BuildingTeam == LocalTeam)
                {
                    ResolvedMarkerType =
                        ESFMinimapMarkerType::AllyBuilding;
                }
                else if (
                    BuildingTeam != Targetable::NeutralTeam
                    )
                {
                    ResolvedMarkerType =
                        ESFMinimapMarkerType::EnemyBuilding;
                }
            }
        }

        CreateMarkerForActor(
            FoundActor,
            ResolvedMarkerType
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

    ASFBaseBuilding* Building =
        Cast<ASFBaseBuilding>(TargetActor);

    if (IsValid(Building))
    {
        MarkerEntry.bWasTierActivated =
            Building->IsTierActivated();

        MarkerEntry.LastKnownTier =
            Building->GetCurrentTier();
    }

    MinimapMarkerEntries.Add(
        MarkerEntry
    );

    if (
        bInitialMarkerScanCompleted &&
        IsValid(Building) &&
        Building->IsTierActivated()
        )
    {
        if (ASFInGamePlayerController* PlayerController =
            Cast<ASFInGamePlayerController>(
                GetOwningPlayer()
            ))
        {
            if (
                Building->GetTeamIndex() ==
                PlayerController->GetMyTeamIndex()
                )
            {
                SF_LOG(
                    Log,
                    "[Minimap] Newly completed building detected | Actor: %s",
                    *Building->GetName()
                );

                StartBuildingEventFlash(Building);
            }
        }
    }

    if (UCombatTargetComponent* CombatTarget =
        TargetActor->FindComponentByClass<UCombatTargetComponent>())
    {
        CombatTarget->OnCombatDamaged.AddUniqueDynamic(
            this,
            &USFMinimapPanelWidget::HandleCombatTargetDamaged
        );
    }
}

void USFMinimapPanelWidget::ClearMinimapMarkers()
{
    for (
        FSFMinimapMarkerEntry& MarkerEntry :
        MinimapMarkerEntries
        )
    {
        AActor* TargetActor =
            MarkerEntry.TargetActor.Get();

        if (IsValid(TargetActor))
        {
            if (UCombatTargetComponent* CombatTarget =
                TargetActor->FindComponentByClass<UCombatTargetComponent>())
            {
                CombatTarget->OnCombatDamaged.RemoveDynamic(
                    this,
                    &USFMinimapPanelWidget::HandleCombatTargetDamaged
                );
            }
        }

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

void USFMinimapPanelWidget::UpdateMinimapMarkers(
    float InDeltaTime
)
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

        if (MarkerEntry.DangerTimeRemaining > 0.0f)
        {
            MarkerEntry.DangerTimeRemaining =
                FMath::Max(
                    0.0f,
                    MarkerEntry.DangerTimeRemaining - InDeltaTime
                );

            if (MarkerEntry.DangerTimeRemaining > 0.0f)
            {
                MarkerWidget->SetBrushColor(
                    DangerMarkerColor
                );
            }
            else
            {
                MarkerWidget->SetBrushColor(
                    GetMarkerColor(
                        MarkerEntry.MarkerType
                    )
                );
            }
        }

        if (
            bBuildingEventFlashActive &&
            BuildingEventTarget.Get() == MarkerEntry.TargetActor.Get() &&
            (
                MarkerEntry.MarkerType == ESFMinimapMarkerType::Building ||
                MarkerEntry.MarkerType == ESFMinimapMarkerType::AllyBuilding ||
                MarkerEntry.MarkerType == ESFMinimapMarkerType::EnemyBuilding
                ) &&
            MarkerEntry.DangerTimeRemaining <= 0.0f
            )
        {
            MarkerWidget->SetBrushColor(
                bBuildingEventOverlayVisible
                ? FLinearColor(0.05f, 1.0f, 0.15f, 1.0f)
                : GetMarkerColor(MarkerEntry.MarkerType)
            );
        }

        if (ASFBaseBuilding* Building =
            Cast<ASFBaseBuilding>(TargetActor))
        {
            const bool bNowTierActivated =
                Building->IsTierActivated();

            const int32 NowTier =
                Building->GetCurrentTier();

            const bool bConstructionCompleted =
                !MarkerEntry.bWasTierActivated &&
                bNowTierActivated;

            const bool bUpgradeCompleted =
                MarkerEntry.bWasTierActivated &&
                bNowTierActivated &&
                MarkerEntry.LastKnownTier != NowTier;

            if (
                bConstructionCompleted ||
                bUpgradeCompleted
                )
            {
                if (ASFInGamePlayerController* PlayerController =
                    Cast<ASFInGamePlayerController>(
                        GetOwningPlayer()
                    ))
                {
                    if (
                        Building->GetTeamIndex() ==
                        PlayerController->GetMyTeamIndex()
                        )
                    {
                        StartBuildingEventFlash(Building);
                    }
                }
            }

            MarkerEntry.bWasTierActivated =
                bNowTierActivated;

            MarkerEntry.LastKnownTier =
                NowTier;
        }

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

    FVector2D AllowedMinimapMin =
        FVector2D::ZeroVector;

    FVector2D AllowedMinimapMax =
        MinimapSize;

    if (MarkerWidget == PlayerViewIndicator.Get())
    {
        GetPlayableMinimapBounds(
            AllowedMinimapMin,
            AllowedMinimapMax
        );
    }

    const FVector2D MinimumPosition(
        AllowedMinimapMin.X +
        MarkerSize.X *
        MarkerAlignment.X,

        AllowedMinimapMin.Y +
        MarkerSize.Y *
        MarkerAlignment.Y
    );

    const FVector2D MaximumPosition(
        FMath::Max(
            MinimumPosition.X,

            AllowedMinimapMax.X -
            MarkerSize.X *
            (
                1.0f -
                MarkerAlignment.X
                )
        ),

        FMath::Max(
            MinimumPosition.Y,

            AllowedMinimapMax.Y -
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

    case ESFMinimapMarkerType::AllyBuilding:

    case ESFMinimapMarkerType::EnemyBuilding:
        return BuildingMarkerSize;

    case ESFMinimapMarkerType::Commander:
        return CommanderMarkerSize;

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

    case ESFMinimapMarkerType::AllyBuilding:
        return AllyMarkerColor;

    case ESFMinimapMarkerType::EnemyBuilding:
        return EnemyMarkerColor;

    case ESFMinimapMarkerType::Commander:
        return CommanderMarkerColor;

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

    const FVector2D ClampedLocalPosition =
        ClampToPlayableMinimapBounds(
            LocalPosition
        );

    if (!LocalPosition.Equals(ClampedLocalPosition, 0.5f))
    {
        return FReply::Handled();
    }

    const FVector WorldLocation =
        MinimapToWorldLocation(
            ClampedLocalPosition
        );

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

    const FVector2D LocalPosition =
        MinimapGeometry.AbsoluteToLocal(
            ScreenPosition
        );

    const FVector2D ClampedLocalPosition =
        ClampToPlayableMinimapBounds(
            LocalPosition
        );

    const FVector WorldLocation =
        MinimapToWorldLocation(
            ClampedLocalPosition
        );

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

void USFMinimapPanelWidget::HandleCombatTargetDamaged(
    UCombatTargetComponent* InTarget,
    float InDamage,
    AActor* InInstigator
)
{
    if (!IsValid(InTarget))
    {
        return;
    }

    AActor* DamagedActor = InTarget->GetOwner();

    if (!IsValid(DamagedActor))
    {
        return;
    }

    for (FSFMinimapMarkerEntry& MarkerEntry :
        MinimapMarkerEntries)
    {
        if (MarkerEntry.TargetActor.Get() != DamagedActor)
        {
            continue;
        }

        MarkerEntry.DangerTimeRemaining =
            DangerMarkerDuration;

        if (UBorder* MarkerWidget =
            MarkerEntry.MarkerWidget.Get())
        {
            MarkerWidget->SetBrushColor(
                DangerMarkerColor
            );
        }

        break;
    }

    if (IsValid(CombatDangerOverlay))
    {
        CombatDangerFlashElapsed = 0.0f;
        CombatDangerFlashToggleCount = 0;
        bCombatDangerFlashActive = true;
        bCombatDangerOverlayVisible = true;

        CombatDangerOverlay->SetVisibility(
            ESlateVisibility::Visible
        );
    }
}

void USFMinimapPanelWidget::UpdateCombatDangerOverlay(
    float InDeltaTime
)
{
    if (
        !bCombatDangerFlashActive ||
        !IsValid(CombatDangerOverlay)
        )
    {
        return;
    }

    CombatDangerFlashElapsed += InDeltaTime;

    while (
        CombatDangerFlashElapsed >=
        CombatDangerFlashHalfPeriod &&
        CombatDangerFlashToggleCount < 4
        )
    {
        CombatDangerFlashElapsed -=
            CombatDangerFlashHalfPeriod;

        ++CombatDangerFlashToggleCount;

        bCombatDangerOverlayVisible =
            !bCombatDangerOverlayVisible;

        CombatDangerOverlay->SetVisibility(
            bCombatDangerOverlayVisible
            ? ESlateVisibility::Visible
            : ESlateVisibility::Hidden
        );
    }

    if (CombatDangerFlashToggleCount >= 4)
    {
        CombatDangerOverlay->SetVisibility(
            ESlateVisibility::Hidden
        );

        bCombatDangerFlashActive = false;
        bCombatDangerOverlayVisible = false;
        CombatDangerFlashElapsed = 0.0f;
        CombatDangerFlashToggleCount = 0;
    }
}

void USFMinimapPanelWidget::StartBuildingEventFlash(
    AActor* InBuilding
)
{
    BuildingEventTarget = InBuilding;

    bBuildingEventFlashActive = true;
    BuildingEventFlashElapsed = 0.0f;
    BuildingEventFlashToggleCount = 0;
    bBuildingEventOverlayVisible = true;

    if (IsValid(BuildingEventOverlay))
    {
        BuildingEventOverlay->SetVisibility(
            ESlateVisibility::Visible
        );
    }
}

void USFMinimapPanelWidget::UpdateBuildingEventOverlay(
    float InDeltaTime
)
{
    if (
        !bBuildingEventFlashActive ||
        !IsValid(BuildingEventOverlay)
        )
    {
        return;
    }

    BuildingEventFlashElapsed += InDeltaTime;

    while (
        BuildingEventFlashElapsed >=
        BuildingEventFlashHalfPeriod &&
        BuildingEventFlashToggleCount < 4
        )
    {
        BuildingEventFlashElapsed -=
            BuildingEventFlashHalfPeriod;

        ++BuildingEventFlashToggleCount;

        bBuildingEventOverlayVisible =
            !bBuildingEventOverlayVisible;

        BuildingEventOverlay->SetVisibility(
            bBuildingEventOverlayVisible
            ? ESlateVisibility::Visible
            : ESlateVisibility::Hidden
        );
    }

    if (BuildingEventFlashToggleCount >= 4)
    {
        BuildingEventOverlay->SetVisibility(
            ESlateVisibility::Hidden
        );

        bBuildingEventFlashActive = false;
        bBuildingEventOverlayVisible = false;
        BuildingEventFlashElapsed = 0.0f;
        BuildingEventFlashToggleCount = 0;
    }
}