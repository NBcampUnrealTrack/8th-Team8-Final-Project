#include "SFOverheadUIComponent.h"

#include "Blueprint/UserWidget.h"
#include "Components/MeshComponent.h"
#include "Buildings/UI/SFOverheadPanelWidget.h"
#include "Game/Subsystem/SFOverheadUISubsystem.h"

static_assert(static_cast<int32>(ESFOverheadElement::MAX) <= 8,
	"ElementMask가 uint8이다. 요소가 8개를 넘으면 타입을 넓혀야 한다.");

USFOverheadUIComponent::USFOverheadUIComponent()
{
	SetWidgetSpace(EWidgetSpace::Screen);
	SetDrawAtDesiredSize(false);
	SetWindowFocusable(false);

	/*
	 * 부모의 기본값은 Enabled라서, 판이 접혀 있어도 매 프레임 틱한다.
	 * Automatic으로 두면 UserWidget이 Collapsed가 되는 순간 부모가 스스로 틱을 끄고,
	 * 다시 보일 때 되살린다. RefreshVisibility()가 그 방아쇠다.
	 */
	SetTickMode(ETickMode::Automatic);

	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetGenerateOverlapEvents(false);
	SetCastShadow(false);

	SetVisibility(false);
}

void USFOverheadUIComponent::Initialize(UMeshComponent* InMeshComponent)
{
	MeshComponent = InMeshComponent;

	RefreshTransform();
	RefreshVisibility();
}

void USFOverheadUIComponent::BeginPlay()
{
	// 부모가 InitWidget()으로 UserWidget을 만든다. 그 뒤에야 위젯을 만질 수 있다.
	Super::BeginPlay();

	if (USFOverheadUISubsystem* OverheadUISubsystem = USFOverheadUISubsystem::Get(this))
	{
		UIScaleHandle = OverheadUISubsystem->OnUIScaleChanged.AddUObject(
			this, &USFOverheadUIComponent::HandleUIScaleChanged);
	}

	AActor* Owner = GetOwner();
	StatusComponent = IsValid(Owner)
		? Owner->FindComponentByClass<USFOverheadStatusComponent>()
		: nullptr;

	if (IsValid(StatusComponent))
	{
		StatusHandle = StatusComponent->OnStatusChanged.AddUObject(
			this, &USFOverheadUIComponent::HandleStatusChanged);

		// 구독 이후의 변화만 오므로 현재 상태를 한 번 반영한다.
		HandleStatusChanged(StatusComponent->GetStatus());
	}

	// BP 기본값이 무엇이든 코드가 아는 상태로 맞춘다.
	PushPanelState();

	// 배지처럼 스스로 끝나는 요소는 판이 끝났다고 알려준다. 그때 비트를 내린다.
	if (USFOverheadPanelWidget* Panel = GetWidget<USFOverheadPanelWidget>())
	{
		ElementFinishedHandle = Panel->OnElementFinished.AddUObject(
			this, &USFOverheadUIComponent::HandleElementFinished);
	}

	RefreshTransform();
	RefreshVisibility();
}

void USFOverheadUIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USFOverheadUISubsystem* OverheadUISubsystem = USFOverheadUISubsystem::Get(this))
	{
		OverheadUISubsystem->OnUIScaleChanged.Remove(UIScaleHandle);
	}
	UIScaleHandle.Reset();

	if (IsValid(StatusComponent))
	{
		StatusComponent->OnStatusChanged.Remove(StatusHandle);
	}
	StatusHandle.Reset();
	StatusComponent = nullptr;

	if (USFOverheadPanelWidget* Panel = GetWidget<USFOverheadPanelWidget>())
	{
		Panel->OnElementFinished.Remove(ElementFinishedHandle);
	}
	ElementFinishedHandle.Reset();

	Super::EndPlay(EndPlayReason);
}

void USFOverheadUIComponent::ApplySettings(const FSFOverheadUISettings& InSettings)
{
	Settings = InSettings;
	RefreshTransform();

	// 판 안쪽 레이아웃(간격)도 같은 설정에서 나온다.
	PushPanelState();
}

void USFOverheadUIComponent::RefreshTransform()
{
	float AnchorZ = 0.f;
	float HalfWidth = 0.f;
	float HalfHeight = 0.f;

	if (IsValid(MeshComponent))
	{
		// 앵커는 메시 중심 하나로 고정한다.
		const FBoxSphereBounds LocalBounds =
			MeshComponent->CalcBounds(MeshComponent->GetRelativeTransform());

		AnchorZ = LocalBounds.Origin.Z;

		// 크기·오프셋 계산은 회전을 빼고 스케일만 적용한다.
		// 월드 바운즈를 쓰면 45도로 배치된 대상의 폭이 부풀어 오른다.
		const FBoxSphereBounds ScaledBounds = MeshComponent->CalcBounds(
			FTransform(FQuat::Identity, FVector::ZeroVector, MeshComponent->GetComponentScale()));

		HalfWidth  = ScaledBounds.BoxExtent.X;
		HalfHeight = ScaledBounds.BoxExtent.Z;
	}

	SetRelativeLocation(FVector(0.f, 0.f, AnchorZ));
	SetPivot(Settings.Pivot);

	const float FinalWidth = FMath::Clamp(
		HalfWidth * Settings.WidthRatio,
		Settings.WidthRange.X,
		Settings.WidthRange.Y);

	SetDrawSize(FVector2D(FinalWidth, Settings.PanelHeight));

	// 판의 위아래 위치는 월드가 아니라 화면 픽셀로 준다.
	ComputedScreenOffsetY = HalfHeight * Settings.VerticalRatio;

	ApplyUIScale();
}

