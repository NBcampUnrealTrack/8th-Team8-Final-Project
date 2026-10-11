#include "SFOverheadUISubsystem.h"

#include "ActorComponent/UI/SFOverheadStatusComponent.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Buildings/UI/SFOverheadPanelWidget.h"
#include "Buildings/UI/Synergy/SFBuildingSynergyBadgeWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Game/Subsystem/SFBuildingSynergySubsystem.h"
#include "Game/Subsystem/SFFogOfWarSubsystem.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "Logging/SFLog.h"
#include "SceneView.h"

namespace SFOverheadUI
{
	/**
	 * 한 프레임의 투영 데이터.
	 *
	 * 뷰 행렬은 프레임당 한 번만 구한다. ProjectWorldLocationToScreen 계열은 부를 때마다 다시 구한다.
	 */
	struct FProjector
	{
		APlayerController* PlayerController = nullptr;
		FSceneViewProjectionData ProjectionData;
		FMatrix ViewProjection = FMatrix::Identity;
		FIntRect ViewRect;

		bool Initialize(APlayerController* InPlayerController)
		{
			PlayerController = InPlayerController;
			const ULocalPlayer* LocalPlayer = IsValid(PlayerController) ? PlayerController->GetLocalPlayer() : nullptr;
			if (!LocalPlayer || !LocalPlayer->ViewportClient)	return false;

			if (!LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData))	return false;

			ViewProjection = ProjectionData.ComputeViewProjectionMatrix();
			ViewRect = ProjectionData.GetConstrainedViewRect();
			return true;
		}

		/** 월드 위치를 캔버스 좌표로 바꾼다. 카메라 뒤에 있으면 false다. */
		bool Project(const FVector& WorldLocation, FVector2D& OutCanvasPosition, double& OutDistance) const
		{
			FVector2D PixelPosition;
			if (!FSceneView::ProjectWorldToScreen(WorldLocation, ViewRect, ViewProjection, PixelPosition))	return false;

			// 레이아웃 반올림과 어긋나 판이 떨리지 않게 엔진 화면 레이어처럼 픽셀 단위로 맞춘다.
			PixelPosition = FVector2D(FMath::RoundToDouble(PixelPosition.X), FMath::RoundToDouble(PixelPosition.Y));
			USlateBlueprintLibrary::ScreenToViewport(PlayerController, PixelPosition, OutCanvasPosition);
			OutDistance = FVector::Dist(ProjectionData.ViewOrigin, WorldLocation);
			return true;
		}
	};
}

USFOverheadUISubsystem* USFOverheadUISubsystem::Get(const UObject* WorldContext)
{
	if (!WorldContext) return nullptr;

	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return IsValid(World) ? World->GetSubsystem<USFOverheadUISubsystem>() : nullptr;
}

void USFOverheadUISubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// 액터 BeginPlay보다 먼저 불린다. 판을 띄우기 전에 클래스를 준비해 둔다.
	LoadedPanelClass = OverheadPanelClass.LoadSynchronous();
	LoadedBadgeClass = SynergyBadgeClass.LoadSynchronous();

	if (!LoadedPanelClass)
	{
		SF_LOG(Error, "[OverheadUI] OverheadPanelClass를 불러오지 못했습니다. DefaultGame.ini를 확인하세요.");
	}

	if (!LoadedBadgeClass)
	{
		SF_LOG(Error, "[OverheadUI] SynergyBadgeClass를 불러오지 못했습니다. DefaultGame.ini를 확인하세요.");
	}

	// 안개는 가시성이 바뀔 때만 알린다. 매 프레임 묻지 않는다.
	if (USFFogOfWarSubsystem* Fog = InWorld.GetSubsystem<USFFogOfWarSubsystem>())
	{
		BoundFog = Fog;
		FogVisibilityHandle = Fog->OnActorFogVisibilityChanged.AddUObject(
			this, &ThisClass::HandleFogVisibilityChanged);
	}
}

