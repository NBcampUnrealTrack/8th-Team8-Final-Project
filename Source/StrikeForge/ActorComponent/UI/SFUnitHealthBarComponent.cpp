#include "ActorComponent/UI/SFUnitHealthBarComponent.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "ActorComponent/Stat/HealthComponent.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Game/Subsystem/SFCombatSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"

void USFUnitHealthBarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>();
	Frame->SetBrushColor(FLinearColor(0.015f, 0.02f, 0.025f, 0.95f));
	Frame->SetPadding(FMargin(1.f));
	Bar = WidgetTree->ConstructWidget<UProgressBar>();
	FProgressBarStyle Style = Bar->GetWidgetStyle();
	Style.BackgroundImage = *FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	Style.BackgroundImage.TintColor = FSlateColor(FLinearColor(0.12f, 0.12f, 0.12f));
	Style.FillImage = *FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	Bar->SetWidgetStyle(Style);
	Bar->SetBorderPadding(FVector2D::ZeroVector);
	Frame->SetContent(Bar);
	WidgetTree->RootWidget = Frame;
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void USFUnitHealthBarWidget::UpdateHealth(float Ratio, const FLinearColor& Color)
{
	if (Bar)
	{
		Bar->SetPercent(FMath::Clamp(Ratio, 0.f, 1.f));
		Bar->SetFillColorAndOpacity(Color);
	}
}

USFUnitHealthBarComponent::USFUnitHealthBarComponent()
{
	SetWidgetSpace(EWidgetSpace::Screen);
	SetWidgetClass(USFUnitHealthBarWidget::StaticClass());
	SetDrawSize(FVector2D(40.f, 6.f));
	SetPivot(FVector2D(0.5f, 1.f));
	SetWindowFocusable(false);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	SetVisibility(false);
	PrimaryComponentTick.TickInterval = 0.05f;
}

void USFUnitHealthBarComponent::BeginPlay()
{
	// The Start map's display units never show combat health bars, including in PIE.
	const bool bStartMap = UGameplayStatics::GetCurrentLevelName(this, true) == TEXT("Start");
	if (bStartMap) SetWidgetClass(nullptr);
	Super::BeginPlay();
	Health = GetOwner()->FindComponentByClass<UHealthComponent>();
	if (Health && !bStartMap)
	{
		Health->OnHealthValueChanged.AddUniqueDynamic(this, &ThisClass::HealthChanged);
		HealthChanged(Health->GetCurrentHealth(), Health->GetCurrentHealth(), Health->GetMaxHealth());
	}
	if (bStartMap)
	{
		SetVisibility(false);
		SetComponentTickEnabled(false);
	}
	// Imported unit blueprints may still carry their old, oversized status widget.
	TInlineComponentArray<UWidgetComponent*> Widgets(GetOwner());
	for (UWidgetComponent* LegacyWidget : Widgets)
	{
		if (LegacyWidget != this && LegacyWidget->GetFName() == TEXT("StatusWidget"))
		{
			LegacyWidget->SetVisibility(false);
			LegacyWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			LegacyWidget->SetCanEverAffectNavigation(false);
			LegacyWidget->SetWidgetClass(nullptr);
		}
	}
}

void USFUnitHealthBarComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Health) Health->OnHealthValueChanged.RemoveDynamic(this, &ThisClass::HealthChanged);
	Super::EndPlay(EndPlayReason);
}

void USFUnitHealthBarComponent::HealthChanged(float Before, float Current, float Maximum)
{
	// No percentage threshold: even 1 HP missing from a very large health pool shows the bar.
	const bool bDamaged = Maximum > 0.f && Current > 0.f && Current < Maximum;
	SetVisibility(bDamaged);
	if (UUserWidget* HealthWidget = GetUserWidgetObject())
		HealthWidget->SetVisibility(bDamaged ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	// Screen-space widgets stay in the viewport until explicitly removed or ticked.
	// Remove before disabling tick so healing/death/respawn cannot leave an old fill on screen.
	if (!bDamaged) RemoveWidgetFromScreen();
	SetComponentTickEnabled(bDamaged);
	if (bDamaged) RefreshLayout();
}

void USFUnitHealthBarComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RefreshLayout();
}

void USFUnitHealthBarComponent::RefreshLayout()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	APlayerController* PC = USFCombatSubsystem::Get(this)->GetLocalPlayerController();
	if (!Character || !Health) return;
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	FVector Anchor = Character->GetActorLocation() + FVector(0.f, 0.f, HalfHeight + 12.f);
	if (USkeletalMeshComponent* Mesh = Character->GetMesh(); Mesh && Mesh->GetSkeletalMeshAsset())
	{
		const FBoxSphereBounds MeshBounds = Mesh->CalcBounds(Mesh->GetComponentTransform());
		Anchor.Z = FMath::Max(Anchor.Z, MeshBounds.Origin.Z + MeshBounds.BoxExtent.Z + 12.f);
	}
	SetWorldLocation(Anchor);
	if (PC)
	{
		FVector CameraLocation; FRotator CameraRotation;
		PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
		const FVector Right = FRotationMatrix(CameraRotation).GetUnitAxis(EAxis::Y);
		FVector2D LeftPixel, RightPixel;
		if (PC->ProjectWorldLocationToScreen(Anchor - Right * Radius, LeftPixel) &&
			PC->ProjectWorldLocationToScreen(Anchor + Right * Radius, RightPixel))
		{
			const float DPI = FMath::Max(0.1f, UWidgetLayoutLibrary::GetViewportScale(this));
			SetDrawSize(FVector2D(FMath::Clamp(FVector2D::Distance(LeftPixel, RightPixel) / DPI, 22.f, 64.f), 6.f));
		}
	}
	if (USFUnitHealthBarWidget* HealthWidget = Cast<USFUnitHealthBarWidget>(GetUserWidgetObject()))
	{
		const UCombatTargetComponent* Target = Character->FindComponentByClass<UCombatTargetComponent>();
		const USFCombatSubsystem* Combat = USFCombatSubsystem::Get(this);
		const bool bAlly = Target && Combat && Target->GetTeamIndex() == Combat->GetLocalPlayerTeam();
		HealthWidget->UpdateHealth(Health->GetHealthPercent(), bAlly ? FLinearColor(0.1f, 0.9f, 0.55f) : FLinearColor(0.95f, 0.22f, 0.16f));
	}
}
