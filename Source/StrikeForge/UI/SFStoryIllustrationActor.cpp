#include "SFStoryIllustrationActor.h"

#include "Animation/AnimationAsset.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

ASFStoryIllustrationActor::ASFStoryIllustrationActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 1.f/30.f;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Stage")));
    Beast = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BeastCommander"));
    Etherian = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("EtherianCommander"));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> BeastMesh(TEXT("/Game/Game/Race_Beast/Character/Unit_Commander/Meshes/SKM_Beast_Commander.SKM_Beast_Commander"));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> EtherMesh(TEXT("/Game/Game/Race_Etherial/Character/Unit_Commander/Meshes/HumanCommander.HumanCommander"));
    static ConstructorHelpers::FObjectFinder<UAnimationAsset> BeastIdle(TEXT("/Game/Game/Race_Beast/Character/Unit_Commander/Anim/AS_Beast_Commander_Idle.AS_Beast_Commander_Idle"));
    static ConstructorHelpers::FObjectFinder<UAnimationAsset> EtherIdle(TEXT("/Game/Game/Race_Etherial/Character/Unit_Commander/Anim/AS_EtherCommanderIdle.AS_EtherCommanderIdle"));
    Beast->SetSkeletalMesh(BeastMesh.Object);
    Etherian->SetSkeletalMesh(EtherMesh.Object);
    for (USkeletalMeshComponent* Mesh : {Beast.Get(),Etherian.Get()})
    {
        Mesh->SetupAttachment(RootComponent);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetCanEverAffectNavigation(false);
        Mesh->SetCastShadow(false);
        Mesh->SetVisibleInSceneCaptureOnly(true);
        Mesh->SetLightingChannels(false,true,false);
        Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    }
    Beast->AnimationData.AnimToPlay=BeastIdle.Object;
    Etherian->AnimationData.AnimToPlay=EtherIdle.Object;
    Beast->AnimationData.bSavedLooping=true;
    Etherian->AnimationData.bSavedLooping=true;

    EtherParticles=CreateDefaultSubobject<UNiagaraComponent>(TEXT("EtherMotes"));
    WarmParticles=CreateDefaultSubobject<UNiagaraComponent>(TEXT("BeastEmbers"));
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> EtherFX(TEXT("/Game/Core/UI/Story/NS_StoryEther.NS_StoryEther"));
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> WarmFX(TEXT("/Game/Core/UI/Story/NS_StoryWarm.NS_StoryWarm"));
    EtherParticles->SetAsset(EtherFX.Object);
    WarmParticles->SetAsset(WarmFX.Object);
    for (UNiagaraComponent* FX : {EtherParticles.Get(),WarmParticles.Get()})
    {
        FX->SetupAttachment(RootComponent);
        FX->SetAutoActivate(false);
        FX->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        FX->SetCanEverAffectNavigation(false);
        FX->SetVisibleInSceneCaptureOnly(true);
        FX->SetCastShadow(false);
    }

    auto* Key=CreateDefaultSubobject<UPointLightComponent>(TEXT("PortraitKey"));
    Key->SetupAttachment(RootComponent);
    Key->SetRelativeLocation({230,-170,220});
    Key->SetIntensity(60000.f);
    Key->SetAttenuationRadius(1000.f);
    Key->SetCastShadows(false);
    Key->SetLightingChannels(false,true,false);
    auto* Rim=CreateDefaultSubobject<UPointLightComponent>(TEXT("EtherRim"));
    Rim->SetupAttachment(RootComponent);
    Rim->SetRelativeLocation({100,190,110});
    Rim->SetLightColor(FLinearColor(.35f,.86f,1.f));
    Rim->SetIntensity(30000.f);
    Rim->SetAttenuationRadius(800.f);
    Rim->SetCastShadows(false);
    Rim->SetLightingChannels(false,true,false);

    Capture=CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("IllustrationCapture"));
    Capture->SetupAttachment(RootComponent);
    Capture->SetRelativeLocation({500,0,0});
    Capture->SetRelativeRotation({0,180,0});
    Capture->ProjectionType=ECameraProjectionMode::Orthographic;
    Capture->OrthoWidth=360.f;
    Capture->CaptureSource=ESceneCaptureSource::SCS_SceneColorHDR;
    Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    Capture->bCaptureEveryFrame=false;
    Capture->bCaptureOnMovement=false;
    Capture->ShowFlags.SetAtmosphere(false);
    Capture->ShowFlags.SetFog(false);
    Capture->ShowFlags.SetBloom(false);
    Capture->ShowFlags.SetDynamicShadows(false);
    Capture->ShowFlags.SetGlobalIllumination(false);
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/Core/UI/Story/M_StoryCaptureUI.M_StoryCaptureUI"));
    CaptureMaterial=Material.Object;
}

