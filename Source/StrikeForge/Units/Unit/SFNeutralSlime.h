#pragma once
#include "CoreMinimal.h"
#include "Units/Unit/UnitBase.h"
#include "SFNeutralSlime.generated.h"

class ASFNeutralSlimeCamp;
class UAnimationAsset;

UCLASS()
class STRIKEFORGE_API ASFNeutralSlime : public AUnitBase
{
    GENERATED_BODY()
public:
    ASFNeutralSlime();
    virtual FSFSelectableInfo GetSelectableInfo_Implementation() override;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Slime") bool bLarge = false;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Slime") TObjectPtr<ASFNeutralSlimeCamp> Camp;
    UFUNCTION(BlueprintPure) bool IsProvokedByTeam(int32 Team) const { return ProvokedTeams.Contains(Team); }
    void Provoke(AActor* Aggressor);
    virtual void OnConstruction(const FTransform& Transform) override;
protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
private:
    UFUNCTION() void OnDamaged(UCombatTargetComponent* Target, float Damage, AActor* InstigatorActor);
    UFUNCTION() void OnKilled(UCombatTargetComponent* Target);
    UPROPERTY() TObjectPtr<UAnimationAsset> IdleAnimation;
    UPROPERTY() TObjectPtr<UAnimationAsset> WalkAnimation;
    UPROPERTY() TObjectPtr<UAnimationAsset> AttackAnimation;
    UPROPERTY() TObjectPtr<UAnimationAsset> DieAnimation;
    UPROPERTY() TObjectPtr<UAnimationAsset> PlayingAnimation;
    TSet<int32> ProvokedTeams;
    TWeakObjectPtr<AActor> Aggressor;
    FVector Home = FVector::ZeroVector;
    int32 LastDamagingTeam = INDEX_NONE;
    bool bRewardPaid = false;
    void PlayStateAnimation(UAnimationAsset* Animation, bool bLoop);
};

/** Authored camp marker; members are placed in the level for editor visibility. */
UCLASS()
class STRIKEFORGE_API ASFNeutralSlimeCamp : public AActor
{
    GENERATED_BODY()
public:
    ASFNeutralSlimeCamp();
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Slime") TArray<TObjectPtr<ASFNeutralSlime>> Members;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Slime") bool bGrantClearReward = true;
    UFUNCTION(BlueprintPure) bool IsCleared() const { return bCleared; }
    void Provoke(AActor* Aggressor);
    void MemberKilled(ASFNeutralSlime* Member, int32 Team);
private:
    TSet<TWeakObjectPtr<ASFNeutralSlime>> KilledMembers;
    bool bCleared = false;
};
