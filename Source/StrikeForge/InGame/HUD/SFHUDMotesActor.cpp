#include "SFHUDMotesActor.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

ASFHUDMotesActor::ASFHUDMotesActor()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.TickInterval=1.f/24.f;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    Motes=CreateDefaultSubobject<UNiagaraComponent>(TEXT("GemMotes"));
    Motes->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> FX(TEXT("/Game/Game/IN_UI/HUDMotes/NS_HUDGemMotes.NS_HUDGemMotes"));
    Motes->SetAsset(FX.Object);
    Motes->SetAutoActivate(false);
    Motes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Motes->SetCanEverAffectNavigation(false);
    Motes->SetVisibleInSceneCaptureOnly(true);
    Motes->SetCastShadow(false);
    Capture=CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("MotesCapture"));
    Capture->SetupAttachment(RootComponent);
    Capture->SetRelativeLocation({300,0,0});
    Capture->SetRelativeRotation({0,180,0});
    Capture->ProjectionType=ECameraProjectionMode::Orthographic;
    Capture->OrthoWidth=180;
    Capture->CaptureSource=ESceneCaptureSource::SCS_SceneColorHDR;
    Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    Capture->bCaptureEveryFrame=false;
    Capture->bCaptureOnMovement=false;
    Capture->ShowFlags.SetAtmosphere(false);
    Capture->ShowFlags.SetFog(false);
    Capture->ShowFlags.SetDynamicShadows(false);
    Capture->ShowFlags.SetGlobalIllumination(false);
    Capture->ShowFlags.SetBloom(false);
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Game/Game/IN_UI/HUDMotes/M_HUDGemMotes.M_HUDGemMotes"));
    BaseMaterial=Mat.Object;
}
void ASFHUDMotesActor::BeginPlay()
{
    Super::BeginPlay();
    Target=NewObject<UTextureRenderTarget2D>(this);
    Target->ClearColor=FLinearColor(0,0,0,1);
    Target->InitCustomFormat(256,128,PF_FloatRGBA,false);
    Capture->TextureTarget=Target;
    Capture->ShowOnlyComponent(Motes);
    if(BaseMaterial)
    {
        Material=UMaterialInstanceDynamic::Create(BaseMaterial,this);
        Material->SetTextureParameterValue(TEXT("Scene"),Target);
    }
}
void ASFHUDMotesActor::RequestFrame() { LastRequest=FPlatformTime::Seconds(); }
void ASFHUDMotesActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const bool bVisible=FPlatformTime::Seconds()-LastRequest<.2;
    if(bVisible)
    {
        if(!Motes->IsActive()) Motes->Activate(true);
        Capture->CaptureScene();
    }
    else if(Motes->IsActive()) Motes->DeactivateImmediate();
}
