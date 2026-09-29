#include "SFMenuAtmosphereWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

USFMenuAtmosphereWidget::USFMenuAtmosphereWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Asset(
		TEXT("/Game/Core/UI/LobbyAtmosphere/M_LobbyAtmosphere.M_LobbyAtmosphere"));
	AtmosphereMaterial = Asset.Object;
}

void USFMenuAtmosphereWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (!WidgetTree || !AtmosphereMaterial) return;
	UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("AtmosphereImage"));
	Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = Image;
	Material = UMaterialInstanceDynamic::Create(AtmosphereMaterial, this);
	Image->SetBrushFromMaterial(Material);
	Material->SetScalarParameterValue(TEXT("Elapsed"), 0.f);
}

void USFMenuAtmosphereWidget::SetDeparture(bool bInDeparture)
{
	Elapsed = 0.f;
	if (Material)
	{
		Material->SetScalarParameterValue(TEXT("Departure"), bInDeparture ? 1.f : 0.f);
		Material->SetScalarParameterValue(TEXT("Elapsed"), 0.f);
	}
}

void USFMenuAtmosphereWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Elapsed += InDeltaTime;
	float PresentationTime = Elapsed;
#if WITH_DEV_AUTOMATION_TESTS
	PresentationTime = PreviewTime.Get(Elapsed);
#endif
	if (Material) Material->SetScalarParameterValue(TEXT("Elapsed"), PresentationTime);
}

#if WITH_DEV_AUTOMATION_TESTS
void USFMenuAtmosphereWidget::SetPreviewTime(TOptional<float> Time)
{
	PreviewTime = Time;
	if (Material) Material->SetScalarParameterValue(TEXT("Elapsed"), PreviewTime.Get(Elapsed));
}
#endif
