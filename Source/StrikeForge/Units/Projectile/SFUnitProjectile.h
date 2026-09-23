// SFUnitProjectile.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SFUnitProjectile.generated.h"

class AUnitBase;
class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UPrimitiveComponent;

UCLASS()
class STRIKEFORGE_API ASFUnitProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASFUnitProjectile();

	/**
	 * 생성된 발사체에 공격자와 대상을 전달한다.
	 * 실제 이동 및 충돌 처리는 SFUnitProjectile이 담당한다.
	 */
	void InitializeProjectile(AUnitBase* InSourceUnit, AActor* InTargetActor, float InDamageRatio, float InProjectileSpeed);

private:
    UFUNCTION()
    void HandleProjectileOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USphereComponent> CollisionComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> ProjectileMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

    UPROPERTY()
    TWeakObjectPtr<AUnitBase> SourceUnit;

    UPROPERTY()
    TWeakObjectPtr<AActor> TargetActor;

    float DamageRatio = 1.f;

};
