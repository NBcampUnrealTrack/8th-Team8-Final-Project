#include "InGame/HUD/Tutorial/SFTutorialGhostActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

ASFTutorialGhostActor::ASFTutorialGhostActor()
{
    // 위아래로 흔들리는 연출 때문에 틱이 필요하다.
    // BobAmplitude 를 0 으로 두면 Tick 첫 줄에서 빠져나간다.
    PrimaryActorTick.bCanEverTick = true;

    GhostMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GhostMesh"));
    SetRootComponent(GhostMeshComponent);

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

void ASFTutorialGhostActor::SetGhostMesh(UStaticMesh* InMesh)
{
    if (!IsValid(GhostMeshComponent) || !IsValid(InMesh))
    {
        return;
    }

    GhostMeshComponent->SetStaticMesh(InMesh);

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