void USFOverheadUIComponent::SetElementVisible(ESFOverheadElement InElement, bool bInVisible)
{
	const int32 Index = static_cast<int32>(InElement);
	if (Index < 0 || Index >= static_cast<int32>(ESFOverheadElement::MAX))	return;

	const uint8 Bit = static_cast<uint8>(1 << Index);
	const bool bWasVisible = (ElementMask & Bit) != 0;
	if (bWasVisible == bInVisible)	return;

	ElementMask = bInVisible
		? static_cast<uint8>(ElementMask | Bit)
		: static_cast<uint8>(ElementMask & ~Bit);

	if (USFOverheadPanelWidget* Panel = GetWidget<USFOverheadPanelWidget>())
	{
		Panel->SetElementVisible(InElement, bInVisible);
	}

	RefreshVisibility();
}

void USFOverheadUIComponent::SetSuppressed(bool bInSuppressed)
{
	if (bSuppressed == bInSuppressed)	return;

	bSuppressed = bInSuppressed;
	RefreshVisibility();
}

void USFOverheadUIComponent::PlayResourceGain(EResourceType InResourceType, int32 InAmount)
{
	USFOverheadPanelWidget* Panel = GetWidget<USFOverheadPanelWidget>();
	if (!IsValid(Panel))	return;

	/*
	 * 켜는 게 먼저다.
	 *
	 * 배지 위젯이 없는 판이면 PlayResourceGain()이 그 자리에서 "끝났다"를 쏜다.
	 * 그때 비트가 이미 올라가 있어야 제대로 내려간다. 순서를 바꾸면 판이 켜진 채 남는다.
	 */
	SetElementVisible(ESFOverheadElement::ResourceGain, true);

	Panel->PlayResourceGain(InResourceType, InAmount);
}

void USFOverheadUIComponent::HandleElementFinished(ESFOverheadElement InElement)
{
	SetElementVisible(InElement, false);
}

void USFOverheadUIComponent::PushPanelState()
{
	USFOverheadPanelWidget* Panel = GetWidget<USFOverheadPanelWidget>();
	if (!IsValid(Panel))	return;

	for (int32 Index = 0; Index < static_cast<int32>(ESFOverheadElement::MAX); ++Index)
	{
		const bool bElementOn = (ElementMask & (1 << Index)) != 0;
		Panel->SetElementVisible(static_cast<ESFOverheadElement>(Index), bElementOn);
	}

	// 요소 사이 간격도 DA가 정한다. 스페이서가 없는 판이면 무시된다.
	Panel->SetElementSpacing(Settings.ElementSpacing);
}

void USFOverheadUIComponent::RefreshVisibility()
{
	// 켜진 요소가 하나도 없으면 판을 띄울 이유가 없다.
	const bool bShouldShow = !bSuppressed && ElementMask != 0;

	// UserWidget 가시성을 먼저 맞춘다.
	// Screen 모드의 IsWidgetVisible()이 이 값을 보고 틱을 끄고 켠다.
	if (UUserWidget* BarWidget = GetUserWidgetObject())
	{
		BarWidget->SetVisibility(bShouldShow
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	if (IsVisible() == bShouldShow)	return;

	SetVisibility(bShouldShow);

	if (bShouldShow)
	{
		ApplyUIScale();
	}
}

void USFOverheadUIComponent::ApplyUIScale()
{
	UUserWidget* BarWidget = GetUserWidgetObject();
	if (!IsValid(BarWidget))	return;

	const USFOverheadUISubsystem* OverheadUISubsystem = USFOverheadUISubsystem::Get(this);
	if (!IsValid(OverheadUISubsystem))	return;

	const float Scale = OverheadUISubsystem->GetUIScale();

	const FVector2D Offset(
		Settings.ScreenOffsetAdjust.X,
		ComputedScreenOffsetY + Settings.ScreenOffsetAdjust.Y);

	// 배치 Pivot과 같은 축으로 확대·축소해야 앵커에 붙은 상태가 유지된다.
	BarWidget->SetRenderTransformPivot(Settings.Pivot);
	BarWidget->SetRenderScale(FVector2D(Scale, Scale));

	// Translation은 Scale과 별도로 합성되므로(Concatenate 순서상 마지막) 직접 곱한다.
	BarWidget->SetRenderTranslation(Offset * Scale);
}

void USFOverheadUIComponent::HandleUIScaleChanged(float InScale)
{
	// 꺼져 있는 판은 지금 건드리지 않는다. 켜질 때 ApplyUIScale()이 반영한다.
	if (!IsVisible())	return;

	ApplyUIScale();
}

void USFOverheadUIComponent::HandleStatusChanged(const FSFOverheadStatus& InStatus)
{
	if (USFOverheadBarWidget* Bar = GetWidget<USFOverheadBarWidget>())
	{
		Bar->ApplyStatus(InStatus);
	}
}