void USFOverheadUISubsystem::Deinitialize()
{
	if (USFFogOfWarSubsystem* Fog = BoundFog.Get())
	{
		Fog->OnActorFogVisibilityChanged.Remove(FogVisibilityHandle);
	}
	BoundFog.Reset();
	FogVisibilityHandle.Reset();

	if (UCanvasPanel* Canvas = OverlayCanvas.Get())
	{
		ClearOverlayCanvas(Canvas);
	}

	ReleaseAllEntries();
	ReleaseSynergyBadges();
	PanelPool.ResetPool();
	RegisteredEntries.Reset();
	OverlayCanvas.Reset();

	Super::Deinitialize();
}

void USFOverheadUISubsystem::SetUIScale(float InScale)
{
	const float NewScale = FMath::Max(InScale, KINDA_SMALL_NUMBER);

	if (FMath::IsNearlyEqual(CurrentUIScale, NewScale))	return;

	CurrentUIScale = NewScale;

	// 배율은 렌더 변환만 바꾼다. 슬롯 위치는 그대로라 다시 투영할 필요가 없다.
	for (const TPair<TWeakObjectPtr<USFOverheadStatusComponent>, FPanelEntry>& Pair : ShownEntries)
	{
		const USFOverheadStatusComponent* Status = Pair.Key.Get();
		USFOverheadPanelWidget* Panel = Pair.Value.Panel.Get();

		if (IsValid(Status) && IsValid(Panel))
		{
			ApplyPanelScale(*Panel, Status->GetLayout());
		}
	}
}

#pragma region Canvas
void USFOverheadUISubsystem::SetOverlayCanvas(UCanvasPanel* InCanvas)
{
	if (!IsValid(InCanvas) || OverlayCanvas.Get() == InCanvas)	return;

	if (UCanvasPanel* PreviousCanvas = OverlayCanvas.Get())
	{
		ClearOverlayCanvas(PreviousCanvas);
	}

	OverlayCanvas = InCanvas;
	PanelPool.SetWorld(GetWorld());
	PanelPool.SetDefaultPlayerController(InCanvas->GetOwningPlayer());

	// 다음 갱신에서 카메라가 그대로여도 전부 한 번 투영하게 한다.
	LastViewRect = FIntRect();

	// 캔버스가 없던 동안 등록된 판 중 띄워야 하는 것을 이제 띄운다.
	for (const TWeakObjectPtr<USFOverheadStatusComponent>& WeakStatus : RegisteredEntries)
	{
		RefreshEntry(WeakStatus.Get());
	}

	if (USFBuildingSynergySubsystem* Synergy = USFBuildingSynergySubsystem::Get(this))
	{
		Synergy->OnSynergyStateChanged.AddUniqueDynamic(this, &ThisClass::HandleSynergyStateChanged);
		RefreshSynergyBadges();
	}
}

void USFOverheadUISubsystem::ClearOverlayCanvas(UCanvasPanel* InCanvas)
{
	if (!IsValid(InCanvas) || OverlayCanvas.Get() != InCanvas)	return;

	// 캔버스가 사라진 뒤 시너지 재계산이 이 캔버스에 배지를 만들지 않게 끊는다.
	if (USFBuildingSynergySubsystem* Synergy = USFBuildingSynergySubsystem::Get(this))
	{
		Synergy->OnSynergyStateChanged.RemoveDynamic(this, &ThisClass::HandleSynergyStateChanged);
	}

	ReleaseAllEntries();
	ReleaseSynergyBadges();

	// 풀의 위젯은 이 캔버스의 플레이어로 만들어졌다. 다음 HUD가 새로 만들게 비운다.
	PanelPool.ResetPool();
	OverlayCanvas.Reset();
}

