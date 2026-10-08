// SFUnitProjectile.cpp


#include "Units/Projectile/SFUnitProjectile.h"

#include "Units/Unit/UnitBase.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

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

	const FVector StartLocation = GetActorLocation();
	const FVector TargetLocation = InTargetActor->GetActorLocation();

	if (MovementType == ESFProjectileMovementType::Ballistic)
	{
		// 돌처럼 포물선으로 이동하는 발사체는 목표를 추적하지 않는다.
		ProjectileMovement->bIsHomingProjectile = false;
		ProjectileMovement->HomingTargetComponent = nullptr;
		ProjectileMovement->HomingAccelerationMagnitude = 0.f;
		ProjectileMovement->ProjectileGravityScale = BallisticGravityScale;

		FVector LaunchVelocity = FVector::ZeroVector;

		const float GravityZ = GetWorld()->GetGravityZ() * BallisticGravityScale;

		const bool bFoundArc = UGameplayStatics::SuggestProjectileVelocity_CustomArc(
				this,
				LaunchVelocity,
				StartLocation,
				TargetLocation,
				GravityZ,
				BallisticArcParam
			);

		if (!bFoundArc || LaunchVelocity.IsNearlyZero())
		{
			// 포물선 계산에 실패하면 발사체가 멈춰 있지 않도록 직선으로 발사한다.
			const FVector Direction = (TargetLocation - StartLocation).GetSafeNormal();

			ProjectileMovement->ProjectileGravityScale = 0.f;
			ProjectileMovement->InitialSpeed = SafeSpeed;
			ProjectileMovement->MaxSpeed = SafeSpeed;
			ProjectileMovement->Velocity = Direction * SafeSpeed;
			return;
		}

		const float CalculatedSpeed = LaunchVelocity.Size();

		ProjectileMovement->InitialSpeed = CalculatedSpeed;
		ProjectileMovement->MaxSpeed = CalculatedSpeed;
		ProjectileMovement->Velocity = LaunchVelocity;
	}
	else
	{
		// 기존 바람총, 석궁, 마법탄의 유도 발사 방식.
		ProjectileMovement->ProjectileGravityScale = 0.f;
		ProjectileMovement->InitialSpeed = SafeSpeed;
		ProjectileMovement->MaxSpeed = SafeSpeed;

		ProjectileMovement->bIsHomingProjectile = true;
		ProjectileMovement->HomingTargetComponent = InTargetActor->GetRootComponent();

		ProjectileMovement->HomingAccelerationMagnitude = SafeSpeed * 10.f;

		const FVector Direction = (TargetLocation - StartLocation).GetSafeNormal();

		ProjectileMovement->Velocity = Direction * SafeSpeed;
	}
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

