#include "SFOverheadStatusComponent.h"

#include "ActorComponent/Stat/HealthComponent.h"
#include "Components/MeshComponent.h"
#include "Game/Subsystem/SFOverheadUISubsystem.h"
#include "Kismet/KismetMathLibrary.h"

static_assert(static_cast<int32>(ESFOverheadElement::MAX) <= 8,
	"ElementMask가 uint8이다. 요소가 8개를 넘으면 타입을 넓혀야 한다.");

namespace SFOverheadStatus
{
	/** 이보다 작은 변화는 화면에서 구분되지 않는다. 불필요한 슬레이트 무효화를 막는다. */
	constexpr float RatioEpsilon = 0.005f;
}

USFOverheadStatusComponent::USFOverheadStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USFOverheadStatusComponent::BeginPlay()
{
	Super::BeginPlay();

	// 등록이 먼저다. 이후의 표시 변경은 모두 매니저를 거친다.
	if (USFOverheadUISubsystem* OverheadUI = USFOverheadUISubsystem::Get(this))
	{
		OverheadUI->RegisterEntry(this);
	}

	// 메시를 받기 전이라도 기본 배치값으로 크기를 잡아 둔다.
	RefreshLayout();
}

void USFOverheadStatusComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USFOverheadUISubsystem* OverheadUI = USFOverheadUISubsystem::Get(this))
	{
		OverheadUI->UnregisterEntry(this);
	}

	Super::EndPlay(EndPlayReason);
}

#pragma region Status
void USFOverheadStatusComponent::BindHealth(UHealthComponent* InHealthComponent)
{
	if (!IsValid(InHealthComponent))	return;

	InHealthComponent->OnHealthValueChanged.AddUniqueDynamic(
		this, &USFOverheadStatusComponent::HandleHealthChanged);

	// 구독 이후의 변화만 오므로 현재 값을 한 번 반영한다.
	HandleHealthChanged(
		InHealthComponent->GetCurrentHealth(),
		InHealthComponent->GetCurrentHealth(),
		InHealthComponent->GetMaxHealth());
}

void USFOverheadStatusComponent::UnbindHealth(UHealthComponent* InHealthComponent)
{
	if (!IsValid(InHealthComponent))	return;

	InHealthComponent->OnHealthValueChanged.RemoveDynamic(
		this, &USFOverheadStatusComponent::HandleHealthChanged);
}

void USFOverheadStatusComponent::HandleHealthChanged(float InBeforeHP, float InCurrentHP, float InMaxHP)
{
	const float NewRatio = FMath::Clamp(
		UKismetMathLibrary::SafeDivide(InCurrentHP, InMaxHP), 0.f, 1.f);

	if (FMath::IsNearlyEqual(Status.HealthRatio, NewRatio, SFOverheadStatus::RatioEpsilon))	return;

	Status.HealthRatio = NewRatio;
	NotifyChanged();
}

void USFOverheadStatusComponent::HandleProgressRatio(float InRatio)
{
	SetProgressRatio(InRatio);
}

void USFOverheadStatusComponent::HandleExtractProgressRatio(EResourceType InResourceType, float InRatio)
{
	// 자원 종류는 색을 정할 때 한 번 쓰이고, 매 갱신에는 필요 없다.
	SetProgressRatio(InRatio);
}

void USFOverheadStatusComponent::SetProgressRatio(float InRatio)
{
	const float NewRatio = FMath::Clamp(InRatio, 0.f, 1.f);

	if (FMath::IsNearlyEqual(Status.ProgressRatio, NewRatio, SFOverheadStatus::RatioEpsilon))	return;

	Status.ProgressRatio = NewRatio;
	NotifyChanged();
}

void USFOverheadStatusComponent::SetHealthColor(const FLinearColor& InColor)
{
	if (Status.HealthColor.Equals(InColor))	return;

	Status.HealthColor = InColor;
	NotifyChanged();
}

void USFOverheadStatusComponent::SetProgressColor(const FLinearColor& InColor)
{
	if (Status.ProgressColor.Equals(InColor))	return;

	Status.ProgressColor = InColor;
	NotifyChanged();
}

void USFOverheadStatusComponent::SetSynergyBonusPercent(float InBonusPercent)
{
	// 게임플레이에서 계산된 생산 보너스를 표시 상태에 보관하고, 실제 변경 때만 UI에 알린다.
	const float NewBonusPercent = FMath::Max(0.f, InBonusPercent);
	if (FMath::IsNearlyEqual(Status.SynergyBonusPercent, NewBonusPercent))	return;

	Status.SynergyBonusPercent = NewBonusPercent;
	NotifyChanged();
}

void USFOverheadStatusComponent::SetStatusIcon(ESFOverheadStatusIcon InIcon)
{
	// 값을 먼저 바꾼다. 요소를 켜는 순간 판이 뜨면서 이 값으로 처음 그려진다.
	if (Status.StatusIcon != InIcon)
	{
		Status.StatusIcon = InIcon;
		NotifyChanged();
	}

	SetElementVisible(ESFOverheadElement::StatusIcon, InIcon != ESFOverheadStatusIcon::None);
}

void USFOverheadStatusComponent::NotifyChanged()
{
	OnStatusChanged.Broadcast(Status);
}
#pragma endregion

