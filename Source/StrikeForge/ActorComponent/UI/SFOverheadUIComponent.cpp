#include "SFOverheadUIComponent.h"

#include "Components/MeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Buildings/UI/SFOverheadBarWidget.h"
#include "Game/Subsystem/SFOverheadUISubsystem.h"

USFOverheadUIComponent::USFOverheadUIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USFOverheadUIComponent::Initialize(UMeshComponent* InMeshComponent)
{
	MeshComponent = InMeshComponent;

	// WidgetComponent는 BeginPlay의 CreateWidgetComponent()가 만든다.
	// 순서가 뒤바뀔 수 있으니 이미 만들어져 있을 때만 갱신한다.
	if (IsValid(WidgetComponent))
	{
		RefreshTransform();
		RefreshVisibility();
	}
}

UUserWidget* USFOverheadUIComponent::GetUserWidget() const
{
	if (CachedWidget.IsValid())	return CachedWidget.Get();
	if (!IsValid(WidgetComponent))	return nullptr;

	UUserWidget* Found = WidgetComponent->GetUserWidgetObject();
	CachedWidget = Found;
	return Found;
}

void USFOverheadUIComponent::BeginPlay()
{
	Super::BeginPlay();
	
	CreateWidgetComponent();

	if (USFOverheadUISubsystem* OverheadUISubsystem = USFOverheadUISubsystem::Get(this))
	{
		UIScaleHandle = OverheadUISubsystem->OnUIScaleChanged.AddUObject(
			this, &USFOverheadUIComponent::HandleUIScaleChanged);
	}

	AActor* Owner = GetOwner();
	USFOverheadStatusComponent* StatusComponent = IsValid(Owner)
		? Owner->FindComponentByClass<USFOverheadStatusComponent>()
		: nullptr;

	if (IsValid(StatusComponent))
	{
		StatusHandle = StatusComponent->OnStatusChanged.AddUObject(
			this, &USFOverheadUIComponent::HandleStatusChanged);

		// 구독 이후의 변화만 오므로 현재 상태를 한 번 반영한다.
		HandleStatusChanged(StatusComponent->GetStatus());
	}
}

void USFOverheadUIComponent::CreateWidgetComponent()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner))	return;

	if (!ensureMsgf(WidgetClass != nullptr,
		TEXT("[%s] %s의 Widget Class가 비어 있습니다."), *Owner->GetName(), *GetName()))
	{
		return;
	}

	WidgetComponent = NewObject<UWidgetComponent>(Owner);
	if (!IsValid(WidgetComponent))	return;

	WidgetComponent->SetWidgetClass(WidgetClass);
	WidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	WidgetComponent->SetDrawAtDesiredSize(false);
	WidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WidgetComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	WidgetComponent->SetGenerateOverlapEvents(false);
	WidgetComponent->SetWindowFocusable(false);
	WidgetComponent->SetVisibility(false);

	// SetupAttachment는 등록 전에만 유효하다. 순서를 지켜야 한다.
	WidgetComponent->SetupAttachment(Owner->GetRootComponent());
	WidgetComponent->RegisterComponent();

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

	if (AActor* Owner = GetOwner())
	{
		if (USFOverheadStatusComponent* StatusComponent =
			Owner->FindComponentByClass<USFOverheadStatusComponent>())
		{
			StatusComponent->OnStatusChanged.Remove(StatusHandle);
		}
	}
	StatusHandle.Reset();
	
	if (IsValid(WidgetComponent))
	{
		WidgetComponent->DestroyComponent();
		WidgetComponent = nullptr;
	}
	CachedWidget.Reset();
	
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

	float AnchorZ = 0.f;
	float HalfWidth = 0.f;
	float HalfHeight = 0.f;

	if (IsValid(MeshComponent))
	{
		// 앵커는 메시 중심 하나로 고정한다.
		// 모든 바가 같은 월드 점에서 투영되어야 시점이 바뀌어도 서로 정렬된다.
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

	WidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, AnchorZ));
	WidgetComponent->SetPivot(Settings.Pivot);

	const float FinalWidth = FMath::Clamp(
		HalfWidth * Settings.WidthRatio,
		Settings.WidthRange.X,
		Settings.WidthRange.Y);

	WidgetComponent->SetDrawSize(FVector2D(FinalWidth, Settings.BarHeight));

	// 위아래 배치는 월드가 아니라 화면 픽셀로 준다. 그래야 앵커를 공유한 채 간격만 벌릴 수 있다.
	ComputedScreenOffsetY = HalfHeight * Settings.VerticalRatio;

	ApplyUIScale();
}

void USFOverheadUIComponent::SetVisible(bool bInVisible)
{
	if (bVisible == bInVisible)	return;

	bVisible = bInVisible;
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

	const bool bShouldShow = !bSuppressed && (bAlwaysVisible || bVisible);

	// UserWidget 가시성은 조기 반환보다 먼저 맞춘다.
	// Screen 모드의 IsWidgetVisible()이 이 값을 보고 컴포넌트 틱을 끈다.
	if (UUserWidget* Widget = GetUserWidget())
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
	UUserWidget* Widget = GetUserWidget();
	if (!IsValid(Widget))	return;

	const USFOverheadUISubsystem* OverheadUISubsystem = USFOverheadUISubsystem::Get(this);
	if (!IsValid(OverheadUISubsystem))	return;

	const float Scale = OverheadUISubsystem->GetUIScale();

	const FVector2D Offset(
		Settings.ScreenOffsetAdjust.X,
		ComputedScreenOffsetY + Settings.ScreenOffsetAdjust.Y);

	// 배치 Pivot과 같은 축으로 확대·축소해야 앵커에 붙은 상태가 유지된다.
	Widget->SetRenderTransformPivot(Settings.Pivot);
	Widget->SetRenderScale(FVector2D(Scale, Scale));

	// Translation은 Scale과 별도로 합성되므로(Concatenate 순서상 마지막) 직접 곱한다.
	Widget->SetRenderTranslation(Offset * Scale);
}

void USFOverheadUIComponent::HandleUIScaleChanged(float InScale)
{
	// 꺼져 있는 위젯은 지금 건드리지 않는다. 켜질 때 ApplyUIScale()이 반영한다.
	if (!IsValid(WidgetComponent) || !WidgetComponent->IsVisible())	return;

	ApplyUIScale();
}

void USFOverheadUIComponent::HandleStatusChanged(const FSFOverheadStatus& InStatus)
{
	if (USFOverheadBarWidget* Bar = GetWidget<USFOverheadBarWidget>())
	{
		Bar->ApplyStatus(InStatus);
	}
}
