#include "SFOverheadUIComponent.h"

#include "Buildings/UI/SFBuildingStatWidget.h"
#include "Components/MeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Game/Subsystem/SFOverheadUISubsystem.h"

namespace SFOverheadWidget
{
	/** 모든 대상이 공유하는 바 세로 두께. 픽셀 단위. */
	constexpr float BarHeight = 40.f;
}

USFOverheadUIComponent::USFOverheadUIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USFOverheadUIComponent::Initialize(UWidgetComponent* InWidgetComponent, UMeshComponent* InMeshComponent)
{
	WidgetComponent = InWidgetComponent;
	MeshComponent = InMeshComponent;
	CachedWidget.Reset();

	// SetHovered/SetProgressActive는 값이 같으면 조기 반환한다.
	// 시작 상태는 여기서 한 번 직접 맞춘다.
	if (USFBuildingStatWidget* Widget = GetWidget())
	{
		// 상시 표시 대상은 체력바 줄을 처음부터 켜 둔다.
		Widget->SetHeathBarVisible(bAlwaysVisible || bHovered);
		Widget->SetProgressVisible(bProgressActive);
	}

	RefreshVisibility();
}

USFBuildingStatWidget* USFOverheadUIComponent::GetWidget() const
{
	if (CachedWidget.IsValid())	return CachedWidget.Get();
	if (!IsValid(WidgetComponent))	return nullptr;

	USFBuildingStatWidget* Found =
		Cast<USFBuildingStatWidget>(WidgetComponent->GetUserWidgetObject());

	CachedWidget = Found;
	return Found;
}

void USFOverheadUIComponent::BeginPlay()
{
	Super::BeginPlay();
	if (USFOverheadUISubsystem* OverheadUISubsystem = USFOverheadUISubsystem::Get(this))
	{
		UIScaleHandle = OverheadUISubsystem->OnUIScaleChanged.AddUObject(
			this, &USFOverheadUIComponent::HandleUIScaleChanged);
	}
}

void USFOverheadUIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USFOverheadUISubsystem* OverheadUISubsystem = USFOverheadUISubsystem::Get(this))
	{
		OverheadUISubsystem->OnUIScaleChanged.Remove(UIScaleHandle);
	}
	UIScaleHandle.Reset();
	
	Super::EndPlay(EndPlayReason);
}

void USFOverheadUIComponent::ApplySettings(const FSFOverheadUISettings& InSettings)
{
	Settings = InSettings;
	RefreshTransform();
}

void USFOverheadUIComponent::RefreshTransform()
{
	if (!IsValid(WidgetComponent))	return;

	// ── 위치 ──
	WidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, Settings.AnchorZ));

	// ── 크기 ──
	float FinalWidth = Settings.WidthRange.X;

	if (IsValid(MeshComponent))
	{
		// 회전은 빼고 스케일만 적용한다.
		// 월드 바운즈를 쓰면 45도로 배치된 대상의 폭이 부풀어 오른다.
		const FBoxSphereBounds ScaledBounds = MeshComponent->CalcBounds(
			FTransform(
				FQuat::Identity,
				FVector::ZeroVector,
				MeshComponent->GetComponentScale()));

		const float WorldWidth = ScaledBounds.BoxExtent.X;

		FinalWidth = FMath::Clamp(
			WorldWidth * Settings.WidthRatio,
			Settings.WidthRange.X,
			Settings.WidthRange.Y);
	}

	WidgetComponent->SetDrawSize(FVector2D(FinalWidth, SFOverheadWidget::BarHeight));
}

void USFOverheadUIComponent::SetHovered(bool bInHovered)
{
	if (bHovered == bInHovered)	return;

	bHovered = bInHovered;

	if (USFBuildingStatWidget* Widget = GetWidget())
	{
		Widget->SetHeathBarVisible(bInHovered);
	}

	RefreshVisibility();
}

void USFOverheadUIComponent::SetProgressActive(bool bInActive)
{
	bProgressActive = bInActive;

	if (USFBuildingStatWidget* Widget = GetWidget())
	{
		Widget->SetProgressVisible(bInActive);
	}

	RefreshVisibility();
}

void USFOverheadUIComponent::SetSuppressed(bool bInSuppressed)
{
	if (bSuppressed == bInSuppressed)	return;

	bSuppressed = bInSuppressed;
	RefreshVisibility();
}

void USFOverheadUIComponent::RefreshVisibility()
{
	if (!IsValid(WidgetComponent))	return;

	const bool bShouldShow = !bSuppressed && (bAlwaysVisible || bHovered || bProgressActive);
	
	// UserWidget 가시성은 조기 반환보다 먼저 맞춘다.
	// Screen 모드의 IsWidgetVisible()이 이 값을 보고 컴포넌트 틱을 끈다.
	if (USFBuildingStatWidget* Widget = GetWidget())
	{
		Widget->SetVisibility(bShouldShow
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	if (WidgetComponent->IsVisible() == bShouldShow)	return;

	WidgetComponent->SetVisibility(bShouldShow);

	if (bShouldShow)
	{
		ApplyUIScale();
	}
}

void USFOverheadUIComponent::ApplyUIScale()
{
	USFBuildingStatWidget* Widget = GetWidget();
	if (!IsValid(Widget))	return;

	const USFOverheadUISubsystem* OverheadUISubsystem = USFOverheadUISubsystem::Get(this);
	if (!IsValid(OverheadUISubsystem))	return;

	const float Scale = OverheadUISubsystem->GetUIScale();

	Widget->SetRenderScale(FVector2D(Scale, Scale));
}

void USFOverheadUIComponent::HandleUIScaleChanged(float InScale)
{
	// 꺼져 있는 위젯은 지금 건드리지 않는다. 켜질 때 ApplyUIScale()이 반영한다.
	if (!IsValid(WidgetComponent) || !WidgetComponent->IsVisible())	return;

	ApplyUIScale();
}

void USFOverheadUIComponent::SetHealthBar(float InBeforeHP, float InCurrentHP, float InMaxHP)
{
	if (USFBuildingStatWidget* Widget = GetWidget())
	{
		Widget->SetHealthBar(InBeforeHP, InCurrentHP, InMaxHP);
	}
}

void USFOverheadUIComponent::SetHealthBarColor(const FLinearColor& InColor)
{
	if (USFBuildingStatWidget* Widget = GetWidget())
	{
		Widget->SetHealthBarColor(InColor);
	}
}

void USFOverheadUIComponent::SetProgressRatio(float InRatio)
{
	if (USFBuildingStatWidget* Widget = GetWidget())
	{
		Widget->SetProgressRatio(InRatio);
	}
}

void USFOverheadUIComponent::SetProgressBarColor(const FLinearColor& InColor)
{
	if (USFBuildingStatWidget* Widget = GetWidget())
	{
		Widget->SetProgressBarColor(InColor);
	}
}