void USFOverheadUISubsystem::UpdateScreenPositions()
{
	UCanvasPanel* Canvas = OverlayCanvas.Get();

	// 띄운 것이 없으면 투영 데이터도 구하지 않는다.
	if (!IsValid(Canvas) || (ShownEntries.IsEmpty() && SynergyBadges.IsEmpty()))	return;

	SFOverheadUI::FProjector Projector;
	if (!Projector.Initialize(Canvas->GetOwningPlayer()))	return;

	// 카메라가 그대로면 건물은 화면에서도 그대로다. 바뀐 판만 다시 투영한다.
	const bool bViewChanged =
		!Projector.ViewProjection.Equals(LastViewProjection, 0.f) ||
		Projector.ViewRect != LastViewRect;

	LastViewProjection = Projector.ViewProjection;
	LastViewRect = Projector.ViewRect;

	for (auto It = ShownEntries.CreateIterator(); It; ++It)
	{
		USFOverheadStatusComponent* Status = It.Key().Get();
		FPanelEntry& Entry = It.Value();
		USFOverheadPanelWidget* Panel = Entry.Panel.Get();
		UCanvasPanelSlot* Slot = Entry.Slot.Get();

		// EndPlay 없이 사라진 경우에 대비한다. 정상 경로는 UnregisterEntry가 먼저 거둔다.
		if (!IsValid(Status) || !IsValid(Panel) || !IsValid(Slot))
		{
			ReleaseEntry(Status, Entry);
			It.RemoveCurrent();
			continue;
		}

		// 안개에 가린 판은 접어 둔다. 다시 보일 때 bPositionDirty가 켜져 이 자리에서 투영된다.
		if (Entry.bFogHidden)	continue;

		const FVector Anchor = Status->GetAnchorWorldLocation();
		if (!bViewChanged && !Entry.bPositionDirty && Anchor.Equals(Entry.LastAnchor))	continue;

		Entry.LastAnchor = Anchor;
		Entry.bPositionDirty = false;

		FVector2D CanvasPosition;
		double Distance = 0.0;
		const bool bProjected = Projector.Project(Anchor, CanvasPosition, Distance);

		if (bProjected)
		{
			Slot->SetPosition(CanvasPosition);

			// 가까운 판이 위에 오게 엔진 화면 레이어처럼 거리순으로 쌓는다. 배지는 0이라 항상 위다.
			Slot->SetZOrder(-FMath::RoundToInt32(Distance));
		}

		const ESlateVisibility NewVisibility = bProjected
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed;

		if (Panel->GetVisibility() != NewVisibility)
		{
			Panel->SetVisibility(NewVisibility);
		}
	}

	UpdateSynergyBadgePositions(Projector, *Canvas);
}
#pragma endregion

#pragma region Entry
void USFOverheadUISubsystem::RegisterEntry(USFOverheadStatusComponent* InStatus)
{
	if (!IsValid(InStatus))	return;

	RegisteredEntries.Add(InStatus);
	RefreshEntry(InStatus);
}

void USFOverheadUISubsystem::UnregisterEntry(USFOverheadStatusComponent* InStatus)
{
	RegisteredEntries.Remove(InStatus);
	HideEntry(InStatus);
}

void USFOverheadUISubsystem::RefreshEntry(USFOverheadStatusComponent* InStatus)
{
	// 등록 전(BeginPlay 이전)의 변경은 무시한다. 등록할 때 현재 상태로 한 번 맞춘다.
	if (!IsValid(InStatus) || !RegisteredEntries.Contains(InStatus))	return;

	if (!InStatus->ShouldShowPanel())
	{
		HideEntry(InStatus);
		return;
	}

	if (FPanelEntry* Entry = ShownEntries.Find(InStatus))
	{
		ApplyEntry(*InStatus, *Entry);
		return;
	}

	ShowEntry(InStatus);
}

void USFOverheadUISubsystem::PlayResourceGain(
	USFOverheadStatusComponent* InStatus,
	EResourceType InResourceType,
	int32 InAmount)
{
	// 띄울 캔버스가 없으면 켜지도 않는다. 켜 두면 끄는 애니메이션이 없어 판이 남는다.
	if (!IsValid(InStatus) || !OverlayCanvas.IsValid())	return;

	/*
	 * 켜는 게 먼저다.
	 *
	 * 배지 위젯이 없는 판이면 PlayResourceGain()이 그 자리에서 "끝났다"를 쏜다.
	 * 그때 비트가 이미 올라가 있어야 제대로 내려간다.
	 */
	InStatus->SetElementVisible(ESFOverheadElement::ResourceGain, true);

	const FPanelEntry* Entry = ShownEntries.Find(InStatus);
	USFOverheadPanelWidget* Panel = Entry ? Entry->Panel.Get() : nullptr;

	if (!IsValid(Panel))
	{
		// 억제 중이라 판이 뜨지 않았다. 끝 신호가 오지 않으므로 바로 내린다.
		InStatus->SetElementVisible(ESFOverheadElement::ResourceGain, false);
		return;
	}

	Panel->PlayResourceGain(InResourceType, InAmount);
}

