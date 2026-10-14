// SFUnitProjectile.cpp


#include "Units/Projectile/SFUnitProjectile.h"

#include "Units/Unit/UnitBase.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

ASFUnitProjectile::ASFUnitProjectile()
{
    // Movement is handled by the component; actor tick also verifies target contact.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

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
    InitialLifeSpan = 3.f;
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
    PreviousLocation = GetActorLocation();
    AddTickPrerequisiteComponent(ProjectileMovement);

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

void ASFUnitProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 타깃 액터가 이미 제거됐다면 발사체도 제거한다.
	AActor* ValidTarget = TargetActor.Get();

	if (!IsValid(ValidTarget))
	{
		Destroy();
		return;
	}

	// 유닛이 사망 상태가 됐다면 시체에 도달하기를 기다리지 않고 제거한다.
	if (const AUnitBase* TargetUnit = Cast<AUnitBase>(ValidTarget))
	{
		if (TargetUnit->GetCombatState() == EUnitCombatState::Dead)
		{
			Destroy();
			return;
		}
	}

	if (bHasImpacted) return;
	// BeginOverlap can fire before InitializeProjectile, or never fire when the
	// target disables overlap events. Sweep only the intended target's real query
	// collision, including the distance travelled this frame, so fast darts and
	// point-blank shots still resolve exactly one impact.
	const FVector CurrentLocation = GetActorLocation();
	const float Radius = CollisionComponent->GetScaledSphereRadius();
	const FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);
	TInlineComponentArray<UPrimitiveComponent*> TargetComponents(ValidTarget);
	for (UPrimitiveComponent* Component : TargetComponents)
	{
		if (!IsValid(Component) || !Component->IsQueryCollisionEnabled()) continue;
		FHitResult Hit;
		FVector ClosestPoint;
		if (Component->SweepComponent(Hit, PreviousLocation, CurrentLocation, FQuat::Identity, Shape))
		{
			ResolveImpact(Hit.ImpactPoint);
			return;
		}
		const float Distance = Component->GetClosestPointOnCollision(CurrentLocation, ClosestPoint);
		if (Distance >= 0.f && Distance <= Radius)
		{
			ResolveImpact(ClosestPoint);
			return;
		}
	}
	PreviousLocation = CurrentLocation;
}

void ASFUnitProjectile::HandleProjectileOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 이미 명중 처리를 완료한 발사체라면 다시 처리하지 않는다.
	if (bHasImpacted)
	{
		return;
	}

	// 공격자나 지나가는 다른 유닛에는 반응하지 않고,
	// 발사할 때 지정한 대상에게만 명중한다.
	if (!IsValid(OtherActor) || OtherActor != TargetActor.Get())
	{
		return;
	}

	// 피격 대상의 충돌 컴포넌트 중심을 기본 명중 위치로 사용한다.
	FVector ImpactLocation = OtherActor->GetActorLocation();

	if (IsValid(OtherComponent))
	{
		ImpactLocation = OtherComponent->Bounds.Origin;
	}

	// Sweep 정보에 실제 충돌 지점이 있으면 해당 위치를 우선 사용한다.
	if (bFromSweep && SweepResult.bBlockingHit)
	{
		ImpactLocation = SweepResult.ImpactPoint;
	}
	ResolveImpact(ImpactLocation);
}

void ASFUnitProjectile::ResolveImpact(const FVector& ImpactLocation)
{
	AActor* OtherActor = TargetActor.Get();
	if (bHasImpacted || !IsValid(OtherActor)) return;
	bHasImpacted = true;

	if (IsValid(ImpactEffect))
	{
		const FVector EffectScale(ImpactEffectScale);

		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			ImpactEffect,
			ImpactLocation,
			FRotator::ZeroRotator,
			EffectScale,
			true,
			true,
			ENCPoolMethod::AutoRelease
		);
	}

	if (AUnitBase* ValidSource = SourceUnit.Get())
	{
		ValidSource->HandleProjectileImpact(OtherActor, DamageRatio);
	}

	Destroy();
}

