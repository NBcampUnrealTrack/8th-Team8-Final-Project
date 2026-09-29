#include "SFStartSceneAmbience.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"
#include "EngineUtils.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ASFStartSceneAmbience::ASFStartSceneAmbience()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.TickInterval=1.f/30.f;
    SetActorEnableCollision(false);
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    WarmMotes=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ForestFireflies"));
    EtherMotes=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("EtherMotes"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Game/Core/UI/LobbyAtmosphere/M_StartWorldMote.M_StartWorldMote"));
    for (UInstancedStaticMeshComponent* Component : {WarmMotes.Get(),EtherMotes.Get()})
    {
        Component->SetupAttachment(RootComponent);
        Component->SetStaticMesh(Plane.Object);
        Component->SetMaterial(0,Glow.Object);
        Component->SetMobility(EComponentMobility::Movable);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetCastShadow(false);
        Component->SetCanEverAffectNavigation(false);
        Component->bReceivesDecals=false;
    }
}

void ASFStartSceneAmbience::SetViewer(APlayerController* Player)
{
    Viewer=Player;
    // The level owns the light. Discover it once, then animate only its intensity.
    for (TActorIterator<APointLight> It(GetWorld()); It; ++It)
    {
        if (It->ActorHasTag(TEXT("SF_StartFireLight")))
        {
            CampfireLight=Cast<UPointLightComponent>(It->GetLightComponent());
            if (CampfireLight.IsValid()) CampfireIntensity=CampfireLight->Intensity;
            break;
        }
    }
    if (auto* Material=WarmMotes->CreateDynamicMaterialInstance(0)) Material->SetVectorParameterValue(TEXT("Tint"),FLinearColor(1.f,.67f,.23f));
    if (auto* Material=EtherMotes->CreateDynamicMaterialInstance(0)) Material->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.18f,.85f,1.f));
}

void ASFStartSceneAmbience::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Age+=DeltaSeconds;
    if (CampfireLight.IsValid())
    {
        const float Flicker=.92f+.05f*FMath::Sin(Age*7.3f)+.03f*FMath::Sin(Age*13.7f);
        CampfireLight->SetIntensity(CampfireIntensity*Flicker);
    }
    APlayerController* Player=Viewer.Get();
    if (!Player || !Player->PlayerCameraManager || Age<.35f) return;
    FVector Camera; FRotator Rotation;
    Player->GetPlayerViewPoint(Camera,Rotation);
    if (Motes.IsEmpty())
    {
        // Anchor once in the level. A moving camera produces real parallax and occlusion.
        const FVector Forward=Rotation.Vector();
        const FVector Right=FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y);
        const FVector Up=FRotationMatrix(Rotation).GetUnitAxis(EAxis::Z);
        int32 Width=0,Height=0;
        Player->GetViewportSize(Width,Height);
        const float Aspect=Height>0 ? float(Width)/Height : 16.f/9.f;
        const float Horizontal=FMath::Tan(FMath::DegreesToRadians(Player->PlayerCameraManager->GetFOVAngle()*.5f));
        const float Vertical=Horizontal/FMath::Max(Aspect,.5f);
        FRandomStream Random(7319);
        for (int32 I=0; I<28; ++I)
        {
            FMote Mote;
            float Depth=Random.FRandRange(160.f,700.f);
            float Side=Random.FRandRange(.22f,.86f)*(I%2 ? -1.f : 1.f);
            Mote.Origin=Camera+Forward*Depth+Right*(Depth*Side*Horizontal)+Up*(Depth*Vertical*Random.FRandRange(-.72f,.60f));
            Mote.Phase=Random.FRandRange(0.f,2.f*PI);
            Mote.Size=Depth*Horizontal*Random.FRandRange(.012f,.024f);
            Mote.Group=I%4==0 ? 1 : 0;
            auto* Component=Mote.Group ? EtherMotes.Get() : WarmMotes.Get();
            Mote.Instance=Component->AddInstance(FTransform(FRotator::ZeroRotator,Mote.Origin,FVector(.01f)),true);
            Motes.Add(Mote);
        }
    }
    for (const FMote& Mote : Motes)
    {
        FVector Position=Mote.Origin+FVector(FMath::Sin(Age*.36f+Mote.Phase)*14.f,FMath::Cos(Age*.27f+Mote.Phase)*11.f,FMath::Sin(Age*.43f+Mote.Phase)*17.f);
        float Pulse=.35f+.65f*FMath::Square(.5f+.5f*FMath::Sin(Age*1.25f+Mote.Phase));
        float Size=Mote.Size*Pulse/100.f;
        FRotator Facing=FRotationMatrix::MakeFromZ(Camera-Position).Rotator();
        auto* Component=Mote.Group ? EtherMotes.Get() : WarmMotes.Get();
        Component->UpdateInstanceTransform(Mote.Instance,FTransform(Facing,Position,FVector(Size)),true,false,true);
    }
    WarmMotes->MarkRenderStateDirty();
    EtherMotes->MarkRenderStateDirty();
}
