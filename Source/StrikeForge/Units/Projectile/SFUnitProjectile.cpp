// SFUnitProjectile.cpp


#include "Units/Projectile/SFUnitProjectile.h"

#include "Units/Unit/UnitBase.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

ASFUnitProjectile::ASFUnitProjectile()
{
    // 이동은 ProjectileMovementComponent가 담당하므로
    // 액터 자체 Tick은 필요하지 않다.
    PrimaryActorTick.bCanEverTick = false;

    CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));

    SetRootComponent(CollisionComponent);

    CollisionComponent->InitSphereRadius(10.f);
    CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

    CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);

    CollisionComponent->SetCollisionResponseToAllChannels(ECR_Overlap);

    CollisionComponent->SetGenerateOverlapEvents(true);

    CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &ASFUnitProjectile::HandleProjectileOverlap);

    ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));

    ProjectileMesh->SetupAttachment(CollisionComponent);
    ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));

    ProjectileMovement->UpdatedComponent = CollisionComponent;
    ProjectileMovement->InitialSpeed = 1500.f;
    ProjectileMovement->MaxSpeed = 1500.f;
    ProjectileMovement->ProjectileGravityScale = 0.f;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->bShouldBounce = false;

    // 발사체가 영원히 남는 상황을 막는다.
    InitialLifeSpan = 5.f;
}

void ASFUnitProjectile::InitializeProjectile(AUnitBase* InSourceUnit, AActor* InTargetActor, float InDamageRatio, float InProjectileSpeed)
{
    if (!IsValid(InSourceUnit) || !IsValid(InTargetActor) || !IsValid(ProjectileMovement))
    {
        Destroy();
        return;
    }

    SourceUnit = InSourceUnit;
    TargetActor = InTargetActor;
    DamageRatio = FMath::Max(0.f, InDamageRatio);

    const float SafeSpeed = FMath::Max(1.f, InProjectileSpeed);

    ProjectileMovement->InitialSpeed = SafeSpeed;
    ProjectileMovement->MaxSpeed = SafeSpeed;

    // 움직이는 적도 따라가도록 유도 발사체로 설정한다.
    ProjectileMovement->bIsHomingProjectile = true;
    ProjectileMovement->HomingTargetComponent = InTargetActor->GetRootComponent();

    // 이 값이 너무 낮으면 빠른 발사체가 목표 방향으로 회전하지 못한다.
    ProjectileMovement->HomingAccelerationMagnitude = SafeSpeed * 10.f;

    const FVector Direction = (InTargetActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();

    ProjectileMovement->Velocity = Direction * SafeSpeed;
}

void ASFUnitProjectile::HandleProjectileOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // 공격자나 지나가는 다른 유닛에는 반응하지 않고,
    // 발사할 때 지정한 대상에게만 명중한다.
    if (!IsValid(OtherActor) || OtherActor != TargetActor.Get())
    {
        return;
    }

    if (AUnitBase* ValidSource = SourceUnit.Get())
    {
        ValidSource->HandleProjectileImpact(OtherActor, DamageRatio);
    }

    Destroy();
}