void USFOverheadUISubsystem::ShowEntry(USFOverheadStatusComponent* InStatus)
{
	UCanvasPanel* Canvas = OverlayCanvas.Get();
	if (!IsValid(Canvas) || !LoadedPanelClass)	return;

	USFOverheadPanelWidget* Panel = PanelPool.GetOrCreateInstance<USFOverheadPanelWidget>(LoadedPanelClass);
	if (!IsValid(Panel))	return;

	// 새 판은 WBP 기본값대로 요소가 모두 켜져 있고, 재사용한 판에는 이전 건물 상태가 남아 있다.
	// 어느 쪽이든 코드가 아는 상태로 맞춘다. 끝 신호 구독보다 먼저 해야 배지를 멈출 때 나는 신호가 이 건물로 오지 않는다.
	Panel->ResetForReuse();

	UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Panel);
	if (!IsValid(Slot))
	{
		PanelPool.Release(Panel);
		return;
	}

	Slot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
	Slot->SetAutoSize(false);

	// 위치는 다음 갱신에서 잡는다. 그 전에는 캔버스 원점에 보이지 않게 접어 둔다.
	Panel->SetVisibility(ESlateVisibility::Collapsed);

	FPanelEntry& Entry = ShownEntries.Add(InStatus);
	Entry.Panel = Panel;
	Entry.Slot = Slot;

	// 구독을 시작하는 시점의 상태는 신호와 같은 집합에서 읽는다.
	const USFFogOfWarSubsystem* Fog = BoundFog.Get();
	Entry.bFogHidden = IsValid(Fog) && Fog->IsActorHiddenByFog(InStatus->GetOwner());

	// 값은 판이 떠 있는 동안만 받는다. 꺼진 건물의 체력 변화는 위젯을 깨우지 않는다.
	const TWeakObjectPtr<USFOverheadStatusComponent> WeakStatus(InStatus);
	Entry.StatusHandle = InStatus->OnStatusChanged.AddUObject(
		this, &ThisClass::HandleStatusChanged, WeakStatus);
	Entry.ElementFinishedHandle = Panel->OnElementFinished.AddUObject(
		this, &ThisClass::HandleElementFinished, WeakStatus);

	Panel->ApplyStatus(InStatus->GetStatus());
	ApplyEntry(*InStatus, Entry);
}

void USFOverheadUISubsystem::ApplyEntry(const USFOverheadStatusComponent& InStatus, FPanelEntry& Entry) const
{
	USFOverheadPanelWidget* Panel = Entry.Panel.Get();
	UCanvasPanelSlot* Slot = Entry.Slot.Get();
	if (!IsValid(Panel) || !IsValid(Slot))	return;

	// 바뀐 요소만 판에 전달한다.
	const uint8 Mask = InStatus.GetElementMask();
	const uint8 ChangedBits = Entry.AppliedMask ^ Mask;

	for (int32 Index = 0; Index < static_cast<int32>(ESFOverheadElement::MAX); ++Index)
	{
		const uint8 Bit = static_cast<uint8>(1 << Index);
		if ((ChangedBits & Bit) != 0)
		{
			Panel->SetElementVisible(static_cast<ESFOverheadElement>(Index), (Mask & Bit) != 0);
		}
	}
	Entry.AppliedMask = Mask;

	// 엔진 화면 레이어와 같은 배치다. 슬롯 크기는 판 크기, 정렬은 Pivot이다.
	const FSFOverheadLayout& Layout = InStatus.GetLayout();
	Slot->SetSize(Layout.DrawSize);
	Slot->SetAlignment(Layout.Pivot);
	Panel->SetElementSpacing(Layout.ElementSpacing);
	ApplyPanelScale(*Panel, Layout);

	Entry.bPositionDirty = true;
}

