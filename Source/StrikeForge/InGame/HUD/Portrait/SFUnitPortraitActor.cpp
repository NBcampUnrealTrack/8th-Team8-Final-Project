// SFUnitPortraitActor.cpp


#include "SFUnitPortraitActor.h"

#include "Animation/AnimationAsset.h"
#include "Components/SpotLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Logging/SFLog.h"

ASFUnitPortraitActor::ASFUnitPortraitActor()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));

    SetRootComponent(SceneRoot);

    PortraitMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PortraitMesh"));

    PortraitMesh->SetupAttachment(SceneRoot);

    PortraitCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("PortraitCapture"));

    PortraitCapture->SetupAttachment(SceneRoot);

    PortraitSpotLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("PortraitSpotLight"));

    PortraitSpotLight->SetupAttachment(SceneRoot);

    PortraitCapture->SetRelativeLocation(PortraitCameraLocation);

    PortraitCapture->SetRelativeRotation(PortraitCameraRotation);

    PortraitCapture->FOVAngle = PortraitFieldOfView;

    PortraitCapture->bCaptureEveryFrame = true;
    PortraitCapture->bCaptureOnMovement = true;

    PortraitMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

void ASFUnitPortraitActor::BeginPlay()
{
    Super::BeginPlay();

    if (!IsValid(PortraitCapture))
    {
        SF_LOG(Error, "[SFUnitPortraitActor] PortraitCapture is invalid.");

        return;
    }

    if (!IsValid(PortraitRenderTarget))
    {
        SF_LOG(Warning, "[SFUnitPortraitActor] PortraitRenderTarget is not assigned.");

        return;
    }

    PortraitCapture->TextureTarget = PortraitRenderTarget;
}

bool ASFUnitPortraitActor::ApplyPortrait(USkeletalMesh* NewPortraitMesh, UAnimationAsset* NewPortraitAnimation)
{
    if (!IsValid(PortraitMesh) || !IsValid(NewPortraitMesh))
    {
        SF_LOG(Warning, "[SFUnitPortraitActor] ApplyPortrait failed: Portrait Mesh is invalid.");

        return false;
    }

    PortraitMesh->SetSkeletalMesh(NewPortraitMesh);

    if (IsValid(NewPortraitAnimation))
    {
        PortraitMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);

        PortraitMesh->SetAnimation(NewPortraitAnimation);

        PortraitMesh->Play(true);
    }
    else
    {
        SF_LOG(Warning, "[SFUnitPortraitActor] Portrait Animation is invalid.");
    }

    PortraitMesh->SetVisibility(true, true);

    SetPortraitCaptureEnabled(true);

    return true;
}

void ASFUnitPortraitActor::ClearPortrait()
{
    if (IsValid(PortraitMesh))
    {
        PortraitMesh->Stop();
        PortraitMesh->SetVisibility(false, true);
    }

    SetPortraitCaptureEnabled(false);
}

void ASFUnitPortraitActor::SetPortraitCaptureEnabled(
    bool bEnabled
)
{
    if (!IsValid(PortraitCapture))
    {
        return;
    }

    PortraitCapture->bCaptureEveryFrame = bEnabled;

    PortraitCapture->bCaptureOnMovement = bEnabled;

    if (bEnabled)
    {
        PortraitCapture->CaptureScene();
    }
}

