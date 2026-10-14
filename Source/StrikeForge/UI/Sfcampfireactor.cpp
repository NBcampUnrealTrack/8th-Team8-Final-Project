#include "SFCampfireActor.h"

#include "Components/PointLightComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Math/UnrealMathUtility.h"

ASFCampfireActor::ASFCampfireActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// =========================================================
	// ROOT
	// =========================================================

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;


	// =========================================================
	// FIRE LIGHT
	// =========================================================

	FireLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FireLight"));
	FireLight->SetupAttachment(RootComponent);

	// 캠프파이어 위쪽
	FireLight->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));

	// 따뜻한 주황색
	FireLight->SetLightColor(FLinearColor(1.0f, 0.28f, 0.06f));

	FireLight->Intensity = BaseIntensity;
	FireLight->AttenuationRadius = 500.0f;

	// 그림자 켜기
	FireLight->CastShadows = true;

	// 살짝 부드럽게
	FireLight->SourceRadius = 100.0f;
	FireLight->SoftSourceRadius = 150.0f;


	// =========================================================
	// MAIN FLAME
	// =========================================================

	MainFire = CreateDefaultSubobject<UNiagaraComponent>(TEXT("MainFire"));
	MainFire->SetupAttachment(RootComponent);

	MainFire->SetRelativeLocation(
		FVector(0.0f, 0.0f, 35.0f)
	);

	MainFire->SetRelativeScale3D(
		FVector(1.15f, 1.15f, 1.25f)
	);

	MainFireBaseLocation = MainFire->GetRelativeLocation();


	// =========================================================
	// SMALL FLAME
	// =========================================================

	SmallFire = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SmallFire"));
	SmallFire->SetupAttachment(RootComponent);

	SmallFire->SetRelativeLocation(
		FVector(15.0f, -8.0f, 30.0f)
	);

	SmallFire->SetRelativeScale3D(
		FVector(0.65f, 0.65f, 0.8f)
	);

	SmallFireBaseLocation = SmallFire->GetRelativeLocation();


	// =========================================================
	// EMBERS
	// =========================================================

	Embers = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Embers"));
	Embers->SetupAttachment(RootComponent);

	Embers->SetRelativeLocation(
		FVector(0.0f, 0.0f, 35.0f)
	);

	Embers->SetRelativeScale3D(
		FVector(1.0f, 1.0f, 1.0f)
	);
}


void ASFCampfireActor::BeginPlay()
{
	Super::BeginPlay();

	TimePassed = FMath::FRandRange(0.0f, 100.0f);

	// 초기 위치 저장
	MainFireBaseLocation = MainFire->GetRelativeLocation();
	SmallFireBaseLocation = SmallFire->GetRelativeLocation();
}


void ASFCampfireActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TimePassed += DeltaTime;


	// =========================================================
	// FIRE LIGHT FLICKER
	// =========================================================

	const float Noise =
		FMath::PerlinNoise1D(TimePassed * FlickerSpeed);

	const float Flicker =
		Noise * FlickerAmount;

	FireLight->SetIntensity(
		BaseIntensity + Flicker
	);


	// =========================================================
	// MAIN FLAME SWAY
	// =========================================================

	const float SwayX =
		FMath::Sin(TimePassed * FlameSwaySpeed) *
		FlameSwayAmount;

	const float SwayY =
		FMath::Cos(TimePassed * FlameSwaySpeed * 0.73f) *
		FlameSwayAmount;


	MainFire->SetRelativeLocation(
		MainFireBaseLocation +
		FVector(SwayX, SwayY, 0.0f)
	);


	// =========================================================
	// SMALL FLAME SWAY
	// =========================================================

	const float SmallSwayX =
		FMath::Sin(TimePassed * FlameSwaySpeed * 1.7f) *
		(FlameSwayAmount * 0.6f);

	const float SmallSwayY =
		FMath::Cos(TimePassed * FlameSwaySpeed * 1.3f) *
		(FlameSwayAmount * 0.6f);


	SmallFire->SetRelativeLocation(
		SmallFireBaseLocation +
		FVector(SmallSwayX, SmallSwayY, 0.0f)
	);
}