void USFOverheadUISubsystem::ApplyPanelScale(USFOverheadPanelWidget& Panel, const FSFOverheadLayout& Layout) const
{
	// 배치 Pivot과 같은 축으로 확대·축소해야 앵커에 붙은 상태가 유지된다.
	Panel.SetRenderTransformPivot(Layout.Pivot);
	Panel.SetRenderScale(FVector2D(CurrentUIScale, CurrentUIScale));

	// Translation은 Scale과 별도로 합성되므로(Concatenate 순서상 마지막) 직접 곱한다.
	Panel.SetRenderTranslation(FVector2D(0.f, Layout.ScreenOffsetY) * CurrentUIScale);
}

void USFOverheadUISubsystem::HideEntry(USFOverheadStatusComponent* InStatus)
{
	FPanelEntry Entry;
	if (!ShownEntries.RemoveAndCopyValue(InStatus, Entry))	return;

	ReleaseEntry(InStatus, Entry);
}

void USFOverheadUISubsystem::ReleaseEntry(USFOverheadStatusComponent* InStatus, const FPanelEntry& Entry)
{
	if (IsValid(InStatus))
	{
		InStatus->OnStatusChanged.Remove(Entry.StatusHandle);
	}

	USFOverheadPanelWidget* Panel = Entry.Panel.Get();
	if (!IsValid(Panel))	return;

	// 다음에 꺼낼 때 ShowEntry가 초기화한다. 여기서는 이 건물과의 연결만 끊는다.
	Panel->OnElementFinished.Remove(Entry.ElementFinishedHandle);
	Panel->RemoveFromParent();
	PanelPool.Release(Panel);
}

void USFOverheadUISubsystem::ReleaseAllEntries()
{
	for (const TPair<TWeakObjectPtr<USFOverheadStatusComponent>, FPanelEntry>& Pair : ShownEntries)
	{
		ReleaseEntry(Pair.Key.Get(), Pair.Value);
	}
	ShownEntries.Reset();
}

void USFOverheadUISubsystem::HandleStatusChanged(
	const FSFOverheadStatus& InStatus,
	TWeakObjectPtr<USFOverheadStatusComponent> InSource)
{
	const FPanelEntry* Entry = ShownEntries.Find(InSource);
	USFOverheadPanelWidget* Panel = Entry ? Entry->Panel.Get() : nullptr;

	if (IsValid(Panel))
	{
		Panel->ApplyStatus(InStatus);
	}
}

void USFOverheadUISubsystem::HandleElementFinished(
	ESFOverheadElement InElement,
	TWeakObjectPtr<USFOverheadStatusComponent> InSource)
{
	// 배지처럼 스스로 끝나는 요소는 판이 알려준다. 액터 쪽 비트를 내리면 다시 이리로 돌아온다.
	if (USFOverheadStatusComponent* Status = InSource.Get())
	{
		Status->SetElementVisible(InElement, false);
	}
}

void USFOverheadUISubsystem::HandleFogVisibilityChanged(const AActor* InActor, bool bVisible)
{
	if (!IsValid(InActor) || ShownEntries.IsEmpty())	return;

	// 유닛 신호도 함께 온다. 판이 떠 있는 건물만 찾는다.
	USFOverheadStatusComponent* Status = InActor->FindComponentByClass<USFOverheadStatusComponent>();
	FPanelEntry* Entry = Status ? ShownEntries.Find(Status) : nullptr;
	if (!Entry)	return;

	Entry->bFogHidden = !bVisible;
	Entry->bPositionDirty = true;

	// 가려지는 즉시 접는다. 다시 보이면 다음 갱신에서 투영하며 펼친다.
	USFOverheadPanelWidget* Panel = Entry->Panel.Get();
	if (Entry->bFogHidden && IsValid(Panel))
	{
		Panel->SetVisibility(ESlateVisibility::Collapsed);
	}
}
#pragma endregion

#pragma region SynergyBadge
void USFOverheadUISubsystem::HandleSynergyStateChanged()
{
	RefreshSynergyBadges();
}