void ASFStoryIllustrationActor::BeginPlay()
{
    Super::BeginPlay();
    Target=NewObject<UTextureRenderTarget2D>(this);
    Target->ClearColor=FLinearColor(0,0,0,1);
    Target->InitCustomFormat(512,512,PF_FloatRGBA,false);
    Capture->TextureTarget=Target;
    Capture->ShowOnlyComponent(Beast);
    Capture->ShowOnlyComponent(Etherian);
    Capture->ShowOnlyComponent(EtherParticles);
    Capture->ShowOnlyComponent(WarmParticles);
    SetChapter(0);
}

UMaterialInstanceDynamic* ASFStoryIllustrationActor::GetPresentationMaterial()
{
    if (!PresentationMaterial && CaptureMaterial && Target)
    {
        PresentationMaterial=UMaterialInstanceDynamic::Create(CaptureMaterial,this);
        PresentationMaterial->SetTextureParameterValue(TEXT("Scene"),Target);
    }
    return PresentationMaterial;
}

void ASFStoryIllustrationActor::SetChapter(int32 Index)
{
    Chapter=Index;
    Elapsed=0.f;
    for (USkeletalMeshComponent* Mesh : {Beast.Get(),Etherian.Get()})
    {
        const bool bShow=Mesh==Beast ? Index==2 || Index==4 : Index==3 || Index==4;
        Mesh->SetVisibility(bShow);
        Mesh->SetComponentTickEnabled(bShow);
        const auto* Asset=Mesh->GetSkeletalMeshAsset();
        if (Asset)
        {
            const FBoxSphereBounds Bounds=Asset->GetBounds();
            const float Scale=(Index==4 ? 190.f : 255.f)/FMath::Max(1.f,Bounds.BoxExtent.Z*2.f);
            Mesh->SetRelativeScale3D(FVector(Scale));
            const float Side=Index==4 ? (Mesh==Beast ? 68.f : -68.f) : -38.f;
            Mesh->SetRelativeLocation({15,Side,-120.f-(Bounds.Origin.Z-Bounds.BoxExtent.Z)*Scale});
            Mesh->SetRelativeRotation({0,Mesh==Beast ? -78.f : -102.f,0});
        }
        if (bShow) Mesh->Play(true);
    }
    EtherParticles->SetRelativeLocation({55,Index==4 ? -65.f : 0.f,-15});
    WarmParticles->SetRelativeLocation({55,Index==4 ? 65.f : 0.f,-15});
    if (Index!=2) EtherParticles->Activate(true); else EtherParticles->DeactivateImmediate();
    if (Index==2 || Index==4) WarmParticles->Activate(true); else WarmParticles->DeactivateImmediate();
}

void ASFStoryIllustrationActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Elapsed+=DeltaSeconds;
    // Foreground breathing and a slower lateral sway complement the authored idle animations.
    for (USkeletalMeshComponent* Mesh : {Beast.Get(),Etherian.Get()})
    {
        if (!Mesh->IsVisible()) continue;
        const float Side=Chapter==4 ? (Mesh==Beast ? 68.f : -68.f) : -38.f;
        FVector Position=Mesh->GetRelativeLocation();
        Position.Y=Side+FMath::Sin(Elapsed*.65f)*2.f;
        Mesh->SetRelativeLocation(Position);
    }
    Capture->CaptureScene();
}

bool ASFStoryIllustrationActor::HasActiveParticles() const
{
    return EtherParticles->IsActive() || WarmParticles->IsActive();
}
