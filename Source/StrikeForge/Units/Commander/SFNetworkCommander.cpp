#include "SFCommander.h"
#include "Net/UnrealNetwork.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "SFCommanderAnim.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Game/Subsystem/SFFogOfWarSubsystem.h"

void ASFCommander::MulticastSkillStarted_Implementation(int32 SkillTier)
{
    if(HasAuthority()||bAwaitingRespawn||!SkillAsset.IsValidIndex(SkillTier-1))return;
    if(auto* Anim=Cast<USFCommanderAnim>(GetMesh()->GetAnimInstance()))
    {
        Anim->OnSkillActiveDelegate.Unbind(); // Client animation notifies never apply gameplay effects.
        Anim->bSkillFloating=false;
        const float Played=Anim->Montage_Play(SkillAsset[SkillTier-1].SkillMontage);
        UE_LOG(LogTemp,Display,TEXT("[PVPPresentation] skill_montage tier=%d played=%.2f"),SkillTier,Played);
    }
}

void ASFCommander::MulticastSkillEffect_Implementation(UNiagaraSystem* Effect,FVector_NetQuantize Location,FVector Scale,bool bAttached)
{
    if(!Effect||GetNetMode()==NM_DedicatedServer)return;
    if(GetNetMode()!=NM_Standalone)
        if(const auto* Fog=USFFogOfWarSubsystem::Get(this);Fog&&!Fog->IsActorVisibleToLocalPlayer(this))return;
    UNiagaraComponent* Particle=bAttached
        ? UNiagaraFunctionLibrary::SpawnSystemAttached(Effect,RootComponent,NAME_None,FVector::ZeroVector,FRotator::ZeroRotator,Scale,EAttachLocation::KeepRelativeOffset,true,ENCPoolMethod::AutoRelease)
        : UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),Effect,Location,FRotator::ZeroRotator,Scale,true,true,ENCPoolMethod::AutoRelease);
    if(Particle&&bAttached)Particle->SetUsingAbsoluteRotation(true);
    UE_LOG(LogTemp,Display,TEXT("[PVPPresentation] skill_effect effect=%s spawned=%d"),*GetNameSafe(Effect),Particle!=nullptr);
}

void ASFCommander::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASFCommander,bMyCommander);DOREPLIFETIME(ASFCommander,bAwaitingRespawn);
    DOREPLIFETIME(ASFCommander,RespawnEndTime);DOREPLIFETIME(ASFCommander,SkillCooldownEnds);
}
ASFInGamePlayerController* ASFCommander::FindOwningPlayerController() const
{
    if(!GetWorld())return nullptr;
    for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)
        if(auto* PC=Cast<ASFInGamePlayerController>(It->Get());PC&&PC->GetMyTeamIndex()==CombatTargetComponent->GetTeamIndex())return PC;
    return nullptr;
}
void ASFCommander::OnRep_PlayerCommander()
{
    if(!bMyCommander)return;
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    if(HasActorBegunPlay())GetWorldTimerManager().SetTimer(LoadPCTimer,this,&ThisClass::LoadPlayerController,.2f,true,0.f);
}
void ASFCommander::OnRep_NetworkRespawn()
{
    if(bAwaitingRespawn)
    {
        if(bClientDeathVisible)return;bClientDeathVisible=true;
        GetCharacterMovement()->StopMovementImmediately();GetCharacterMovement()->DisableMovement();SetActorEnableCollision(false);
        if(DeathAnimation)
        {
            GetMesh()->PlayAnimation(DeathAnimation,false);
            GetWorldTimerManager().SetTimer(DeathAnimationTimer,this,&ThisClass::HideAfterDeathAnimation,DeathAnimation->GetPlayLength(),false);
        }
        else HideAfterDeathAnimation();
    }
    else
    {
        bClientDeathVisible=false;GetWorldTimerManager().ClearTimer(DeathAnimationTimer);
        SetActorHiddenInGame(false);SetActorEnableCollision(true);GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);GetMesh()->InitAnim(true);
    }
}