void USFOverheadUISubsystem::RefreshSynergyBadges()
{
	UCanvasPanel* Canvas = OverlayCanvas.Get();
	if (!IsValid(Canvas) || !LoadedBadgeClass)	return;

	const ASFInGamePlayerController* PlayerController =
		Cast<ASFInGamePlayerController>(Canvas->GetOwningPlayer());

	USFBuildingSynergySubsystem* SynergySubsystem =
		USFBuildingSynergySubsystem::Get(this);

	if (!IsValid(PlayerController) || !IsValid(SynergySubsystem))	return;

	const TArray<FSFBuildingSynergyClusterProgressInfo> ProgressInfos =
		SynergySubsystem->GetClusterProgressInfosForTeam(
			PlayerController->GetMyTeamIndex());

	while (SynergyBadges.Num() < ProgressInfos.Num())
	{
		USFBuildingSynergyBadgeWidget* NewBadge =
			CreateWidget<USFBuildingSynergyBadgeWidget>(
				Canvas->GetOwningPlayer(),
				LoadedBadgeClass);

		if (!IsValid(NewBadge))	break;

		UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(NewBadge);
		CanvasSlot->SetAutoSize(true);
		CanvasSlot->SetAlignment(FVector2D(0.5f, 1.f));

		// 머리 위 판은 거리만큼 음수 ZOrder를 쓴다. 배지는 그 위에 그린다.
		CanvasSlot->SetZOrder(0);

		SynergyBadges.Add(NewBadge);
	}

	for (int32 Index = 0; Index < SynergyBadges.Num(); ++Index)
	{
		USFBuildingSynergyBadgeWidget* Badge = SynergyBadges[Index];
		if (!IsValid(Badge))	continue;

		if (ProgressInfos.IsValidIndex(Index))
		{
			Badge->SetProgressInfo(ProgressInfos[Index]);
		}
		else
		{
			Badge->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void USFOverheadUISubsystem::UpdateSynergyBadgePositions(
	const SFOverheadUI::FProjector& Projector,
	const UCanvasPanel& Canvas) const
{
	// 투영된 좌표와 같은 UI 좌표계를 사용하기 위해 실제 캔버스 크기를 사용합니다.
	const FVector2D CanvasSize = Canvas.GetCachedGeometry().GetLocalSize();
	if (CanvasSize.X <= 0.f || CanvasSize.Y <= 0.f)	return;

	const float SafeScreenMargin = FMath::Max(0.f, SynergyBadgeScreenMargin);

	for (USFBuildingSynergyBadgeWidget* Badge : SynergyBadges)
	{
		// Collapsed는 현재 클러스터와 연결되지 않은 풀링용 배지입니다.
		if (!IsValid(Badge) || Badge->GetVisibility() == ESlateVisibility::Collapsed)	continue;

		UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Badge->Slot);

		FVector2D ScreenPosition;
		double Distance = 0.0;

		if (!IsValid(CanvasSlot) || !Projector.Project(Badge->GetBadgeWorldLocation(), ScreenPosition, Distance))
		{
			// Hidden은 다음 갱신에도 다시 화면 투영을 검사할 수 있습니다.
			Badge->SetVisibility(ESlateVisibility::Hidden);
			continue;
		}

		// 배지 문구가 바뀌면 크기도 바뀌므로 배지는 카메라가 멈춰 있어도 매번 검사합니다.
		const FVector2D BadgeSize = Badge->GetDesiredSize();
		const FVector2D BadgeTopLeft = ScreenPosition - BadgeSize * CanvasSlot->GetAlignment();
		const FVector2D BadgeBottomRight = BadgeTopLeft + BadgeSize;

		const bool bIsFullyInsideScreen =
			BadgeTopLeft.X >= SafeScreenMargin &&
			BadgeTopLeft.Y >= SafeScreenMargin &&
			BadgeBottomRight.X <= CanvasSize.X - SafeScreenMargin &&
			BadgeBottomRight.Y <= CanvasSize.Y - SafeScreenMargin;

		if (!bIsFullyInsideScreen)
		{
			Badge->SetVisibility(ESlateVisibility::Hidden);
			continue;
		}

		CanvasSlot->SetPosition(ScreenPosition);
		Badge->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void USFOverheadUISubsystem::ReleaseSynergyBadges()
{
	for (USFBuildingSynergyBadgeWidget* Badge : SynergyBadges)
	{
		if (IsValid(Badge))
		{
			Badge->RemoveFromParent();
		}
	}
	SynergyBadges.Reset();
}
#pragma endregion
