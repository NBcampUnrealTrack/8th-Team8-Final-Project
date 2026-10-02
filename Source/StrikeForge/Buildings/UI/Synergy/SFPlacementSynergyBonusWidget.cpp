#include "SFPlacementSynergyBonusWidget.h"

#include "ActorComponent/Controller/BuildingPlacement.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "InGame/Controller/SFInGamePlayerController.h"

#define LOCTEXT_NAMESPACE "SFPlacementSynergyBonusWidget"

// 소유 컨트롤러의 배치 컴포넌트 보너스 알림을 구독하고, 현재 배치 상태로 처음 표시를 맞춥니다.
// USFInGameHUDWidget::CreatePlacementSynergyBonusWidget이 이 위젯을 RootCanvas에 넣을 때 실행됩니다.
void USFPlacementSynergyBonusWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetVisibility(ESlateVisibility::Collapsed);

	const ASFInGamePlayerController* PlayerController =
		GetOwningPlayer<ASFInGamePlayerController>();
	UBuildingPlacement* Placement =
		IsValid(PlayerController) ? PlayerController->GetBuildingPlacementComponent() : nullptr;

	if (!IsValid(Placement))
	{
		return;
	}

	BoundPlacement = Placement;
	Placement->OnPlacementSynergyBonusChanged.AddUniqueDynamic(
		this, &ThisClass::HandlePlacementSynergyBonusChanged);

	HandlePlacementSynergyBonusChanged(
		Placement->IsPlacementModeActive(),
		Placement->GetPlacementSynergyBonusPercent());
}

// NativeConstruct에서 연결한 배치 컴포넌트 알림을 해제합니다.
void USFPlacementSynergyBonusWidget::NativeDestruct()
{
	if (UBuildingPlacement* Placement = BoundPlacement.Get())
	{
		Placement->OnPlacementSynergyBonusChanged.RemoveDynamic(
			this, &ThisClass::HandlePlacementSynergyBonusChanged);
	}
	BoundPlacement.Reset();

	Super::NativeDestruct();
}

// 보이는 동안 매 프레임 커서를 따라갑니다. 숨겨진 동안에는 엔진이 Tick을 호출하지 않습니다.
void USFPlacementSynergyBonusWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	FollowCursor();
}

// 배치 모드 동안에는 보너스가 0이어도 "+0%"로 표시하고, 배치 모드가 끝나면 숨깁니다.
// UBuildingPlacement::OnPlacementSynergyBonusChanged에 연결됩니다.
void USFPlacementSynergyBonusWidget::HandlePlacementSynergyBonusChanged(
	bool bPlacementActive,
	float BonusPercent)
{
	if (IsValid(BonusText))
	{
		FNumberFormattingOptions NumberFormat;
		NumberFormat.MinimumFractionalDigits = 0;
		NumberFormat.MaximumFractionalDigits = 1;

		BonusText->SetText(FText::Format(
			LOCTEXT("BonusFormat", "+{0}%"),
			FText::AsNumber(FMath::Max(0.f, BonusPercent), &NumberFormat)));
	}

	if (!bPlacementActive)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// 숨겨져 있던 동안 Tick이 돌지 않았으므로 보이기 전에 커서 위치로 먼저 옮깁니다.
	FollowCursor();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

// 뷰포트 기준 커서 위치(DPI 보정된 Slate 좌표)에 CursorOffset을 더해 Canvas 슬롯 위치로 지정합니다.
// HUD의 RootCanvas가 화면 전체를 덮으므로 뷰포트 좌표를 그대로 Canvas 좌표로 씁니다.
void USFPlacementSynergyBonusWidget::FollowCursor()
{
	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot);
	if (!IsValid(CanvasSlot))
	{
		return;
	}

	const FVector2D MousePosition =
		UWidgetLayoutLibrary::GetMousePositionOnViewport(this);

	CanvasSlot->SetPosition(MousePosition + CursorOffset);
}

#undef LOCTEXT_NAMESPACE