#pragma region Presentation
void USFOverheadStatusComponent::SetAnchorMesh(UMeshComponent* InMeshComponent)
{
	AnchorMesh = InMeshComponent;
	RefreshLayout();
}

void USFOverheadStatusComponent::ApplySettings(const FSFOverheadUISettings& InSettings)
{
	Settings = InSettings;
	RefreshLayout();
}

void USFOverheadStatusComponent::RefreshLayout()
{
	FVector AnchorLocation = FVector::ZeroVector;
	float HalfWidth = 0.f;
	float HalfHeight = 0.f;

	if (IsValid(AnchorMesh))
	{
		/*
		 * 앵커는 메시 중심이다.
		 *
		 * 윗면에 붙이면 그 점이 건물 꼭대기에 찍히는데, 피치가 기운 카메라에서는
		 * 그 지점이 건물 실루엣의 시각적 중심에서 벗어나 보인다. 중심을 기준으로 잡고
		 * 비율로 밀어 올리는 쪽이 눈에 자연스럽고, 건물마다 값으로 조절하기도 쉽다.
		 */
		const FBoxSphereBounds LocalBounds =
			AnchorMesh->CalcBounds(AnchorMesh->GetRelativeTransform());

		/*
		 * 앞뒤는 액터 기준 좌표의 Y축으로 옮긴다.
		 * 건물이 90도 회전해 놓이면서 메시 정면이 액터의 +Y 쪽이 됐다.
		 * LocalBounds는 루트 기준이라 BoxExtent.Y가 곧 건물 앞뒤 반길이다.
		 * 월드로 바꿀 때 액터 회전이 적용되므로 건물이 몇 도로 놓여도 정면을 따라간다.
		 * 0이면 루트 위(지금까지의 중앙 자리)에 그대로 둔다.
		 */
		AnchorLocation = FVector(
			0.f,
			LocalBounds.BoxExtent.Y * Settings.ForwardRatio,
			LocalBounds.Origin.Z);

		// 크기·오프셋 계산은 회전을 빼고 스케일만 적용한다.
		// 월드 바운즈를 쓰면 45도로 배치된 대상의 폭이 부풀어 오른다.
		const FBoxSphereBounds ScaledBounds = AnchorMesh->CalcBounds(
			FTransform(FQuat::Identity, FVector::ZeroVector, AnchorMesh->GetComponentScale()));

		HalfWidth  = ScaledBounds.BoxExtent.X;
		HalfHeight = ScaledBounds.BoxExtent.Z;
	}

	Layout.AnchorLocation = AnchorLocation;
	Layout.DrawSize = FVector2D(
		FMath::Clamp(HalfWidth * Settings.WidthRatio, Settings.WidthRange.X, Settings.WidthRange.Y),
		Settings.PanelHeight);

	// 판의 위아래 위치는 월드가 아니라 화면 픽셀로 준다. 메시가 클수록 더 멀리 띄운다.
	Layout.ScreenOffsetY = HalfHeight * Settings.VerticalRatio;
	Layout.Pivot = Settings.Pivot;
	Layout.ElementSpacing = Settings.ElementSpacing;

	NotifyPresentationChanged();
}

void USFOverheadStatusComponent::SetElementVisible(ESFOverheadElement InElement, bool bInVisible)
{
	const int32 Index = static_cast<int32>(InElement);
	if (Index < 0 || Index >= static_cast<int32>(ESFOverheadElement::MAX))	return;

	const uint8 Bit = static_cast<uint8>(1 << Index);
	const bool bWasVisible = (ElementMask & Bit) != 0;
	if (bWasVisible == bInVisible)	return;

	ElementMask = bInVisible
		? static_cast<uint8>(ElementMask | Bit)
		: static_cast<uint8>(ElementMask & ~Bit);

	NotifyPresentationChanged();
}

void USFOverheadStatusComponent::SetSuppressed(bool bInSuppressed)
{
	if (bSuppressed == bInSuppressed)	return;

	bSuppressed = bInSuppressed;
	NotifyPresentationChanged();
}

void USFOverheadStatusComponent::PlayResourceGain(EResourceType InResourceType, int32 InAmount)
{
	if (USFOverheadUISubsystem* OverheadUI = USFOverheadUISubsystem::Get(this))
	{
		OverheadUI->PlayResourceGain(this, InResourceType, InAmount);
	}
}

FVector USFOverheadStatusComponent::GetAnchorWorldLocation() const
{
	const AActor* Owner = GetOwner();
	const USceneComponent* Root = IsValid(Owner) ? Owner->GetRootComponent() : nullptr;

	// 건물 기준 앵커를 루트 트랜스폼으로 월드에 옮긴다. 회전이 적용되어 앞뒤 오프셋이 건물 정면을 따라간다.
	return IsValid(Root)
		? Root->GetComponentTransform().TransformPosition(Layout.AnchorLocation)
		: FVector::ZeroVector;
}

void USFOverheadStatusComponent::NotifyPresentationChanged()
{
	if (USFOverheadUISubsystem* OverheadUI = USFOverheadUISubsystem::Get(this))
	{
		OverheadUI->RefreshEntry(this);
	}
}
#pragma endregion
