#include "InGame/HUD/Tutorial/SFTutorialGhostActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Logging/SFLog.h"
#include "Materials/MaterialInterface.h"

ASFTutorialGhostActor::ASFTutorialGhostActor()
{
    // 위아래로 흔들리는 연출 때문에 틱이 필요하다.
    // BobAmplitude 를 0 으로 두면 Tick 첫 줄에서 빠져나간다.
    PrimaryActorTick.bCanEverTick = true;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    GhostMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GhostMesh"));
    GhostMeshComponent->SetupAttachment(SceneRoot);

    // 안내용 모형이다. 무엇과도 부딪히면 안 된다 —
    // 콜리전을 남겨두면 플레이어가 그 자리에 실제 건물을 못 짓는다.
    GhostMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GhostMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    GhostMeshComponent->SetGenerateOverlapEvents(false);
    GhostMeshComponent->SetCanEverAffectNavigation(false);
    GhostMeshComponent->SetCastShadow(false);

}

void ASFTutorialGhostActor::BeginPlay()
{
    Super::BeginPlay();

    BaseLocation = GetActorLocation();

    OnGhostAppeared();
}

void ASFTutorialGhostActor::SetGhostMesh(UStaticMesh* InMesh, const FTransform& InRelativeTransform)
{
    if (!IsValid(GhostMeshComponent) || !IsValid(InMesh))
    {
        return;
    }

    GhostMeshComponent->SetStaticMesh(InMesh);

    // Nanite 끄기는 메시를 붙인 뒤에 적용한다.
    // 생성자에서 걸면 메시가 없어 무시되는 경우가 있다.
    GhostMeshComponent->bDisallowNanite = bDisableNanite;
    GhostMeshComponent->MarkRenderStateDirty();

    // 루트가 곧 메시 컴포넌트라 상대 트랜스폼을 그대로 얹으면 된다.
    // 액터 트랜스폼(위치·회전·크기)과 곱해져 최종 자리가 나온다.
    GhostMeshComponent->SetRelativeTransform(InRelativeTransform);

    BaseLocation = GetActorLocation();

    // 눈으로는 "안 보인다" 하나로 뭉뚱그려지는 실패가 여럿이라, 실제로
    // 어디에 얼마나 큰 것이 섰는지 값으로 남긴다.
    SF_LOG(Log,
        "[SFTutorialGhost] Mesh applied | Actor: %s | Mesh world scale: %s | Bounds radius: %.1f | Nanite disabled: %s",
        *GetActorLocation().ToString(),
        *GhostMeshComponent->GetComponentScale().ToString(),
        GhostMeshComponent->Bounds.SphereRadius,
        bDisableNanite ? TEXT("yes") : TEXT("no"));

    if (!IsValid(GhostMaterial))
    {
        return;
    }

    // 슬롯 수는 메시마다 다르다. 하나만 덮으면 나머지가 원본 색으로 남아
    // 반쯤 실제 건물처럼 보인다.
    const int32 MaterialCount = GhostMeshComponent->GetNumMaterials();

    for (int32 Index = 0; Index < MaterialCount; ++Index)
    {
        GhostMeshComponent->SetMaterial(Index, GhostMaterial);
    }
}

void ASFTutorialGhostActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (BobAmplitude <= 0.f)
    {
        return;
    }

    ElapsedTime += DeltaSeconds;

    FVector NewLocation = BaseLocation;
    NewLocation.Z += FMath::Sin(ElapsedTime * BobSpeed) * BobAmplitude;

    SetActorLocation(NewLocation);
}
