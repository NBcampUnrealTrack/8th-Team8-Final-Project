#include "SFCharacterSpotlightActor.h"
#include "Components/SpotLightComponent.h"
#include "NiagaraComponent.h"

ASFCharacterSpotlightActor::ASFCharacterSpotlightActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SpotLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("SpotLight"));
	SpotLight->SetupAttachment(SceneRoot);
	SpotLight->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	SpotLight->CastShadows = false;

	SparkleEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SparkleEffect"));
	SparkleEffect->SetupAttachment(SceneRoot);
}

void ASFCharacterSpotlightActor::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(SpotLight))
	{
		SpotLight->SetIntensity(LightIntensity);
		SpotLight->SetLightColor(LightColor);
		SpotLight->SetInnerConeAngle(InnerConeAngle);
		SpotLight->SetOuterConeAngle(OuterConeAngle);
		SpotLight->SetAttenuationRadius(AttenuationRadius);
	}

	if (IsValid(SparkleEffect) && SparkleSystemAsset)
	{
		SparkleEffect->SetAsset(SparkleSystemAsset);
		SparkleEffect->Activate(true);
	}
}